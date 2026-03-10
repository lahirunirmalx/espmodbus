/**
 * App HTTP — REST handlers for status, write, scan, link.
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

/** GET / — serve index HTML (pass PROGMEM pointer). */
void app_http_handle_index(const char *index_html);

/** GET /api/status/1 or /api/status/2 */
void app_http_handle_status(void);

/** POST /api/write/1 or /api/write/2 — body or args: reg, val */
void app_http_handle_write(void);

/** GET /api/scan/1 or /api/scan/2 — optional args: start, end */
void app_http_handle_scan(void);

/** POST /api/link — copy Ch1 settings to Ch2 */
void app_http_handle_link(void);

/** 404 */
void app_http_not_found(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HTTP_H */
