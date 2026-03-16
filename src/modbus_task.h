/**
 * Modbus Task — runs Modbus operations on a separate FreeRTOS task.
 * Main loop stays responsive; Modbus doesn't block HTTP/WS.
 */

#ifndef MODBUS_TASK_H
#define MODBUS_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MT_CMD_READ_CHANNEL,
  MT_CMD_WRITE_REG,
  MT_CMD_LINK
} mt_cmd_type_t;

typedef struct {
  mt_cmd_type_t type;
  uint8_t ch;
  uint16_t reg;
  uint16_t val;
} mt_cmd_t;

/** Create Modbus task. Call once from setup() after modbus_psu_init(). */
void modbus_task_init(void);

/** Queue a write command. Returns true if queued. */
bool modbus_task_write(uint8_t ch, uint16_t reg, uint16_t val);

/** Queue a link command (copy Ch1 to Ch2). */
bool modbus_task_link(void);

/** Get current cached status JSON. Thread-safe. */
void modbus_task_get_status(char *buf, size_t buflen);

/** Check if status changed since last call. Resets flag. */
bool modbus_task_status_changed(void);

/** Set poll interval in ms. */
void modbus_task_set_poll_ms(uint32_t ms);

/** Get poll interval. */
uint32_t modbus_task_get_poll_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TASK_H */
