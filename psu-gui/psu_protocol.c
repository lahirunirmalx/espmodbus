/**
 * PSU protocol implementation - threaded reader for streaming data.
 * Parses all register fields from DATA lines.
 */

#include "psu_protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * Parse extended DATA line with all fields.
 * Format: DATA <ch> setV=<v> setA=<a> outV=<v> outA=<a> outP=<p> outE=<e> inV=<v> temp=<t> \
 *              time=<s> cap=<c> ovp=<v> ocp=<a> opp=<p> status=<s> cvcc=<m> out=<0|1> mppt=<0|1>
 */
static bool parse_data_line(const char *line, int *ch, psu_status_t *st) {
    int channel;
    bool has_error = (strstr(line, " ERR") != NULL);

    /* Parse channel number */
    if (sscanf(line, "DATA %d", &channel) != 1)
        return false;

    *ch = channel;
    memset(st, 0, sizeof(*st));

    /* Parse key=value pairs */
    const char *p = line;

    /* Skip "DATA <ch> " */
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;

    /* Now parse fields */
    char key[32];
    unsigned long val;

    while (*p) {
        /* Skip whitespace */
        while (*p == ' ') p++;
        if (!*p) break;

        /* Find = */
        const char *eq = strchr(p, '=');
        if (!eq) break;

        /* Extract key */
        size_t keylen = eq - p;
        if (keylen >= sizeof(key)) keylen = sizeof(key) - 1;
        memcpy(key, p, keylen);
        key[keylen] = '\0';

        /* Extract value */
        p = eq + 1;
        char *endp;
        val = strtoul(p, &endp, 10);
        p = endp;

        /* Map key to field */
        if (strcmp(key, "setV") == 0) st->set_v = (uint16_t)val;
        else if (strcmp(key, "setA") == 0) st->set_a = (uint16_t)val;
        else if (strcmp(key, "outV") == 0) st->out_v = (uint16_t)val;
        else if (strcmp(key, "outA") == 0) st->out_a = (uint16_t)val;
        else if (strcmp(key, "outP") == 0) st->out_p = (uint16_t)val;
        else if (strcmp(key, "outE") == 0) st->out_e = (uint32_t)val;
        else if (strcmp(key, "inV") == 0) st->in_v = (uint16_t)val;
        else if (strcmp(key, "temp") == 0) st->temp = (uint16_t)val;
        else if (strcmp(key, "time") == 0) st->runtime = (uint32_t)val;
        else if (strcmp(key, "cap") == 0) st->capacity = (uint32_t)val;
        else if (strcmp(key, "ovp") == 0) st->ovp = (uint16_t)val;
        else if (strcmp(key, "ocp") == 0) st->ocp = (uint16_t)val;
        else if (strcmp(key, "opp") == 0) st->opp = (uint16_t)val;
        else if (strcmp(key, "status") == 0) st->status = (uint16_t)val;
        else if (strcmp(key, "cvcc") == 0) st->cvcc = (uint8_t)val;
        else if (strcmp(key, "out") == 0) st->out_on = (uint8_t)val;
        else if (strcmp(key, "mppt") == 0) st->mppt = (uint8_t)val;
    }

    /* Mark as error if ERR flag present, otherwise valid */
    st->valid = !has_error;
    st->timestamp_ms = get_time_ms();

    return true;
}

static void *reader_thread_func(void *arg) {
    psu_context_t *ctx = (psu_context_t *)arg;
    char line[512];

    while (ctx->running) {
        if (serial_read_line(ctx->serial, line, sizeof(line), 100)) {
            /* Parse DATA lines */
            if (strncmp(line, "DATA ", 5) == 0) {
                int ch;
                psu_status_t st;
                if (parse_data_line(line, &ch, &st)) {
                    if (ch >= 1 && ch <= 2) {
                        pthread_mutex_lock(&ctx->lock);
                        /* Only update if valid data - preserve last good values on errors */
                        if (st.valid) {
                            ctx->ch[ch - 1] = st;
                        } else {
                            /* Just mark as error, keep old data */
                            ctx->ch[ch - 1].valid = false;
                        }
                        ctx->rx_count++;
                        ctx->connected = true;
                        pthread_mutex_unlock(&ctx->lock);
                    }
                } else {
                    pthread_mutex_lock(&ctx->lock);
                    ctx->err_count++;
                    pthread_mutex_unlock(&ctx->lock);
                }
            } else if (strncmp(line, "!READY", 6) == 0 || strncmp(line, "PONG", 4) == 0) {
                pthread_mutex_lock(&ctx->lock);
                ctx->connected = true;
                pthread_mutex_unlock(&ctx->lock);
            }
        }
    }

    return NULL;
}

bool psu_init(psu_context_t *ctx, const char *device, int baud) {
    if (!ctx)
        return false;

    memset(ctx, 0, sizeof(*ctx));
    pthread_mutex_init(&ctx->lock, NULL);

    ctx->serial = serial_open(device, baud);
    if (!ctx->serial) {
        return false;
    }

    serial_flush(ctx->serial);

    ctx->running = true;
    if (pthread_create(&ctx->reader_thread, NULL, reader_thread_func, ctx) != 0) {
        serial_close(ctx->serial);
        ctx->serial = NULL;
        return false;
    }

    usleep(200000);

    return true;
}

void psu_shutdown(psu_context_t *ctx) {
    if (!ctx)
        return;

    ctx->running = false;

    if (ctx->reader_thread) {
        pthread_join(ctx->reader_thread, NULL);
    }

    if (ctx->serial) {
        serial_close(ctx->serial);
        ctx->serial = NULL;
    }

    pthread_mutex_destroy(&ctx->lock);
}

void psu_get_status(psu_context_t *ctx, int ch, psu_status_t *out) {
    if (!ctx || !out || ch < 1 || ch > 2) {
        if (out) out->valid = false;
        return;
    }

    pthread_mutex_lock(&ctx->lock);
    *out = ctx->ch[ch - 1];
    pthread_mutex_unlock(&ctx->lock);
}

bool psu_is_connected(psu_context_t *ctx) {
    if (!ctx) return false;
    return ctx->connected;
}

bool psu_write_reg(psu_context_t *ctx, int ch, uint16_t reg, uint16_t val) {
    if (!ctx || !ctx->serial || ch < 1 || ch > 2)
        return false;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "WRITE %d 0x%04X %u", ch, reg, val);
    return serial_send_line(ctx->serial, cmd);
}

bool psu_set_voltage(psu_context_t *ctx, int ch, float volts) {
    uint16_t val = (uint16_t)(volts * 100.0f + 0.5f);
    return psu_write_reg(ctx, ch, PSU_REG_SET_VOLT, val);
}

bool psu_set_current(psu_context_t *ctx, int ch, float amps) {
    uint16_t val = (uint16_t)(amps * 1000.0f + 0.5f);
    return psu_write_reg(ctx, ch, PSU_REG_SET_CURR, val);
}

bool psu_set_output(psu_context_t *ctx, int ch, bool enable) {
    return psu_write_reg(ctx, ch, PSU_REG_OUT_ENABLE, enable ? 1 : 0);
}

bool psu_link(psu_context_t *ctx) {
    if (!ctx || !ctx->serial)
        return false;
    return serial_send_line(ctx->serial, "LINK");
}

bool psu_set_poll_rate(psu_context_t *ctx, int ms) {
    if (!ctx || !ctx->serial)
        return false;
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "POLL %d", ms);
    return serial_send_line(ctx->serial, cmd);
}

void psu_get_stats(psu_context_t *ctx, uint32_t *rx_count, uint32_t *err_count) {
    if (!ctx) {
        if (rx_count) *rx_count = 0;
        if (err_count) *err_count = 0;
        return;
    }

    pthread_mutex_lock(&ctx->lock);
    if (rx_count) *rx_count = ctx->rx_count;
    if (err_count) *err_count = ctx->err_count;
    pthread_mutex_unlock(&ctx->lock);
}
