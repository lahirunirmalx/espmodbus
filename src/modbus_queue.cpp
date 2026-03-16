/**
 * Modbus Queue — implementation.
 * Ring buffer with writes inserted at front (priority), reads at back.
 * process() executes one command per call to avoid blocking.
 */

#include "modbus_queue.h"
#include "modbus_psu.h"
#include <Arduino.h>
#include <string.h>

#define MQ_SIZE 16

static mq_cmd_t s_queue[MQ_SIZE];
static volatile uint8_t s_head;
static volatile uint8_t s_tail;
static volatile uint8_t s_count;

void modbus_queue_init(void) {
  memset(s_queue, 0, sizeof(s_queue));
  s_head = 0;
  s_tail = 0;
  s_count = 0;
}

static bool queue_full(void) {
  return s_count >= MQ_SIZE;
}

bool modbus_queue_empty(void) {
  return s_count == 0;
}

static bool push_back(const mq_cmd_t *cmd) {
  if (queue_full())
    return false;
  s_queue[s_tail] = *cmd;
  s_tail = (s_tail + 1) % MQ_SIZE;
  s_count++;
  return true;
}

static bool push_front(const mq_cmd_t *cmd) {
  if (queue_full())
    return false;
  s_head = (s_head + MQ_SIZE - 1) % MQ_SIZE;
  s_queue[s_head] = *cmd;
  s_count++;
  return true;
}

static bool pop_front(mq_cmd_t *out) {
  if (modbus_queue_empty())
    return false;
  *out = s_queue[s_head];
  s_head = (s_head + 1) % MQ_SIZE;
  s_count--;
  return true;
}

bool modbus_queue_push_write(uint8_t slave_id, uint16_t reg, uint16_t val,
                             void (*cb)(bool, void *), void *ctx) {
  mq_cmd_t cmd = {MQ_CMD_WRITE_U16, slave_id, reg, 1, val, nullptr, cb, ctx};
  return push_front(&cmd);
}

bool modbus_queue_push_read(uint8_t slave_id, uint16_t reg, uint16_t *out,
                            void (*cb)(bool, void *), void *ctx) {
  mq_cmd_t cmd = {MQ_CMD_READ_U16, slave_id, reg, 1, 0, out, cb, ctx};
  return push_back(&cmd);
}

bool modbus_queue_push_read_batch(uint8_t slave_id, uint16_t start_reg,
                                  uint8_t count, uint16_t *out,
                                  void (*cb)(bool, void *), void *ctx) {
  mq_cmd_t cmd = {MQ_CMD_READ_BATCH, slave_id, start_reg, count, 0, out, cb, ctx};
  return push_back(&cmd);
}

void modbus_queue_process(void) {
  mq_cmd_t cmd;
  if (!pop_front(&cmd))
    return;

  bool ok = false;
  switch (cmd.type) {
    case MQ_CMD_WRITE_U16:
      ok = modbus_psu_write_u16(cmd.slave_id, cmd.reg, cmd.value);
      break;
    case MQ_CMD_READ_U16:
      ok = modbus_psu_read_u16(cmd.slave_id, cmd.reg, cmd.out_buf);
      break;
    case MQ_CMD_READ_BATCH:
      ok = modbus_psu_read_registers(cmd.slave_id, cmd.reg, (uint8_t)cmd.count, cmd.out_buf);
      break;
    default:
      break;
  }

  if (cmd.callback)
    cmd.callback(ok, cmd.ctx);
}

void modbus_queue_clear(void) {
  s_head = 0;
  s_tail = 0;
  s_count = 0;
}
