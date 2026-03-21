/**
 * PSU protocol layer — thread-safe, handles streaming data with all registers.
 */

#ifndef PSU_PROTOCOL_H
#define PSU_PROTOCOL_H

#include "serial_port.h"
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

/**
 * Channel status with all available register values.
 */
typedef struct {
    /* Setpoints */
    uint16_t set_v;       /* Voltage setpoint: V * 100 */
    uint16_t set_a;       /* Current limit: A * 1000 */

    /* Output readings */
    uint16_t out_v;       /* Output voltage: V * 100 */
    uint16_t out_a;       /* Output current: A * 1000 */
    uint16_t out_p;       /* Output power: W * 100 */
    uint32_t out_e;       /* Output energy: Wh */

    /* Input / Temperature */
    uint16_t in_v;        /* Input voltage: V * 100 */
    uint16_t temp;        /* Temperature: °C * 10 */

    /* Timers */
    uint32_t runtime;     /* Runtime: seconds */
    uint32_t capacity;    /* Capacity: Ah * 1000 */

    /* Protection thresholds */
    uint16_t ovp;         /* Over-voltage protection: V * 100 */
    uint16_t ocp;         /* Over-current protection: A * 1000 */
    uint16_t opp;         /* Over-power protection: W * 100 */

    /* Status */
    uint16_t status;      /* Status flags */
    uint8_t  cvcc;        /* CV/CC mode: 0=CV, 1=CC */
    uint8_t  out_on;      /* Output enable: 0/1 */
    uint8_t  mppt;        /* MPPT enable: 0/1 */

    /* Meta */
    bool     valid;       /* true if data is valid */
    uint64_t timestamp_ms; /* last update time */
} psu_status_t;

/* Known register addresses */
#define PSU_REG_SET_VOLT   0x0000
#define PSU_REG_SET_CURR   0x0001
#define PSU_REG_OUT_VOLT   0x0002
#define PSU_REG_OUT_CURR   0x0003
#define PSU_REG_OUT_POWER  0x0004
#define PSU_REG_OUT_ENABLE 0x0012
#define PSU_REG_MPPT       0x001F

/* PSU context for thread-safe operations */
typedef struct {
    serial_port_t *serial;
    psu_status_t ch[2];
    pthread_mutex_t lock;
    pthread_t reader_thread;
    volatile bool running;
    volatile bool connected;
    volatile uint32_t rx_count;
    volatile uint32_t err_count;
} psu_context_t;

/**
 * Initialize PSU context and start reader thread.
 */
bool psu_init(psu_context_t *ctx, const char *device, int baud);

/**
 * Shutdown PSU context and stop reader thread.
 */
void psu_shutdown(psu_context_t *ctx);

/**
 * Get channel status (thread-safe copy).
 */
void psu_get_status(psu_context_t *ctx, int ch, psu_status_t *out);

/**
 * Check if connected.
 */
bool psu_is_connected(psu_context_t *ctx);

/**
 * Write a register (queued to serial, thread-safe).
 */
bool psu_write_reg(psu_context_t *ctx, int ch, uint16_t reg, uint16_t val);

/**
 * Set voltage (V as float).
 */
bool psu_set_voltage(psu_context_t *ctx, int ch, float volts);

/**
 * Set current (A as float).
 */
bool psu_set_current(psu_context_t *ctx, int ch, float amps);

/**
 * Enable/disable output.
 */
bool psu_set_output(psu_context_t *ctx, int ch, bool enable);

/**
 * Link Ch1 to Ch2.
 */
bool psu_link(psu_context_t *ctx);

/**
 * Set poll rate on ESP32.
 */
bool psu_set_poll_rate(psu_context_t *ctx, int ms);

/**
 * Get stats.
 */
void psu_get_stats(psu_context_t *ctx, uint32_t *rx_count, uint32_t *err_count);

#endif /* PSU_PROTOCOL_H */
