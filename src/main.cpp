/**
 * Main — wires HAL, Modbus PSU driver, and HTTP app.
 * No board pins or register addresses here; all in hal_board and modbus_psu.
 */

#include <Arduino.h>
#include <ModbusMaster.h>
#include <WebServer.h>
#include <WiFi.h>

#include "hal/hal_board.h"
#include "modbus_psu.h"
#include "modbus_queue.h"
#include "modbus_task.h"
#include "app_http.h"
#include "index_html.h"

static const char *WIFI_SSID = "Lahiru's iPhone";
static const char *WIFI_PASS = "eh5kbv3fys5h6";

static ModbusMaster node;
static WebServer server(80);

static bool s_wifi_sta_mode = false;

static void connect_wifi(void) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
    s_wifi_sta_mode = true;
  } else {
    Serial.println("WiFi failed; starting AP 'SK120x-ESP32'...");
    WiFi.softAP("SK120x-ESP32");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    s_wifi_sta_mode = false;
  }
}

static void check_wifi(void) {
  static uint32_t last_check = 0;
  if (!s_wifi_sta_mode)
    return;
  if (millis() - last_check < 10000)
    return;
  last_check = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
  }
}

void setup() {
  hal_board_init();

  Stream *modbus_stream = hal_modbus_stream();
  node.begin(modbus_psu_slave_id(1), *modbus_stream);
  hal_modbus_install_rs485_callbacks(&node);
  modbus_psu_init(&node, modbus_stream);
  modbus_queue_init();
  modbus_task_init();

  connect_wifi();

  app_http_init(&server);
  server.on("/", HTTP_GET, []() { app_http_handle_index(INDEX_HTML); });
  server.on("/api/status", HTTP_GET, app_http_handle_status_all);
  server.on("/api/status/1", HTTP_GET, app_http_handle_status);
  server.on("/api/status/2", HTTP_GET, app_http_handle_status);
  server.on("/api/write/1", HTTP_POST, app_http_handle_write);
  server.on("/api/write/2", HTTP_POST, app_http_handle_write);
  server.on("/api/scan/1", HTTP_GET, app_http_handle_scan);
  server.on("/api/scan/2", HTTP_GET, app_http_handle_scan);
  server.on("/api/link", HTTP_POST, app_http_handle_link);
  server.on("/api/health", HTTP_GET, app_http_handle_health);
  server.on("/api/config", HTTP_GET, app_http_handle_config);
  server.on("/api/config", HTTP_POST, app_http_handle_config);
  server.onNotFound(app_http_not_found);
  server.begin();

  app_ws_init();

  Serial.println("HTTP server started - Dual Output 36V/6A");
  Serial.println("Channel 1: Slave ID " + String(modbus_psu_slave_id(1)));
  Serial.println("Channel 2: Slave ID " + String(modbus_psu_slave_id(2)));
}

void loop() {
  check_wifi();
  app_ws_loop();
  server.handleClient();
  app_http_background_poll();
}
