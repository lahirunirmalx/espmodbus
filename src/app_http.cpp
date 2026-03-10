/**
 * App HTTP — implementation. Handlers use modbus_psu API only.
 */

#include "app_http.h"
#include "modbus_psu.h"
#include <Arduino.h>

static WebServer *s_server = nullptr;

static void send_json(const String &s) {
  if (s_server)
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

void app_http_init(WebServer *server) {
  s_server = server;
}

void app_http_handle_index(const char *index_html) {
  if (s_server && index_html)
    s_server->send_P(200, "text/html", index_html);
}

void app_http_handle_status(void) {
  if (!s_server)
    return;
  int ch = channel_from_uri(s_server->uri(), 1);
  uint8_t slave_id = modbus_psu_slave_id(ch);

  uint16_t sV = 0, sA = 0, oV = 0, oA = 0, oP = 0, outE = 0, mppt = 0;
  bool ok = true;
  ok &= modbus_psu_read_u16(slave_id, REG_SET_VOLT, &sV);
  ok &= modbus_psu_read_u16(slave_id, REG_SET_CURR, &sA);
  ok &= modbus_psu_read_u16(slave_id, REG_OUT_VOLT, &oV);
  ok &= modbus_psu_read_u16(slave_id, REG_OUT_CURR, &oA);
  ok &= modbus_psu_read_u16(slave_id, REG_OUT_POWER, &oP);
  bool mppt_ok = modbus_psu_read_u16(slave_id, REG_MPPT_ENABLE, &mppt);

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

  if (modbus_psu_read_u16(slave_id, REG_OUT_ENABLE, &outE)) {
    json += ",\"outputOn\":";
    json += (outE == 1 ? "true" : "false");
  } else {
    json += ",\"outputOn\":null";
  }
  if (mppt_ok) {
    json += ",\"mppt\":";
    json += (mppt ? "true" : "false");
  } else {
    json += ",\"mppt\":null";
  }
  json += "}";
  send_json(json);
}

void app_http_handle_write(void) {
  if (!s_server)
    return;
  int ch = channel_from_uri(s_server->uri(), 1);
  uint8_t slave_id = modbus_psu_slave_id(ch);

  if (!s_server->hasArg("reg") || !s_server->hasArg("val")) {
    s_server->send(400, "application/json",
                   "{\"ok\":false,\"msg\":\"reg & val required\"}");
    return;
  }
  uint16_t reg = (uint16_t)strtoul(s_server->arg("reg").c_str(), nullptr, 0);
  uint16_t val = (uint16_t)strtoul(s_server->arg("val").c_str(), nullptr, 0);
  bool ok = modbus_psu_write_u16(slave_id, reg, val);
  s_server->send(ok ? 200 : 500, "application/json",
                 String("{\"ok\":") + (ok ? "true}" : "false,\"msg\":\"write failed\"}"));
}

static bool write_channel_settings(uint8_t slave_id, uint16_t sV, uint16_t sA, uint16_t outE, uint16_t mppt) {
  bool ok = true;
  ok &= modbus_psu_write_u16(slave_id, REG_SET_VOLT, sV);
  ok &= modbus_psu_write_u16(slave_id, REG_SET_CURR, sA);
  modbus_psu_write_u16(slave_id, REG_OUT_ENABLE, outE);
  modbus_psu_write_u16(slave_id, REG_MPPT_ENABLE, mppt);
  return ok;
}

void app_http_handle_link(void) {
  if (!s_server)
    return;
  uint16_t sV = 0, sA = 0, outE = 0, mppt = 0;
  uint8_t sid1 = modbus_psu_slave_id(1);
  bool ok = true;
  ok &= modbus_psu_read_u16(sid1, REG_SET_VOLT, &sV);
  ok &= modbus_psu_read_u16(sid1, REG_SET_CURR, &sA);
  modbus_psu_read_u16(sid1, REG_OUT_ENABLE, &outE);
  modbus_psu_read_u16(sid1, REG_MPPT_ENABLE, &mppt);

  if (!ok) {
    s_server->send(500, "application/json",
                   "{\"ok\":false,\"msg\":\"failed to read Ch1\"}");
    return;
  }

  bool write_ok = write_channel_settings(modbus_psu_slave_id(2), sV, sA, outE, mppt);

  s_server->send(write_ok ? 200 : 500, "application/json",
                 String("{\"ok\":") + (write_ok ? "true}" : "false,\"msg\":\"write to Ch2 failed\"}"));
}

void app_http_handle_scan(void) {
  if (!s_server)
    return;
  int ch = channel_from_uri(s_server->uri(), 1);
  uint8_t slave_id = modbus_psu_slave_id(ch);

  uint16_t start = 0x0000, end = 0x0080;
  if (s_server->hasArg("start")) {
    if (!parse_num(s_server->arg("start"), &start)) {
      s_server->send(400, "application/json", "{\"ok\":false,\"msg\":\"bad start\"}");
      return;
    }
  }
  if (s_server->hasArg("end")) {
    if (!parse_num(s_server->arg("end"), &end)) {
      s_server->send(400, "application/json", "{\"ok\":false,\"msg\":\"bad end\"}");
      return;
    }
  }
  if (end < start || (end - start) > 256) {
    s_server->send(400, "application/json", "{\"ok\":false,\"msg\":\"range too large\"}");
    return;
  }

  String json = "{\"ok\":true,\"ch\":";
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
    delay(5);
  }
  json += "]}";
  send_json(json);
}

void app_http_not_found(void) {
  if (s_server)
    s_server->send(404, "text/plain", "Not found");
}
