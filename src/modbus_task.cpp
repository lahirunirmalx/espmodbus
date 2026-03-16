/**
 * Modbus Task — implementation.
 * Runs on FreeRTOS task, processes command queue, updates status cache.
 */

#include "modbus_task.h"
#include "modbus_psu.h"
#include "app_http.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <string.h>

#define MT_QUEUE_SIZE 8
#define MT_STACK_SIZE 4096
#define MT_PRIORITY 1

static QueueHandle_t s_cmd_queue = nullptr;
static SemaphoreHandle_t s_cache_mutex = nullptr;
static TaskHandle_t s_task_handle = nullptr;

static char s_status_json[512];
static volatile bool s_status_changed = false;
static volatile uint32_t s_poll_ms = 400;
static uint16_t s_prev[2][7];
static bool s_prev_valid = false;

static bool raw_diff(uint16_t *a, uint16_t *b) {
  for (int i = 0; i < 7; i++)
    if (a[i] != b[i]) return true;
  return false;
}

static bool read_channel_raw(int ch, uint16_t *raw) {
  uint16_t batch[32];
  uint8_t slave_id = modbus_psu_slave_id(ch);
  if (!modbus_psu_read_registers(slave_id, 0x00, 32, batch)) {
    app_http_stat_modbus_fail();
    return false;
  }
  app_http_stat_modbus_ok();
  raw[0] = batch[0];
  raw[1] = batch[1];
  raw[2] = batch[2];
  raw[3] = batch[3];
  raw[4] = batch[4];
  raw[5] = batch[0x12];
  raw[6] = batch[0x1F];
  return true;
}

static void build_status_json(uint16_t cur[2][7]) {
  char buf[512];
  int n = snprintf(buf, sizeof(buf),
    "{\"ch1\":{\"ok\":true,\"ch\":1,\"setV\":%.2f,\"setA\":%.3f,\"outV\":%.2f,\"outA\":%.3f,\"outP\":%.2f,\"outputOn\":%s,\"mppt\":%s},"
    "\"ch2\":{\"ok\":true,\"ch\":2,\"setV\":%.2f,\"setA\":%.3f,\"outV\":%.2f,\"outA\":%.3f,\"outP\":%.2f,\"outputOn\":%s,\"mppt\":%s}}",
    cur[0][0] / 100.0f, cur[0][1] / 1000.0f, cur[0][2] / 100.0f, cur[0][3] / 1000.0f, cur[0][4] / 100.0f,
    cur[0][5] ? "true" : "false", cur[0][6] ? "true" : "false",
    cur[1][0] / 100.0f, cur[1][1] / 1000.0f, cur[1][2] / 100.0f, cur[1][3] / 1000.0f, cur[1][4] / 100.0f,
    cur[1][5] ? "true" : "false", cur[1][6] ? "true" : "false"
  );
  (void)n;

  if (xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    strncpy(s_status_json, buf, sizeof(s_status_json) - 1);
    s_status_json[sizeof(s_status_json) - 1] = '\0';
    s_status_changed = true;
    xSemaphoreGive(s_cache_mutex);
  }
}

static void do_poll(void) {
  uint16_t cur[2][7];
  memset(cur, 0, sizeof(cur));
  read_channel_raw(1, cur[0]);
  read_channel_raw(2, cur[1]);

  bool changed = !s_prev_valid || raw_diff(cur[0], s_prev[0]) || raw_diff(cur[1], s_prev[1]);
  if (changed) {
    memcpy(s_prev, cur, sizeof(s_prev));
    s_prev_valid = true;
    build_status_json(cur);
  }
}

static void handle_write(uint8_t ch, uint16_t reg, uint16_t val) {
  uint8_t slave_id = modbus_psu_slave_id(ch);
  bool ok = modbus_psu_write_u16(slave_id, reg, val);
  if (ok)
    app_http_stat_modbus_ok();
  else
    app_http_stat_modbus_fail();

  s_prev_valid = false;
  do_poll();
}

static void handle_link(void) {
  uint16_t raw[7];
  uint8_t sid1 = modbus_psu_slave_id(1);
  if (!modbus_psu_read_registers(sid1, 0x00, 32, raw)) {
    app_http_stat_modbus_fail();
    return;
  }
  app_http_stat_modbus_ok();

  uint8_t sid2 = modbus_psu_slave_id(2);
  modbus_psu_write_u16(sid2, REG_SET_VOLT, raw[0]);
  modbus_psu_write_u16(sid2, REG_SET_CURR, raw[1]);
  modbus_psu_write_u16(sid2, REG_OUT_ENABLE, raw[0x12]);
  modbus_psu_write_u16(sid2, REG_MPPT_ENABLE, raw[0x1F]);

  s_prev_valid = false;
  do_poll();
}

static void modbus_task_fn(void *arg) {
  (void)arg;
  TickType_t last_poll = xTaskGetTickCount();

  for (;;) {
    mt_cmd_t cmd;
    if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
      switch (cmd.type) {
        case MT_CMD_WRITE_REG:
          handle_write(cmd.ch, cmd.reg, cmd.val);
          break;
        case MT_CMD_LINK:
          handle_link();
          break;
        case MT_CMD_READ_CHANNEL:
          do_poll();
          break;
      }
      last_poll = xTaskGetTickCount();
    } else {
      TickType_t now = xTaskGetTickCount();
      if ((now - last_poll) >= pdMS_TO_TICKS(s_poll_ms)) {
        do_poll();
        last_poll = now;
      }
    }
  }
}

void modbus_task_init(void) {
  s_cmd_queue = xQueueCreate(MT_QUEUE_SIZE, sizeof(mt_cmd_t));
  s_cache_mutex = xSemaphoreCreateMutex();
  s_status_json[0] = '\0';

  xTaskCreate(modbus_task_fn, "modbus", MT_STACK_SIZE, nullptr, MT_PRIORITY, &s_task_handle);
}

bool modbus_task_write(uint8_t ch, uint16_t reg, uint16_t val) {
  mt_cmd_t cmd = {MT_CMD_WRITE_REG, ch, reg, val};
  return xQueueSendToFront(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE;
}

bool modbus_task_link(void) {
  mt_cmd_t cmd = {MT_CMD_LINK, 0, 0, 0};
  return xQueueSendToFront(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE;
}

void modbus_task_get_status(char *buf, size_t buflen) {
  if (xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    strncpy(buf, s_status_json, buflen - 1);
    buf[buflen - 1] = '\0';
    xSemaphoreGive(s_cache_mutex);
  } else {
    buf[0] = '\0';
  }
}

bool modbus_task_status_changed(void) {
  if (xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    bool changed = s_status_changed;
    s_status_changed = false;
    xSemaphoreGive(s_cache_mutex);
    return changed;
  }
  return false;
}

void modbus_task_set_poll_ms(uint32_t ms) {
  if (ms < 50) ms = 50;
  if (ms > 5000) ms = 5000;
  s_poll_ms = ms;
}

uint32_t modbus_task_get_poll_ms(void) {
  return s_poll_ms;
}
