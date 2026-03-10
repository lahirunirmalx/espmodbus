/**
 * Modbus PSU driver — implementation. Uses node + stream set at init.
 */

#include "modbus_psu.h"
#include <ModbusMaster.h>
#include <Arduino.h>

static ModbusMaster *s_node = nullptr;
static Stream *s_stream = nullptr;
static const uint8_t SLAVE_CH1 = 1;
static const uint8_t SLAVE_CH2 = 2;

static char s_meaning_buf[64];
static char s_interpret_buf[32];

void modbus_psu_init(void *node, void *stream) {
  s_node = (ModbusMaster *)node;
  s_stream = (Stream *)stream;
}

uint8_t modbus_psu_slave_id(int ch) {
  return (ch == 2) ? SLAVE_CH2 : SLAVE_CH1;
}

bool modbus_psu_read_u16(uint8_t slave_id, uint16_t reg, uint16_t *out) {
  if (!s_node || !s_stream || !out)
    return false;
  s_node->begin(slave_id, *s_stream);
  uint8_t rc = s_node->readHoldingRegisters(reg, 1);
  if (rc == s_node->ku8MBSuccess) {
    *out = s_node->getResponseBuffer(0);
    s_node->clearResponseBuffer();
    return true;
  }
  return false;
}

bool modbus_psu_write_u16(uint8_t slave_id, uint16_t reg, uint16_t val) {
  if (!s_node || !s_stream)
    return false;
  s_node->begin(slave_id, *s_stream);
  uint8_t rc = s_node->writeSingleRegister(reg, val);
  return rc == s_node->ku8MBSuccess;
}

const char *modbus_psu_meaning(uint16_t reg) {
  s_meaning_buf[0] = '\0';
  switch (reg) {
    case REG_SET_VOLT:   strcpy(s_meaning_buf, "Vset (V*100)"); break;
    case REG_SET_CURR:   strcpy(s_meaning_buf, "Iset (A*1000)"); break;
    case REG_OUT_VOLT:   strcpy(s_meaning_buf, "Vout (V*100)"); break;
    case REG_OUT_CURR:   strcpy(s_meaning_buf, "Iout (A*1000)"); break;
    case REG_OUT_POWER:  strcpy(s_meaning_buf, "Pout (W*100)"); break;
    case REG_OUT_ENABLE: strcpy(s_meaning_buf, "Output Enable (0/1)"); break;
    case REG_MPPT_ENABLE: strcpy(s_meaning_buf, "MPPT Enable (0/1) [exp]"); break;
    default: break;
  }
  return s_meaning_buf;
}

const char *modbus_psu_interpret(uint16_t reg, uint16_t raw) {
  s_interpret_buf[0] = '\0';
  switch (reg) {
    case REG_SET_VOLT:
    case REG_OUT_VOLT:
      snprintf(s_interpret_buf, sizeof(s_interpret_buf), "%.2f V", raw / 100.0f);
      break;
    case REG_SET_CURR:
    case REG_OUT_CURR:
      snprintf(s_interpret_buf, sizeof(s_interpret_buf), "%.3f A", raw / 1000.0f);
      break;
    case REG_OUT_POWER:
      snprintf(s_interpret_buf, sizeof(s_interpret_buf), "%.2f W", raw / 100.0f);
      break;
    case REG_OUT_ENABLE:
    case REG_MPPT_ENABLE:
      strcpy(s_interpret_buf, raw ? "ON" : "OFF");
      break;
    default:
      break;
  }
  return s_interpret_buf;
}
