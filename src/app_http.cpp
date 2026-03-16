/**
 * App HTTP — implementation. Handlers use modbus_psu API only.
 * WebSocket server on port 81 pushes status updates when values change.
 */

#include "app_http.h"
#include "modbus_psu.h"
#include "modbus_queue.h"
#include "modbus_task.h"
#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <string.h>

static WebServer *s_server = nullptr;
static WebSocketsServer s_ws(81);

static struct {
  uint32_t modbus_ok;
  uint32_t modbus_fail;
  uint32_t ws_connects;
  uint32_t ws_disconnects;
  uint32_t boot_time;
} s_stats;

void app_http_stat_modbus_ok(void) { s_stats.modbus_ok++; }
void app_http_stat_modbus_fail(void) { s_stats.modbus_fail++; }

/* Skip send if client already disconnected (avoids "Connection reset by peer" log spam). */
static bool client_connected(void) {
  return s_server && s_server->client().connected();
}

static void send_json(const String &s) {
  if (client_connected())
    s_server->send(200, "application/json", s);
}

static int channel_from_uri(const String &uri, int default_ch) {
  if (uri.endsWith("/2")) return 2;
  if (uri.indexOf("/2") > 0) return 2;
  return default_ch;
}

static bool parse_num(const String &s, uint16_t *out) {
  if (!out)
    return false;
  char *end = nullptr;
  unsigned long v = (s.startsWith("0x") || s.startsWith("0X"))
      ? strtoul(s.c_str(), &end, 16)
      : strtoul(s.c_str(), &end, 10);
  if (end == s.c_str())
    return false;
  *out = (uint16_t)v;
  return true;
}

static uint32_t s_poll_ms = 400;
static Preferences s_prefs;

static void load_config(void) {
  s_prefs.begin("psu", true);
  s_poll_ms = s_prefs.getUInt("poll_ms", 400);
  s_prefs.end();
  if (s_poll_ms < 50) s_poll_ms = 50;
  if (s_poll_ms > 5000) s_poll_ms = 5000;
}

static void save_config(void) {
  s_prefs.begin("psu", false);
  s_prefs.putUInt("poll_ms", s_poll_ms);
  s_prefs.end();
}

void app_http_init(WebServer *server) {
  s_server = server;
  s_stats.boot_time = millis();
  load_config();
  modbus_task_set_poll_ms(s_poll_ms);
}

static String s_status_cache;

static void ws_handle_write(uint8_t num, JsonDocument &doc) {
  int ch = doc["ch"] | 1;
  uint16_t reg = doc["reg"] | 0;
  uint16_t val = doc["val"] | 0;

  bool ok = modbus_task_write((uint8_t)ch, reg, val);

  String resp;
  resp.reserve(64);
  resp = "{\"cmd\":\"write\",\"ok\":";
  resp += ok ? "true" : "false";
  resp += "}";
  s_ws.sendTXT(num, resp);
}

static void ws_event(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      s_stats.ws_connects++;
      Serial.printf("[WS] Client %u connected\n", num);
      if (s_status_cache.length() > 0)
        s_ws.sendTXT(num, s_status_cache);
      break;
    case WStype_DISCONNECTED:
      s_stats.ws_disconnects++;
      Serial.printf("[WS] Client %u disconnected\n", num);
      break;
    case WStype_TEXT: {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) break;
      const char *cmd = doc["cmd"];
      if (cmd && strcmp(cmd, "write") == 0) {
        ws_handle_write(num, doc);
      }
      break;
    }
    default:
      break;
  }
}

void app_ws_init(void) {
  s_ws.begin();
  s_ws.onEvent(ws_event);
  Serial.println("WebSocket server started on port 81");
}

void app_ws_loop(void) {
  s_ws.loop();
}

static void ws_broadcast_status(void) {
  if (s_status_cache.length() > 0)
    s_ws.broadcastTXT(s_status_cache);
}


void app_http_background_poll(void) {
  if (modbus_task_status_changed()) {
    char buf[512];
    modbus_task_get_status(buf, sizeof(buf));
    if (buf[0]) {
      s_status_cache = buf;
      ws_broadcast_status();
    }
  }
}

void app_http_invalidate_status_cache(void) {
  s_status_cache = "";
}

void app_http_handle_index(const char *index_html) {
  if (client_connected() && index_html)
    s_server->send_P(200, "text/html", index_html);
}

void app_http_handle_status_all(void) {
  if (!s_server)
    return;
  if (s_status_cache.length() == 0) {
    char buf[512];
    modbus_task_get_status(buf, sizeof(buf));
    if (buf[0])
      s_status_cache = buf;
  }
  if (s_status_cache.length() > 0) {
    send_json(s_status_cache);
  } else {
    s_server->send(503, "application/json", "{\"ok\":false,\"msg\":\"not ready\"}");
  }
}

void app_http_handle_status(void) {
  if (!s_server)
    return;
  app_http_handle_status_all();
}

void app_http_handle_write(void) {
  if (!s_server)
    return;
  int ch = channel_from_uri(s_server->uri(), 1);

  if (!s_server->hasArg("reg") || !s_server->hasArg("val")) {
    if (client_connected())
      s_server->send(400, "application/json",
                     "{\"ok\":false,\"msg\":\"reg & val required\"}");
    return;
  }
  uint16_t reg = (uint16_t)strtoul(s_server->arg("reg").c_str(), nullptr, 0);
  uint16_t val = (uint16_t)strtoul(s_server->arg("val").c_str(), nullptr, 0);

  bool ok = modbus_task_write((uint8_t)ch, reg, val);
  if (client_connected())
    s_server->send(ok ? 200 : 500, "application/json",
                   String("{\"ok\":") + (ok ? "true}" : "false,\"msg\":\"write queued\"}"));
}

void app_http_handle_link(void) {
  if (!s_server)
    return;

  bool ok = modbus_task_link();
  if (client_connected())
    s_server->send(ok ? 200 : 500, "application/json",
                   String("{\"ok\":") + (ok ? "true}" : "false,\"msg\":\"link queued\"}"));
}

void app_http_handle_scan(void) {
  if (!s_server)
    return;

  modbus_queue_clear();

  int ch = channel_from_uri(s_server->uri(), 1);
  uint8_t slave_id = modbus_psu_slave_id(ch);

  uint16_t start = 0x0000, end = 0x0020;
  if (s_server->hasArg("start")) {
    if (!parse_num(s_server->arg("start"), &start)) {
      if (client_connected())
        s_server->send(400, "application/json", "{\"ok\":false,\"msg\":\"bad start\"}");
      return;
    }
  }
  if (s_server->hasArg("end")) {
    if (!parse_num(s_server->arg("end"), &end)) {
      if (client_connected())
        s_server->send(400, "application/json", "{\"ok\":false,\"msg\":\"bad end\"}");
      return;
    }
  }
  if (end < start || (end - start) > 32) {
    if (client_connected())
      s_server->send(400, "application/json", "{\"ok\":false,\"msg\":\"range max 32 regs\"}");
    return;
  }

  String json;
  json.reserve(1024);
  json = "{\"ok\":true,\"ch\":";
  json += String(ch);
  json += ",\"rows\":[";
  bool first = true;
  for (uint16_t reg = start; reg <= end; reg++) {
    uint16_t v = 0;
    if (modbus_psu_read_u16(slave_id, reg, &v)) {
      if (!first)
        json += ",";
      first = false;
      const char *m = modbus_psu_meaning(reg);
      const char *i = modbus_psu_interpret(reg, v);
      json += "{\"addr\":" + String(reg) + ",\"dec\":" + String(v);
      if (m && m[0])
        json += ",\"meaning\":\"" + String(m) + "\"";
      if (i && i[0])
        json += ",\"interpretation\":\"" + String(i) + "\"";
      json += "}";
    }
    delay(0);
  }
  json += "]}";
  send_json(json);
}

void app_http_handle_health(void) {
  if (!s_server)
    return;
  uint32_t uptime_s = (millis() - s_stats.boot_time) / 1000;
  String json;
  json.reserve(200);
  json = "{\"uptime_s\":";
  json += String(uptime_s);
  json += ",\"modbus_ok\":";
  json += String(s_stats.modbus_ok);
  json += ",\"modbus_fail\":";
  json += String(s_stats.modbus_fail);
  json += ",\"ws_connects\":";
  json += String(s_stats.ws_connects);
  json += ",\"ws_disconnects\":";
  json += String(s_stats.ws_disconnects);
  json += ",\"heap_free\":";
  json += String(ESP.getFreeHeap());
  json += "}";
  send_json(json);
}

void app_http_handle_config(void) {
  if (!s_server)
    return;

  if (s_server->hasArg("poll_ms")) {
    uint32_t new_poll = strtoul(s_server->arg("poll_ms").c_str(), nullptr, 10);
    if (new_poll >= 50 && new_poll <= 5000) {
      s_poll_ms = new_poll;
      modbus_task_set_poll_ms(new_poll);
      save_config();
    }
  }

  String json;
  json.reserve(64);
  json = "{\"ok\":true,\"poll_ms\":";
  json += String(modbus_task_get_poll_ms());
  json += "}";
  send_json(json);
}

void app_http_not_found(void) {
  if (client_connected())
    s_server->send(404, "text/plain", "Not found");
}
