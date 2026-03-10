/**
 * HAL Board — hardware abstraction for UART/RS485 and board-specific pins.
 * All board selection (#if BOARD_ESP32C3 / else) lives here.
 * Application and Modbus layers do not include Arduino or pin numbers.
 */

#ifndef HAL_BOARD_H
#define HAL_BOARD_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Call once at startup: Serial debug, Modbus UART, RS485 DE/RE pin. */
void hal_board_init(void);

/**
 * Install RS485 direction callbacks on the Modbus node.
 * Pass pointer to ModbusMaster; HAL sets preTransmission/postTransmission.
 */
void hal_modbus_install_rs485_callbacks(void *modbus_node);

/** Stream used for Modbus (so driver layer can begin(slaveId, stream)). */
Stream *hal_modbus_stream(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BOARD_H */
