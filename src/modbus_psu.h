/**
 * Modbus PSU driver — register definitions and read/write API.
 * Depends on ModbusMaster node and transport; no board or pin knowledge.
 *
 * Register map based on SK120X / DPS / XYS series DC power supplies.
 * 32 registers (0x00-0x1F) read in single batch.
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

/**
 * Holding register map (0x00-0x1F).
 * Based on reverse-engineering SK120X and similar modules.
 */

/* Core setpoint/output registers */
#define REG_SET_VOLT      0x0000  /* Voltage setpoint: V * 100 (e.g., 1200 = 12.00V) */
#define REG_SET_CURR      0x0001  /* Current limit: A * 1000 (e.g., 3000 = 3.000A) */
#define REG_OUT_VOLT      0x0002  /* Output voltage: V * 100 */
#define REG_OUT_CURR      0x0003  /* Output current: A * 1000 */
#define REG_OUT_POWER     0x0004  /* Output power: W * 100 */

/* Energy registers (32-bit, split across 2 regs) */
#define REG_OUT_ENERGY_H  0x0005  /* Output energy high word: Wh */
#define REG_OUT_ENERGY_L  0x0006  /* Output energy low word: Wh */

/* Input/Temperature */
#define REG_IN_VOLT       0x0007  /* Input voltage: V * 100 */
#define REG_UNKNOWN_08    0x0008  /* Unknown register */

/* Time registers */
#define REG_TIME_H        0x0009  /* Runtime high word: seconds */
#define REG_TIME_L        0x000A  /* Runtime low word: seconds */

/* Capacity (Ah) */
#define REG_CAPACITY_H    0x000B  /* Capacity high word: Ah * 1000 */
#define REG_CAPACITY_L    0x000C  /* Capacity low word: Ah * 1000 */

/* Temperature - confirmed at 0x0D on this PSU model */
#define REG_TEMP          0x000D  /* Temperature: °C * 10 (e.g., 381 = 38.1°C) */

/* Protection thresholds */
#define REG_OVP           0x000E  /* Over-voltage protection: V * 100 */
#define REG_OCP           0x000F  /* Over-current protection: A * 1000 */
#define REG_OPP           0x0010  /* Over-power protection: W * 100 (may be at different addr) */

/* Device info */
#define REG_MODEL_H       0x0010  /* Model number high */
#define REG_MODEL_L       0x0011  /* Model number low */
#define REG_FW_VERSION    0x0012  /* Firmware version (if not output enable) */

/* Status flags */
#define REG_OUT_ENABLE    0x0012  /* Output enable: 0/1 (some models) */
#define REG_STATUS        0x0013  /* Status flags: bit0=CV/CC, bit1=OVP, bit2=OCP, etc. */
#define REG_CV_CC         0x0014  /* CV/CC mode: 0=CV, 1=CC */

/* Reserved / Unknown */
#define REG_UNKNOWN_15    0x0015
#define REG_UNKNOWN_16    0x0016
#define REG_UNKNOWN_17    0x0017
#define REG_UNKNOWN_18    0x0018
#define REG_UNKNOWN_19    0x0019
#define REG_UNKNOWN_1A    0x001A
#define REG_UNKNOWN_1B    0x001B
#define REG_UNKNOWN_1C    0x001C
#define REG_UNKNOWN_1D    0x001D
#define REG_UNKNOWN_1E    0x001E

/* MPPT (experimental, device-dependent) */
#define REG_MPPT_ENABLE   0x001F  /* MPPT enable: 0/1 */

/**
 * Init driver with the Modbus node and transport stream.
 */
void modbus_psu_init(void *node, void *stream);

/** Map channel to slave ID. ch is 1 or 2. */
uint8_t modbus_psu_slave_id(int ch);

/** Read one holding register. Returns true on success. */
bool modbus_psu_read_u16(uint8_t slave_id, uint16_t reg, uint16_t *out);

/** Read count consecutive holding registers. Count 1..125. */
bool modbus_psu_read_registers(uint8_t slave_id, uint16_t start_reg, uint8_t count, uint16_t *out);

/** Write one holding register. Returns true on success. */
bool modbus_psu_write_u16(uint8_t slave_id, uint16_t reg, uint16_t val);

/** Human-readable name for register. */
const char *modbus_psu_meaning(uint16_t reg);

/** Human-readable value for register (e.g. "12.34 V"). */
const char *modbus_psu_interpret(uint16_t reg, uint16_t raw);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PSU_H */
