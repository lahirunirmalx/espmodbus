/**
 * App HTTP — REST handlers for status, write, scan, link + WebSocket push.
 * Uses modbus_psu only; no HAL or board knowledge.
 */

#ifndef APP_HTTP_H
#define APP_HTTP_H

#include <WebServer.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Call once: pass the WebServer instance used for all handlers. */
void app_http_init(WebServer *server);

/** Call once from setup() after app_http_init to start WebSocket server on port 81. */
void app_ws_init(void);

/** Call from loop(): handles WebSocket events. */
void app_ws_loop(void);

/** Call from loop(): runs Modbus read in background, updates cache only when values change, pushes via WS. */
void app_http_background_poll(void);

/** Call after write/link so next poll picks up new values. */
void app_http_invalidate_status_cache(void);

/** GET / — serve index HTML (pass PROGMEM pointer). */
void app_http_handle_index(const char *index_html);

/** GET /api/status — combined status for both channels (one response). */
void app_http_handle_status_all(void);

/** GET /api/status/1 or /api/status/2 */
void app_http_handle_status(void);

/** POST /api/write/1 or /api/write/2 — body or args: reg, val */
void app_http_handle_write(void);

/** GET /api/scan/1 or /api/scan/2 — optional args: start, end */
void app_http_handle_scan(void);

/** POST /api/link — copy Ch1 settings to Ch2 */
void app_http_handle_link(void);

/** GET /api/health — system stats */
void app_http_handle_health(void);

/** GET/POST /api/config — poll rate config */
void app_http_handle_config(void);

/** 404 */
void app_http_not_found(void);

/** Increment stats counters. */
void app_http_stat_modbus_ok(void);
void app_http_stat_modbus_fail(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HTTP_H */
