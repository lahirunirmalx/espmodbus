/**
 * Modbus Queue — write-priority FIFO for Modbus commands.
 * Writes are pushed to front (high priority), reads to back.
 * Single consumer in loop() ensures no bus contention.
 */

#ifndef MODBUS_QUEUE_H
#define MODBUS_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MQ_CMD_NONE = 0,
  MQ_CMD_READ_U16,
  MQ_CMD_READ_BATCH,
  MQ_CMD_WRITE_U16
} mq_cmd_type_t;

typedef struct {
  mq_cmd_type_t type;
  uint8_t slave_id;
  uint16_t reg;
  uint16_t count;
  uint16_t value;
  uint16_t *out_buf;
  void (*callback)(bool ok, void *ctx);
  void *ctx;
} mq_cmd_t;

void modbus_queue_init(void);
bool modbus_queue_push_write(uint8_t slave_id, uint16_t reg, uint16_t val,
                             void (*cb)(bool, void *), void *ctx);
bool modbus_queue_push_read(uint8_t slave_id, uint16_t reg, uint16_t *out,
                            void (*cb)(bool, void *), void *ctx);
bool modbus_queue_push_read_batch(uint8_t slave_id, uint16_t start_reg,
                                  uint8_t count, uint16_t *out,
                                  void (*cb)(bool, void *), void *ctx);
void modbus_queue_process(void);
bool modbus_queue_empty(void);
void modbus_queue_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_QUEUE_H */
