/**
 * HAL Board — implementation. Single place for UART pins and RS485 DE/RE.
 */

#include "hal_board.h"
#include <ModbusMaster.h>
#include <HardwareSerial.h>

#if defined(BOARD_ESP32C3)
/* ESP32-C3 e.g. Super Mini: UART on GPIO20 (RX), GPIO21 (TX); DE/RE on GPIO3 */
static HardwareSerial *s_modbus_uart = &Serial1;
#define UART_RX      20
#define UART_TX      21
#define UART_BAUD    115200
#define RS485_DE_RE_PIN  3
#define USE_RS485_DIR    0
#else
/* ESP32 (e.g. devkit): Serial2, RX=16, TX=17 */
static HardwareSerial *s_modbus_uart = &Serial2;
#define UART_RX      16
#define UART_TX      17
#define UART_BAUD    115200
#define RS485_DE_RE_PIN  4
#define USE_RS485_DIR    0
#endif

void hal_board_init(void) {
  Serial.begin(115200);
  delay(100);

  s_modbus_uart->begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);

#if USE_RS485_DIR
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);
#endif
}

static void pre_tx(void) {
#if USE_RS485_DIR
  digitalWrite(RS485_DE_RE_PIN, HIGH);
  delayMicroseconds(10);
#endif
}

static void post_tx(void) {
#if USE_RS485_DIR
  delayMicroseconds(10);
  digitalWrite(RS485_DE_RE_PIN, LOW);
#endif
}

void hal_modbus_install_rs485_callbacks(void *modbus_node) {
  ModbusMaster *node = (ModbusMaster *)modbus_node;
  node->preTransmission(pre_tx);
  node->postTransmission(post_tx);
}

Stream *hal_modbus_stream(void) {
  return s_modbus_uart;
}
