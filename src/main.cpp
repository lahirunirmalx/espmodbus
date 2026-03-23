/**
 * Serial-to-Modbus Bridge — Fast streaming with all registers.
 *
 * Protocol:
 *   ESP32 streams:
 *     DATA <ch> setV=<v> setA=<a> outV=<v> outA=<a> outP=<p> outE=<e> inV=<v> temp=<t> \
 *              ovp=<v> ocp=<a> opp=<p> status=<s> cvcc=<m> out=<0|1> mppt=<0|1>
 *
 *   GUI sends:
 *     WRITE <ch> <reg> <val>  -> OK or ERR
 *     LINK                    -> OK or ERR
 *     STOP                    -> stops streaming
 *     START                   -> resumes streaming
 *     POLL <ms>               -> set poll interval (50-1000)
 *     RAW <ch>                -> send raw 32 register dump
 *     VERSION                 -> firmware version
 *
 * Write commands have priority - processed immediately before polling.
 */

#include <Arduino.h>

#define FW_VERSION "1.0.0"
#include <ModbusMaster.h>

#include "hal/hal_board.h"
#include "modbus_psu.h"

static ModbusMaster node;

/* Serial input buffer */
static char s_line[128];
static size_t s_line_len = 0;

/* Streaming state */
static bool s_streaming = true;
static uint32_t s_poll_ms = 100;
static uint32_t s_last_poll = 0;
static int s_current_ch = 1;

/* Cached 32-register buffer for each channel */
static uint16_t s_ch1_buf[32];
static uint16_t s_ch2_buf[32];
static bool s_ch1_valid = false;
static bool s_ch2_valid = false;

static void process_command(const char *line);
static void poll_and_send(int ch);
static void send_status(int ch, uint16_t *buf);
static void send_raw(int ch, uint16_t *buf);

void setup() {
  hal_board_init();

  Stream *modbus_stream = hal_modbus_stream();
  node.begin(modbus_psu_slave_id(1), *modbus_stream);
  hal_modbus_install_rs485_callbacks(&node);
  modbus_psu_init(&node, modbus_stream);

  delay(100);
  Serial.print("!READY v");
  Serial.println(FW_VERSION);
}

void loop() {
  /* Priority 1: Process incoming commands immediately */
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r')
      continue;
    if (c == '\n') {
      s_line[s_line_len] = '\0';
      if (s_line_len > 0) {
        process_command(s_line);
      }
      s_line_len = 0;
    } else if (s_line_len < sizeof(s_line) - 1) {
      s_line[s_line_len++] = c;
    }
  }

  /* Priority 2: Stream data at configured rate */
  if (s_streaming) {
    uint32_t now = millis();
    if (now - s_last_poll >= s_poll_ms) {
      s_last_poll = now;
      poll_and_send(s_current_ch);
      s_current_ch = (s_current_ch == 1) ? 2 : 1;
    }
  }
}

static void poll_and_send(int ch) {
  uint8_t slave = modbus_psu_slave_id(ch);
  uint16_t *buf = (ch == 1) ? s_ch1_buf : s_ch2_buf;
  bool *valid = (ch == 1) ? &s_ch1_valid : &s_ch2_valid;

  if (modbus_psu_read_registers(slave, 0x00, 32, buf)) {
    *valid = true;
    send_status(ch, buf);
  } else {
    *valid = false;
    /* Send zeroed data on error - prevents GUI from showing junk */
    Serial.print("DATA ");
    Serial.print(ch);
    Serial.println(" setV=0 setA=0 outV=0 outA=0 outP=0 outE=0 inV=0 temp=0 time=0 cap=0 ovp=0 ocp=0 opp=0 status=0 cvcc=0 out=0 mppt=0 ERR");
  }
}

/**
 * Send status with all known register values.
 * Format: DATA <ch> <field>=<value> ...
 */
static void send_status(int ch, uint16_t *buf) {
  /* Calculate 32-bit values from register pairs */
  uint32_t outEnergy = ((uint32_t)buf[REG_OUT_ENERGY_H] << 16) | buf[REG_OUT_ENERGY_L];
  uint32_t runtime   = ((uint32_t)buf[REG_TIME_H] << 16) | buf[REG_TIME_L];
  uint32_t capacity  = ((uint32_t)buf[REG_CAPACITY_H] << 16) | buf[REG_CAPACITY_L];

  Serial.print("DATA ");
  Serial.print(ch);

  /* Core readings */
  Serial.print(" setV=");
  Serial.print(buf[REG_SET_VOLT]);

  Serial.print(" setA=");
  Serial.print(buf[REG_SET_CURR]);

  Serial.print(" outV=");
  Serial.print(buf[REG_OUT_VOLT]);

  Serial.print(" outA=");
  Serial.print(buf[REG_OUT_CURR]);

  Serial.print(" outP=");
  Serial.print(buf[REG_OUT_POWER]);

  /* Energy (Wh) - 32-bit */
  Serial.print(" outE=");
  Serial.print(outEnergy);

  /* Input voltage */
  Serial.print(" inV=");
  Serial.print(buf[REG_IN_VOLT]);

  /* Temperature (°C * 10) */
  Serial.print(" temp=");
  Serial.print(buf[REG_TEMP]);

  /* Runtime (seconds) - 32-bit */
  Serial.print(" time=");
  Serial.print(runtime);

  /* Capacity (Ah * 1000) - 32-bit */
  Serial.print(" cap=");
  Serial.print(capacity);

  /* Protection thresholds */
  Serial.print(" ovp=");
  Serial.print(buf[REG_OVP]);

  Serial.print(" ocp=");
  Serial.print(buf[REG_OCP]);

  Serial.print(" opp=");
  Serial.print(buf[REG_OPP]);

  /* Status/mode */
  Serial.print(" status=");
  Serial.print(buf[REG_STATUS]);

  Serial.print(" cvcc=");
  Serial.print(buf[REG_CV_CC]);

  /* Output enable */
  Serial.print(" out=");
  Serial.print(buf[REG_OUT_ENABLE]);

  /* MPPT */
  Serial.print(" mppt=");
  Serial.print(buf[REG_MPPT_ENABLE]);

  Serial.println();
}

/**
 * Send raw register dump for debugging.
 */
static void send_raw(int ch, uint16_t *buf) {
  Serial.print("RAW ");
  Serial.print(ch);
  for (int i = 0; i < 32; i++) {
    Serial.print(' ');
    Serial.print(buf[i]);
  }
  Serial.println();
}

static bool parse_uint16(const char *s, uint16_t *out) {
  if (!s || !*s)
    return false;
  char *end = nullptr;
  unsigned long v = strtoul(s, &end, 0);
  if (end == s || v > 0xFFFF)
    return false;
  *out = (uint16_t)v;
  return true;
}

static void cmd_write(int ch, uint16_t reg, uint16_t val) {
  uint8_t slave = modbus_psu_slave_id(ch);
  if (modbus_psu_write_u16(slave, reg, val)) {
    Serial.println("OK");
    /* Immediately poll to get updated values */
    poll_and_send(ch);
  } else {
    Serial.println("ERR write_failed");
  }
}

static void cmd_link(void) {
  uint8_t s1 = modbus_psu_slave_id(1);
  uint8_t s2 = modbus_psu_slave_id(2);

  if (!s_ch1_valid) {
    if (!modbus_psu_read_registers(s1, 0x00, 32, s_ch1_buf)) {
      Serial.println("ERR read_ch1_failed");
      return;
    }
    s_ch1_valid = true;
  }

  bool ok = true;
  ok = ok && modbus_psu_write_u16(s2, REG_SET_VOLT, s_ch1_buf[REG_SET_VOLT]);
  ok = ok && modbus_psu_write_u16(s2, REG_SET_CURR, s_ch1_buf[REG_SET_CURR]);
  ok = ok && modbus_psu_write_u16(s2, REG_OUT_ENABLE, s_ch1_buf[REG_OUT_ENABLE]);
  ok = ok && modbus_psu_write_u16(s2, REG_MPPT_ENABLE, s_ch1_buf[REG_MPPT_ENABLE]);

  if (ok) {
    Serial.println("OK");
    poll_and_send(2);
  } else {
    Serial.println("ERR write_ch2_failed");
  }
}

static void process_command(const char *line) {
  char cmd[16] = {0};
  char arg1[16] = {0};
  char arg2[16] = {0};
  char arg3[16] = {0};

  int n = sscanf(line, "%15s %15s %15s %15s", cmd, arg1, arg2, arg3);
  if (n < 1)
    return;

  for (char *p = cmd; *p; p++)
    *p = toupper((unsigned char)*p);

  if (strcmp(cmd, "WRITE") == 0) {
    if (n < 4) {
      Serial.println("ERR usage: WRITE <ch> <reg> <val>");
      return;
    }
    int ch = atoi(arg1);
    if (ch != 1 && ch != 2) {
      Serial.println("ERR ch_must_be_1_or_2");
      return;
    }
    uint16_t reg, val;
    if (!parse_uint16(arg2, &reg) || !parse_uint16(arg3, &val)) {
      Serial.println("ERR invalid_reg_or_val");
      return;
    }
    cmd_write(ch, reg, val);
    return;
  }

  if (strcmp(cmd, "LINK") == 0) {
    cmd_link();
    return;
  }

  if (strcmp(cmd, "STOP") == 0) {
    s_streaming = false;
    Serial.println("OK stopped");
    return;
  }

  if (strcmp(cmd, "START") == 0) {
    s_streaming = true;
    Serial.println("OK started");
    return;
  }

  if (strcmp(cmd, "POLL") == 0) {
    if (n < 2) {
      Serial.print("OK poll_ms=");
      Serial.println(s_poll_ms);
      return;
    }
    uint32_t ms = strtoul(arg1, nullptr, 10);
    if (ms < 50) ms = 50;
    if (ms > 1000) ms = 1000;
    s_poll_ms = ms;
    Serial.print("OK poll_ms=");
    Serial.println(s_poll_ms);
    return;
  }

  if (strcmp(cmd, "RAW") == 0) {
    int ch = (n >= 2) ? atoi(arg1) : 1;
    if (ch != 1 && ch != 2) ch = 1;
    uint16_t *buf = (ch == 1) ? s_ch1_buf : s_ch2_buf;
    bool valid = (ch == 1) ? s_ch1_valid : s_ch2_valid;
    if (valid) {
      send_raw(ch, buf);
    } else {
      /* Force a read */
      uint8_t slave = modbus_psu_slave_id(ch);
      if (modbus_psu_read_registers(slave, 0x00, 32, buf)) {
        if (ch == 1) s_ch1_valid = true;
        else s_ch2_valid = true;
        send_raw(ch, buf);
      } else {
        Serial.println("ERR read_failed");
      }
    }
    return;
  }

  if (strcmp(cmd, "PING") == 0) {
    Serial.println("PONG");
    return;
  }

  if (strcmp(cmd, "REGS") == 0) {
    Serial.println("Register map:");
    Serial.println("0x00 setV   V*100");
    Serial.println("0x01 setA   A*1000");
    Serial.println("0x02 outV   V*100");
    Serial.println("0x03 outA   A*1000");
    Serial.println("0x04 outP   W*100");
    Serial.println("0x05 outE_H Wh high");
    Serial.println("0x06 outE_L Wh low");
    Serial.println("0x07 inV    V*100");
    Serial.println("0x0D temp   C*10");
    Serial.println("0x09 time_H sec high");
    Serial.println("0x0A time_L sec low");
    Serial.println("0x0B cap_H  Ah*1000 high");
    Serial.println("0x0C cap_L  Ah*1000 low");
    Serial.println("0x0E OVP    V*100");
    Serial.println("0x0F OCP    A*1000");
    Serial.println("0x10 OPP    W*100");
    Serial.println("0x12 out    0/1");
    Serial.println("0x13 status flags");
    Serial.println("0x14 cvcc   0=CV,1=CC");
    Serial.println("0x1F mppt   0/1");
    return;
  }

  if (strcmp(cmd, "VERSION") == 0) {
    Serial.print("OK ");
    Serial.println(FW_VERSION);
    return;
  }

  Serial.println("ERR unknown_cmd");
}
