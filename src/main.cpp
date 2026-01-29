#include <Arduino.h>
#include <ModbusMaster.h>
#include <WebServer.h>
#include <WiFi.h>

// ===== HTML Content (PROGMEM)
#include "index_html.h"

// ===== WiFi
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ===== UART / RS485
#define UART_RX 16
#define UART_TX 17
#define UART_BAUD 115200
#define RS485_DE_RE_PIN 4
#define USE_RS485_DIR false

// ===== Modbus Slave Addresses
#define MODBUS_SLAVE_CH1 1
#define MODBUS_SLAVE_CH2 2

// Known regs (stable)
#define REG_SET_VOLT 0x0000   // V*100
#define REG_SET_CURR 0x0001   // A*1000
#define REG_OUT_VOLT 0x0002   // V*100
#define REG_OUT_CURR 0x0003   // A*1000
#define REG_OUT_POWER 0x0004  // W*100
#define REG_OUT_ENABLE 0x0012 // 0/1

// Community/experimental (may differ by FW)
#define REG_MPPT_ENABLE 0x001F // 0/1 (medium confidence)

ModbusMaster node;
WebServer server(80);

// Link mode: when true, Ch1 settings mirror to Ch2
bool linkMode = false;

// ---------- RS485 Direction ----------
void preTransmission() {
#if USE_RS485_DIR
digitalWrite(RS485_DE_RE_PIN, HIGH);
delayMicroseconds(10);
#endif
}
void postTransmission() {
#if USE_RS485_DIR
 delayMicroseconds(10);
  digitalWrite(RS485_DE_RE_PIN, LOW);
#endif
}

// ---------- Helpers ----------
bool mbReadU16(uint8_t slaveId, uint16_t reg, uint16_t &out) {
  node.begin(slaveId, Serial2);
  uint8_t rc = node.readHoldingRegisters(reg, 1);
  if (rc == node.ku8MBSuccess) {
    out = node.getResponseBuffer(0);
    node.clearResponseBuffer();
    return true;
  }
  return false;
}

bool mbWriteU16(uint8_t slaveId, uint16_t reg, uint16_t val) {
  node.begin(slaveId, Serial2);
  uint8_t rc = node.writeSingleRegister(reg, val);
  return rc == node.ku8MBSuccess;
}

static bool parseNum(const String &s, uint16_t &out) {
  char *end = nullptr;
  uint32_t v = (s.startsWith("0x") || s.startsWith("0X"))
                   ? strtoul(s.c_str(), &end, 16)
                   : strtoul(s.c_str(), &end, 10);
  if (end == s.c_str())
    return false;
  out = (uint16_t)v;
  return true;
}

uint8_t getSlaveId(int ch) {
  return (ch == 2) ? MODBUS_SLAVE_CH2 : MODBUS_SLAVE_CH1;
}

// ---------- HTTP Handlers ----------
void sendJSON(const String &s) { server.send(200, "application/json", s); }

void handleIndex() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  // Get channel from URL path
  String uri = server.uri();
  int ch = 1;
  if (uri.endsWith("/2")) ch = 2;
  uint8_t slaveId = getSlaveId(ch);

  uint16_t sV = 0, sA = 0, oV = 0, oA = 0, oP = 0, outE = 0, mppt = 0;
  bool ok = true;
  ok &= mbReadU16(slaveId, REG_SET_VOLT, sV);
  ok &= mbReadU16(slaveId, REG_SET_CURR, sA);
  ok &= mbReadU16(slaveId, REG_OUT_VOLT, oV);
  ok &= mbReadU16(slaveId, REG_OUT_CURR, oA);
  ok &= mbReadU16(slaveId, REG_OUT_POWER, oP);
  bool mpptOk = mbReadU16(slaveId, REG_MPPT_ENABLE, mppt);

  String json = "{";
  json += "\"ok\":";
  json += ok ? "true" : "false";
  json += ",\"ch\":";
  json += String(ch);
  json += ",\"setV\":";
  json += String(sV / 100.0f, 2);
  json += ",\"setA\":";
  json += String(sA / 1000.0f, 3);
  json += ",\"outV\":";
  json += String(oV / 100.0f, 2);
  json += ",\"outA\":";
  json += String(oA / 1000.0f, 3);
  json += ",\"outP\":";
  json += String(oP / 100.0f, 2);

  if (mbReadU16(slaveId, REG_OUT_ENABLE, outE)) {
    json += ",\"outputOn\":";
    json += (outE == 1 ? "true" : "false");
  } else {
    json += ",\"outputOn\":null";
  }

  if (mpptOk) {
    json += ",\"mppt\":";
    json += (mppt ? "true" : "false");
  } else {
    json += ",\"mppt\":null";
  }
  json += "}";
  sendJSON(json);
}

void handleWrite() {
  // Get channel from URL path
  String uri = server.uri();
  int ch = 1;
  if (uri.indexOf("/2") > 0) ch = 2;
  uint8_t slaveId = getSlaveId(ch);

  if (!server.hasArg("reg") || !server.hasArg("val")) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"reg & val required\"}");
    return;
  }
  uint16_t reg = (uint16_t)strtoul(server.arg("reg").c_str(), nullptr, 0);
  uint16_t val = (uint16_t)strtoul(server.arg("val").c_str(), nullptr, 0);
  bool ok = mbWriteU16(slaveId, reg, val);
  server.send(ok ? 200 : 500, "application/json",
              String("{\"ok\":") +
                  (ok ? "true}" : "false,\"msg\":\"write failed\"}"));
}

// Link mode: copy Ch1 settings to Ch2
void handleLink() {
  uint16_t sV = 0, sA = 0, outE = 0, mppt = 0;
  bool ok = true;

  // Read Ch1 settings
  ok &= mbReadU16(MODBUS_SLAVE_CH1, REG_SET_VOLT, sV);
  ok &= mbReadU16(MODBUS_SLAVE_CH1, REG_SET_CURR, sA);
  mbReadU16(MODBUS_SLAVE_CH1, REG_OUT_ENABLE, outE);
  mbReadU16(MODBUS_SLAVE_CH1, REG_MPPT_ENABLE, mppt);

  if (!ok) {
    server.send(500, "application/json",
                "{\"ok\":false,\"msg\":\"failed to read Ch1\"}");
    return;
  }

  // Write to Ch2
  bool writeOk = true;
  writeOk &= mbWriteU16(MODBUS_SLAVE_CH2, REG_SET_VOLT, sV);
  writeOk &= mbWriteU16(MODBUS_SLAVE_CH2, REG_SET_CURR, sA);
  mbWriteU16(MODBUS_SLAVE_CH2, REG_OUT_ENABLE, outE);
  mbWriteU16(MODBUS_SLAVE_CH2, REG_MPPT_ENABLE, mppt);

  server.send(writeOk ? 200 : 500, "application/json",
              String("{\"ok\":") +
                  (writeOk ? "true}" : "false,\"msg\":\"write to Ch2 failed\"}"));
}

String meaningFor(uint16_t addr) {
  switch (addr) {
  case REG_SET_VOLT:
    return "Vset (V*100)";
  case REG_SET_CURR:
    return "Iset (A*1000)";
  case REG_OUT_VOLT:
    return "Vout (V*100)";
  case REG_OUT_CURR:
    return "Iout (A*1000)";
  case REG_OUT_POWER:
    return "Pout (W*100)";
  case REG_OUT_ENABLE:
    return "Output Enable (0/1)";
  case REG_MPPT_ENABLE:
    return "MPPT Enable (0/1) [exp]";
  default:
    return "";
  }
}

String interpretFor(uint16_t addr, uint16_t raw) {
  switch (addr) {
  case REG_SET_VOLT:
  case REG_OUT_VOLT:
    return String(raw / 100.0f, 2) + " V";
  case REG_SET_CURR:
  case REG_OUT_CURR:
    return String(raw / 1000.0f, 3) + " A";
  case REG_OUT_POWER:
    return String(raw / 100.0f, 2) + " W";
  case REG_OUT_ENABLE:
    return raw ? "ON" : "OFF";
  case REG_MPPT_ENABLE:
    return raw ? "ON" : "OFF";
  default:
    return "";
  }
}

void handleScan() {
  // Get channel from URL path
  String uri = server.uri();
  int ch = 1;
  if (uri.indexOf("/2") > 0) ch = 2;
  uint8_t slaveId = getSlaveId(ch);

  uint16_t start = 0x0000, end = 0x0080;
  if (server.hasArg("start")) {
    if (!parseNum(server.arg("start"), start)) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"msg\":\"bad start\"}");
      return;
    }
  }
  if (server.hasArg("end")) {
    if (!parseNum(server.arg("end"), end)) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"msg\":\"bad end\"}");
      return;
    }
  }
  if (end < start || (end - start) > 256) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"range too large\"}");
    return;
  }

  String json = "{\"ok\":true,\"ch\":";
  json += String(ch);
  json += ",\"rows\":[";
  bool first = true;
  for (uint16_t reg = start; reg <= end; reg++) {
    uint16_t v = 0;
    node.begin(slaveId, Serial2);
    uint8_t rc = node.readHoldingRegisters(reg, 1);
    if (rc == node.ku8MBSuccess) {
      v = node.getResponseBuffer(0);
      node.clearResponseBuffer();
      if (!first)
        json += ",";
      first = false;
      String m = meaningFor(reg);
      String i = interpretFor(reg, v);
      json += "{\"addr\":" + String(reg) + ",\"dec\":" + String(v);
      if (m.length()) {
        json += ",\"meaning\":\"" + m + "\"";
      }
      if (i.length()) {
        json += ",\"interpretation\":\"" + i + "\"";
      }
      json += "}";
    }
    delay(5);
  }
  json += "]}";
  sendJSON(json);
}

void notFound() {
  server.send(404, "text/plain", "Not found");
}

void connectWiFi() {
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
  } else {
    Serial.println("WiFi failed; starting AP 'SK120x-ESP32'...");
    WiFi.softAP("SK120x-ESP32");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
#if USE_RS485_DIR
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);
#endif

  // Initialize with default slave
  node.begin(MODBUS_SLAVE_CH1, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  connectWiFi();

  // Routes
  server.on("/", HTTP_GET, handleIndex);

  // Status endpoints for each channel
  server.on("/api/status/1", HTTP_GET, handleStatus);
  server.on("/api/status/2", HTTP_GET, handleStatus);

  // Write endpoints for each channel
  server.on("/api/write/1", HTTP_POST, handleWrite);
  server.on("/api/write/2", HTTP_POST, handleWrite);

  // Scan endpoints for each channel
  server.on("/api/scan/1", HTTP_GET, handleScan);
  server.on("/api/scan/2", HTTP_GET, handleScan);

  // Link mode endpoint
  server.on("/api/link", HTTP_POST, handleLink);

  server.onNotFound(notFound);
  server.begin();
  Serial.println("HTTP server started - Dual Channel Mode");
  Serial.println("Channel 1: Slave ID " + String(MODBUS_SLAVE_CH1));
  Serial.println("Channel 2: Slave ID " + String(MODBUS_SLAVE_CH2));
}

void loop() { server.handleClient(); }
