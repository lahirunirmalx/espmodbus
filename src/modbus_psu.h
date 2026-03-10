/**
 * Modbus PSU driver — register definitions and read/write API.
 * Depends on ModbusMaster node and transport; no board or pin knowledge.
 */

#ifndef MODBUS_PSU_H
#define MODBUS_PSU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Channel index (1 or 2). */
#define PSU_CH1  1
#define PSU_CH2  2

/** Known holding registers (stable). */
#define REG_SET_VOLT   0x0000  /* V * 100 */
#define REG_SET_CURR   0x0001  /* A * 1000 */
#define REG_OUT_VOLT   0x0002  /* V * 100 */
#define REG_OUT_CURR   0x0003  /* A * 1000 */
#define REG_OUT_POWER  0x0004  /* W * 100 */
#define REG_OUT_ENABLE 0x0012  /* 0 / 1 */
#define REG_MPPT_ENABLE 0x001F /* 0 / 1 (experimental) */

/**
 * Init driver with the Modbus node and transport stream.
 * node: ModbusMaster instance (caller installs HAL RS485 callbacks on it).
 * stream: Stream* used for Modbus (e.g. from hal_modbus_stream()).
 */
void modbus_psu_init(void *node, void *stream);

/** Map channel to slave ID. ch is 1 or 2. */
uint8_t modbus_psu_slave_id(int ch);

/** Read one holding register. Returns true on success. */
bool modbus_psu_read_u16(uint8_t slave_id, uint16_t reg, uint16_t *out);

/** Write one holding register. Returns true on success. */
bool modbus_psu_write_u16(uint8_t slave_id, uint16_t reg, uint16_t val);

/** Human-readable name for register (for scan UI). Empty string if unknown. */
const char *modbus_psu_meaning(uint16_t reg);

/** Human-readable value for register (e.g. "12.34 V"). Empty if unknown. */
const char *modbus_psu_interpret(uint16_t reg, uint16_t raw);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PSU_H */
