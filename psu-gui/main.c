/**
 * PSU Control GUI — SDL2 with multithreaded serial communication.
 * Responsive UI with fast updates from streaming ESP32 data.
 */

#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "serial_port.h"
#include "psu_protocol.h"

/* Window dimensions - override before including this file for alternate layouts */
#ifndef WIN_W
#define WIN_W 1240
#endif
#ifndef WIN_H
#define WIN_H 720
#endif

/* Layout constants */
#define HEADER_H     40
#define TOOLBAR_H    36
#define PANEL_W      480
#define PANEL_H      600
#define PANEL_GAP    12
#define KEYPAD_W     180

/* Color palette */
typedef struct { Uint8 r, g, b, a; } Color;

static const Color COL_BG_DARK      = {30, 30, 32, 255};
static const Color COL_BG_PANEL     = {42, 42, 46, 255};
static const Color COL_BG_WIDGET    = {28, 28, 30, 255};
static const Color COL_HEADER       = {24, 24, 26, 255};
static const Color COL_BORDER       = {60, 60, 65, 255};
static const Color COL_BORDER_LIGHT = {80, 80, 88, 255};
static const Color COL_TEXT         = {200, 200, 205, 255};
static const Color COL_TEXT_DIM     = {120, 120, 128, 255};
static const Color COL_LABEL        = {160, 160, 168, 255};
static const Color COL_ACCENT       = {0, 180, 220, 255};
static const Color COL_SUCCESS      = {50, 205, 100, 255};
static const Color COL_WARNING      = {255, 180, 0, 255};
static const Color COL_ERROR        = {220, 60, 60, 255};
static const Color COL_VFD_BG       = {8, 18, 12, 255};
static const Color COL_VFD_ON       = {0, 255, 120, 255};
static const Color COL_VFD_OFF      = {0, 60, 35, 255};
static const Color COL_NEEDLE       = {200, 40, 40, 255};
static const Color COL_SCOPE_BG     = {10, 20, 15, 255};
static const Color COL_SCOPE_GRID   = {30, 50, 35, 255};
static const Color COL_SCOPE_LINE   = {80, 255, 120, 255};
static const Color COL_BTN_NORMAL   = {55, 55, 60, 255};
static const Color COL_BTN_HOVER    = {70, 70, 78, 255};
static const Color COL_BTN_ACTIVE   = {0, 150, 180, 255};
static const Color COL_INPUT_BG     = {22, 22, 24, 255};
static const Color COL_INPUT_BORDER = {70, 70, 78, 255};
static const Color COL_INPUT_FOCUS  = {0, 150, 180, 255};

/* Trace history */
#define TRACE_LEN 150

typedef struct {
    float voltage[TRACE_LEN];
    float current[TRACE_LEN];
    int head;
    int count;
} trace_t;

typedef struct {
    psu_status_t status;
    trace_t trace;
    /* Cached display values - prevents jumping to zero */
    float disp_v;
    float disp_a;
    float disp_p;
} channel_t;

/* Keypad modes */
#define KEYPAD_MODE_VOLTAGE 0
#define KEYPAD_MODE_CURRENT 1

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font_title;
    TTF_Font *font_large;
    TTF_Font *font_medium;
    TTF_Font *font_small;
    TTF_Font *font_vfd;
    TTF_Font *font_vfd_small;

    psu_context_t psu;
    channel_t ch[2];

    bool tracking;
    bool running;
    bool demo_mode;

    int active_input;
    char input_buf[16];
    int hover_btn;

    /* Keypad state */
    int keypad_channel;     /* 0=CH1, 1=CH2 */
    int keypad_mode;        /* KEYPAD_MODE_VOLTAGE or KEYPAD_MODE_CURRENT */
    char keypad_value[16];  /* Current keypad input */
    bool keypad_active;     /* Is keypad entry in progress */

    uint32_t frame_count;
    uint32_t last_fps_time;
    int fps;
} app_t;

static app_t g_app;

/* Button IDs */
#define BTN_TRACKING   1
#define BTN_REFRESH    2
#define BTN_CH1_OUTPUT 10
#define BTN_CH1_SET_V  11
#define BTN_CH1_SET_A  12
#define BTN_CH2_OUTPUT 20
#define BTN_CH2_SET_V  21
#define BTN_CH2_SET_A  22

/* Keypad buttons */
#define BTN_KEY_0      100
#define BTN_KEY_1      101
#define BTN_KEY_2      102
#define BTN_KEY_3      103
#define BTN_KEY_4      104
#define BTN_KEY_5      105
#define BTN_KEY_6      106
#define BTN_KEY_7      107
#define BTN_KEY_8      108
#define BTN_KEY_9      109
#define BTN_KEY_DOT    110
#define BTN_KEY_CLR    111
#define BTN_KEY_BACK   112
#define BTN_KEY_ENTER  113
#define BTN_KEY_CH_TOG 114  /* Channel toggle */
#define BTN_KEY_MODE   115  /* V/A mode toggle */

#define MAX_BUTTONS 64

/* Button definitions */
typedef struct {
    SDL_Rect rect;
    int id;
} button_t;

static button_t g_buttons[MAX_BUTTONS];
static int g_num_buttons = 0;

/* Input field regions */
static SDL_Rect g_inputs[4];

static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Helpers */
static void set_color(SDL_Renderer *r, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(r, &rect);
}

static void fill_rounded_rect(SDL_Renderer *r, int x, int y, int w, int h, int radius) {
    fill_rect(r, x + radius, y, w - 2 * radius, h);
    fill_rect(r, x, y + radius, w, h - 2 * radius);
    for (int corner = 0; corner < 4; corner++) {
        int cx = (corner % 2 == 0) ? x + radius : x + w - radius;
        int cy = (corner < 2) ? y + radius : y + h - radius;
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy <= radius * radius) {
                    SDL_RenderDrawPoint(r, cx + dx, cy + dy);
                }
            }
        }
    }
}

static void draw_text(SDL_Renderer *r, TTF_Font *font, const char *text,
                      int x, int y, Color color, int align) {
    if (!font || !text || !*text) return;
    SDL_Color sdl_col = {color.r, color.g, color.b, color.a};
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, sdl_col);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        if (align == 1) dst.x = x - surf->w / 2;
        else if (align == 2) dst.x = x - surf->w;
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static void draw_text_centered(SDL_Renderer *r, TTF_Font *font, const char *text,
                               int cx, int cy, Color color) {
    if (!font || !text || !*text) return;
    SDL_Color sdl_col = {color.r, color.g, color.b, color.a};
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, sdl_col);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_Rect dst = {cx - surf->w / 2, cy - surf->h / 2, surf->w, surf->h};
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static int add_button(int x, int y, int w, int h, int id) {
    if (g_num_buttons >= MAX_BUTTONS) return -1;
    g_buttons[g_num_buttons] = (button_t){ .rect = {x, y, w, h}, .id = id };
    return g_num_buttons++;
}

static int button_at(int mx, int my) {
    for (int i = 0; i < g_num_buttons; i++) {
        SDL_Rect *r = &g_buttons[i].rect;
        if (mx >= r->x && mx < r->x + r->w && my >= r->y && my < r->y + r->h)
            return g_buttons[i].id;
    }
    return 0;
}

static void draw_button(SDL_Renderer *r, int x, int y, int w, int h,
                        const char *text, bool active, bool hover, int id) {
    Color bg = active ? COL_BTN_ACTIVE : (hover ? COL_BTN_HOVER : COL_BTN_NORMAL);
    Color border = active ? COL_ACCENT : COL_BORDER_LIGHT;
    Color text_col = active ? (Color){255, 255, 255, 255} : COL_TEXT;

    set_color(r, bg);
    fill_rounded_rect(r, x, y, w, h, 3);
    set_color(r, border);
    draw_rect(r, x, y, w, h);
    draw_text_centered(r, g_app.font_small, text, x + w / 2, y + h / 2, text_col);
    add_button(x, y, w, h, id);
}

static void draw_led(SDL_Renderer *r, int cx, int cy, int radius, bool on, Color on_col) {
    Color col = on ? on_col : COL_TEXT_DIM;
    set_color(r, COL_BORDER);
    for (int dy = -radius - 1; dy <= radius + 1; dy++) {
        for (int dx = -radius - 1; dx <= radius + 1; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 <= (radius + 1) * (radius + 1) && d2 > (radius - 1) * (radius - 1))
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
        }
    }
    set_color(r, col);
    for (int dy = -radius + 1; dy < radius; dy++) {
        for (int dx = -radius + 1; dx < radius; dx++) {
            if (dx * dx + dy * dy < (radius - 1) * (radius - 1))
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
        }
    }
    if (on) {
        Color hl = {(Uint8)(col.r + (255 - col.r) / 2),
                    (Uint8)(col.g + (255 - col.g) / 2),
                    (Uint8)(col.b + (255 - col.b) / 2), 255};
        set_color(r, hl);
        for (int dy = -radius / 3; dy <= 0; dy++)
            for (int dx = -radius / 3; dx <= 0; dx++)
                if (dx * dx + dy * dy < radius * radius / 9)
                    SDL_RenderDrawPoint(r, cx + dx - 1, cy + dy - 1);
    }
}

/**
 * 5x7 Dot-matrix VFD character patterns.
 * Each character is 5 columns, 7 rows. Stored as 5 bytes (one per column).
 */
static const uint8_t DOT_MATRIX_FONT[12][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* . (dot) */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* - (minus) */
};

/**
 * Draw a single VFD dot/pixel with glow effect.
 */
static void draw_vfd_dot(SDL_Renderer *r, int cx, int cy, int radius, Color col, bool on) {
    if (!on) {
        /* Off pixel - very dim */
        Color dim = {(Uint8)(col.r / 15), (Uint8)(col.g / 15), (Uint8)(col.b / 15), 80};
        set_color(r, dim);
        for (int dy = -radius + 1; dy < radius; dy++) {
            for (int dx = -radius + 1; dx < radius; dx++) {
                if (dx * dx + dy * dy < radius * radius) {
                    SDL_RenderDrawPoint(r, cx + dx, cy + dy);
                }
            }
        }
        return;
    }

    /* Outer glow */
    int glow_r = radius + 2;
    Color glow1 = {(Uint8)(col.r / 6), (Uint8)(col.g / 6), (Uint8)(col.b / 6), 60};
    set_color(r, glow1);
    for (int dy = -glow_r; dy <= glow_r; dy++) {
        for (int dx = -glow_r; dx <= glow_r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 <= glow_r * glow_r && d2 > radius * radius) {
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
            }
        }
    }

    /* Main pixel */
    set_color(r, col);
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
            }
        }
    }

    /* Bright center highlight */
    Color hi = {(Uint8)(col.r + (255 - col.r) / 2),
                (Uint8)(col.g + (255 - col.g) / 2),
                (Uint8)(col.b + (255 - col.b) / 2), 255};
    set_color(r, hi);
    int hi_r = radius / 2;
    if (hi_r < 1) hi_r = 1;
    for (int dy = -hi_r; dy <= hi_r; dy++) {
        for (int dx = -hi_r; dx <= hi_r; dx++) {
            if (dx * dx + dy * dy <= hi_r * hi_r) {
                SDL_RenderDrawPoint(r, cx + dx - 1, cy + dy - 1);
            }
        }
    }
}

/**
 * Draw a single dot-matrix character.
 */
static void draw_vfd_char(SDL_Renderer *r, int x, int y, int ch_idx,
                          int dot_size, int dot_gap, Color on_col, Color off_col, bool show_off) {
    if (ch_idx < 0 || ch_idx > 11) return;

    const uint8_t *pattern = DOT_MATRIX_FONT[ch_idx];
    int dot_spacing = dot_size * 2 + dot_gap;

    for (int col = 0; col < 5; col++) {
        uint8_t coldata = pattern[col];
        for (int row = 0; row < 7; row++) {
            bool on = (coldata >> row) & 1;
            int px = x + col * dot_spacing + dot_size;
            int py = y + row * dot_spacing + dot_size;
            if (on) {
                draw_vfd_dot(r, px, py, dot_size, on_col, true);
            } else if (show_off) {
                draw_vfd_dot(r, px, py, dot_size, off_col, false);
            }
        }
    }
}

/**
 * Draw a number string using dot-matrix VFD display.
 * Returns the width used.
 */
static int draw_vfd_number(SDL_Renderer *r, int x, int y, const char *str,
                           int dot_size, int dot_gap, int char_gap,
                           Color on_col, Color off_col, bool show_off) {
    int cx = x;
    int char_w = 5 * (dot_size * 2 + dot_gap);

    for (const char *p = str; *p; p++) {
        int ch_idx = -1;
        if (*p >= '0' && *p <= '9') {
            ch_idx = *p - '0';
        } else if (*p == '.') {
            ch_idx = 10;
        } else if (*p == '-') {
            ch_idx = 11;
        } else if (*p == ' ') {
            cx += char_w + char_gap;
            continue;
        }

        if (ch_idx >= 0) {
            draw_vfd_char(r, cx, y, ch_idx, dot_size, dot_gap, on_col, off_col, show_off);
            /* Dot takes less space */
            if (ch_idx == 10) {
                cx += (dot_size * 2 + dot_gap) * 2 + char_gap;
            } else {
                cx += char_w + char_gap;
            }
        }
    }
    return cx - x;
}

/**
 * Square bar meter - horizontal bar with scale.
 */
static void draw_bar_meter(SDL_Renderer *r, int x, int y, int w, int h,
                           float value, float max_val, const char *label, const char *unit) {
    /* Background */
    set_color(r, (Color){35, 35, 38, 255});
    fill_rect(r, x, y, w, h);

    /* Border */
    set_color(r, COL_BORDER_LIGHT);
    draw_rect(r, x, y, w, h);

    /* Scale area */
    int bar_x = x + 8;
    int bar_y = y + 22;
    int bar_w = w - 16;
    int bar_h = 16;

    /* Bar background (dark) */
    set_color(r, (Color){20, 20, 22, 255});
    fill_rect(r, bar_x, bar_y, bar_w, bar_h);

    /* Fill bar */
    float frac = value / max_val;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int fill_w = (int)(bar_w * frac);

    /* Gradient fill - green to yellow to red */
    for (int i = 0; i < fill_w; i++) {
        float f = (float)i / bar_w;
        Uint8 red, green;
        if (f < 0.5f) {
            red = (Uint8)(f * 2 * 200);
            green = 200;
        } else {
            red = 200;
            green = (Uint8)((1.0f - (f - 0.5f) * 2) * 200);
        }
        SDL_SetRenderDrawColor(r, red, green, 50, 255);
        SDL_RenderDrawLine(r, bar_x + i, bar_y + 1, bar_x + i, bar_y + bar_h - 2);
    }

    /* Bar border */
    set_color(r, (Color){80, 80, 85, 255});
    draw_rect(r, bar_x, bar_y, bar_w, bar_h);

    /* Tick marks */
    set_color(r, COL_TEXT_DIM);
    for (int i = 0; i <= 10; i++) {
        int tx = bar_x + bar_w * i / 10;
        int th = (i % 5 == 0) ? 4 : 2;
        SDL_RenderDrawLine(r, tx, bar_y + bar_h, tx, bar_y + bar_h + th);
    }

    /* Label (top left) */
    draw_text(r, g_app.font_small, label, x + 8, y + 4, COL_LABEL, 0);

    /* Value (top right) */
    char val_str[24];
    snprintf(val_str, sizeof(val_str), "%.2f %s", value, unit);
    draw_text(r, g_app.font_small, val_str, x + w - 8, y + 4, COL_TEXT, 2);

    /* Scale labels */
    char scale_str[8];
    snprintf(scale_str, sizeof(scale_str), "0");
    draw_text(r, g_app.font_small, scale_str, bar_x, bar_y + bar_h + 5, COL_TEXT_DIM, 0);
    snprintf(scale_str, sizeof(scale_str), "%.0f", max_val);
    draw_text(r, g_app.font_small, scale_str, bar_x + bar_w, bar_y + bar_h + 5, COL_TEXT_DIM, 2);
}

/**
 * Temperature gauge with C/H scale and warning blink.
 */
static void draw_temp_gauge(SDL_Renderer *r, int x, int y, int w, int h,
                            float temp_c, bool warning_blink) {
    /* Background - dark if normal, red tint if hot */
    bool is_hot = temp_c >= 50.0f;
    bool blink_on = warning_blink && ((SDL_GetTicks() / 300) % 2 == 0);

    if (is_hot && blink_on) {
        set_color(r, (Color){80, 30, 30, 255});
    } else {
        set_color(r, (Color){35, 35, 38, 255});
    }
    fill_rect(r, x, y, w, h);

    /* Border - red if hot */
    if (is_hot) {
        set_color(r, blink_on ? COL_ERROR : (Color){150, 60, 60, 255});
    } else {
        set_color(r, COL_BORDER_LIGHT);
    }
    draw_rect(r, x, y, w, h);
    if (is_hot) {
        draw_rect(r, x + 1, y + 1, w - 2, h - 2);
    }

    /* Title */
    Color title_col = is_hot ? COL_ERROR : COL_LABEL;
    draw_text(r, g_app.font_small, "TEMPERATURE", x + 8, y + 4, title_col, 0);

    /* C and H labels */
    int gauge_x = x + 25;
    int gauge_y = y + 22;
    int gauge_w = w - 50;
    int gauge_h = 20;

    draw_text(r, g_app.font_small, "C", x + 10, gauge_y + 2, (Color){100, 150, 255, 255}, 0);
    draw_text(r, g_app.font_small, "H", x + w - 18, gauge_y + 2, COL_ERROR, 0);

    /* Gauge background */
    set_color(r, (Color){20, 20, 22, 255});
    fill_rect(r, gauge_x, gauge_y, gauge_w, gauge_h);

    /* Temperature bar - gradient blue to red */
    float frac = temp_c / 100.0f;  /* 0-100°C range */
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int fill_w = (int)(gauge_w * frac);

    for (int i = 0; i < fill_w; i++) {
        float f = (float)i / gauge_w;
        Uint8 red, green, blue;
        if (f < 0.5f) {
            /* Blue to green */
            blue = (Uint8)((1.0f - f * 2) * 200);
            green = (Uint8)(f * 2 * 200);
            red = 50;
        } else {
            /* Green to red */
            blue = 50;
            green = (Uint8)((1.0f - (f - 0.5f) * 2) * 200);
            red = (Uint8)((f - 0.5f) * 2 * 200 + 50);
        }
        SDL_SetRenderDrawColor(r, red, green, blue, 255);
        SDL_RenderDrawLine(r, gauge_x + i, gauge_y + 1, gauge_x + i, gauge_y + gauge_h - 2);
    }

    /* Warning zone marker (50°C) */
    int warn_x = gauge_x + gauge_w / 2;
    set_color(r, (Color){200, 100, 0, 255});
    SDL_RenderDrawLine(r, warn_x, gauge_y, warn_x, gauge_y + gauge_h);

    /* Gauge border */
    set_color(r, (Color){80, 80, 85, 255});
    draw_rect(r, gauge_x, gauge_y, gauge_w, gauge_h);

    /* Temperature value */
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.1f", temp_c);
    Color val_col = is_hot ? (blink_on ? (Color){255, 100, 100, 255} : COL_ERROR) : COL_TEXT;
    draw_text(r, g_app.font_medium, temp_str, x + w / 2 - 10, y + gauge_y + gauge_h + 2 - y, val_col, 0);
    draw_text(r, g_app.font_small, "C", x + w / 2 + 20, y + gauge_y + gauge_h + 4 - y, val_col, 0);

    /* Warning text if hot */
    if (is_hot && blink_on) {
        draw_text_centered(r, g_app.font_small, "! HOT !", x + w / 2, y + h - 8, COL_ERROR);
    }
}

/**
 * Draw oscilloscope-style dual trace display with auto-scaling.
 * Shows voltage (green) and current (yellow) on same time base.
 */
static void draw_scope(SDL_Renderer *r, int x, int y, int w, int h,
                       trace_t *trace, const char *label) {
    /* Background */
    set_color(r, COL_SCOPE_BG);
    fill_rect(r, x, y, w, h);

    int samples = trace->count < TRACE_LEN ? trace->count : TRACE_LEN;
    if (samples < 2) {
        set_color(r, COL_BORDER);
        draw_rect(r, x, y, w, h);
        draw_text(r, g_app.font_small, "NO DATA", x + w / 2 - 25, y + h / 2 - 6, COL_TEXT_DIM, 0);
        return;
    }

    /* Auto-scale: find min/max for both channels */
    float v_min = 1e9f, v_max = -1e9f;
    float a_min = 1e9f, a_max = -1e9f;

    for (int i = 0; i < samples; i++) {
        int idx = (trace->head - samples + i + TRACE_LEN) % TRACE_LEN;
        float v = trace->voltage[idx];
        float a = trace->current[idx];
        if (v < v_min) v_min = v;
        if (v > v_max) v_max = v;
        if (a < a_min) a_min = a;
        if (a > a_max) a_max = a;
    }

    /* Add 10% margin and round to nice values */
    float v_range = v_max - v_min;
    float a_range = a_max - a_min;

    /* Minimum range to avoid division by zero */
    if (v_range < 0.5f) { v_range = 0.5f; v_min -= 0.25f; v_max += 0.25f; }
    if (a_range < 0.1f) { a_range = 0.1f; a_min -= 0.05f; a_max += 0.05f; }

    /* Add margin */
    v_min -= v_range * 0.1f;
    v_max += v_range * 0.1f;
    a_min -= a_range * 0.1f;
    a_max += a_range * 0.1f;

    /* Clamp to reasonable values */
    if (v_min < 0) v_min = 0;
    if (a_min < 0) a_min = 0;
    if (v_max > 40) v_max = 40;
    if (a_max > 8) a_max = 8;

    v_range = v_max - v_min;
    a_range = a_max - a_min;

    /* Grid */
    set_color(r, COL_SCOPE_GRID);
    for (int i = 1; i < 4; i++) {
        int gy = y + h * i / 4;
        SDL_RenderDrawLine(r, x, gy, x + w, gy);
    }
    for (int i = 1; i < 10; i++) {
        int gx = x + w * i / 10;
        SDL_RenderDrawLine(r, gx, y, gx, y + h);
    }

    /* Center reference line */
    SDL_SetRenderDrawColor(r, 40, 60, 45, 255);
    SDL_RenderDrawLine(r, x, y + h / 2, x + w, y + h / 2);

    /* Plot area margins */
    int plot_x = x + 2;
    int plot_w = w - 4;
    int plot_y = y + 2;
    int plot_h = h - 4;

    /* Draw CURRENT trace (yellow/orange) - behind voltage */
    Color col_current = {255, 180, 50, 255};
    set_color(r, col_current);

    int prev_px = -1, prev_py = -1;
    for (int i = 0; i < samples; i++) {
        int idx = (trace->head - samples + i + TRACE_LEN) % TRACE_LEN;
        float a = trace->current[idx];
        int px = plot_x + i * plot_w / TRACE_LEN;
        int py = plot_y + plot_h - (int)((a - a_min) / a_range * plot_h);
        if (py < plot_y) py = plot_y;
        if (py > plot_y + plot_h - 1) py = plot_y + plot_h - 1;
        if (prev_px >= 0) {
            SDL_RenderDrawLine(r, prev_px, prev_py, px, py);
        }
        prev_px = px;
        prev_py = py;
    }

    /* Draw VOLTAGE trace (green) - in front */
    set_color(r, COL_SCOPE_LINE);

    prev_px = -1;
    prev_py = -1;
    for (int i = 0; i < samples; i++) {
        int idx = (trace->head - samples + i + TRACE_LEN) % TRACE_LEN;
        float v = trace->voltage[idx];
        int px = plot_x + i * plot_w / TRACE_LEN;
        int py = plot_y + plot_h - (int)((v - v_min) / v_range * plot_h);
        if (py < plot_y) py = plot_y;
        if (py > plot_y + plot_h - 1) py = plot_y + plot_h - 1;
        if (prev_px >= 0) {
            SDL_RenderDrawLine(r, prev_px, prev_py, px, py);
        }
        prev_px = px;
        prev_py = py;
    }

    /* Border */
    set_color(r, COL_BORDER);
    draw_rect(r, x, y, w, h);

    /* Scale labels - voltage (left, green) */
    char scale_str[16];
    snprintf(scale_str, sizeof(scale_str), "%.1fV", v_max);
    draw_text(r, g_app.font_small, scale_str, x + 3, y + 2, COL_SCOPE_LINE, 0);
    snprintf(scale_str, sizeof(scale_str), "%.1fV", v_min);
    draw_text(r, g_app.font_small, scale_str, x + 3, y + h - 14, COL_SCOPE_LINE, 0);

    /* Scale labels - current (right, yellow) */
    snprintf(scale_str, sizeof(scale_str), "%.2fA", a_max);
    draw_text(r, g_app.font_small, scale_str, x + w - 5, y + 2, col_current, 2);
    snprintf(scale_str, sizeof(scale_str), "%.2fA", a_min);
    draw_text(r, g_app.font_small, scale_str, x + w - 5, y + h - 14, col_current, 2);

    /* Channel label */
    draw_text(r, g_app.font_small, label, x + w / 2, y + h - 14, COL_TEXT_DIM, 1);

    /* Legend */
    set_color(r, COL_SCOPE_LINE);
    SDL_RenderDrawLine(r, x + 5, y + h - 26, x + 20, y + h - 26);
    draw_text(r, g_app.font_small, "V", x + 23, y + h - 30, COL_SCOPE_LINE, 0);

    set_color(r, col_current);
    SDL_RenderDrawLine(r, x + 35, y + h - 26, x + 50, y + h - 26);
    draw_text(r, g_app.font_small, "A", x + 53, y + h - 30, col_current, 0);
}

static void draw_vfd_display(SDL_Renderer *r, int x, int y, int w, int h,
                             float voltage, float current, float power, bool output_on) {
    /* VFD background - dark green-black with subtle texture */
    set_color(r, COL_VFD_BG);
    fill_rect(r, x, y, w, h);

    /* Inner bezel effect - deeper */
    SDL_SetRenderDrawColor(r, 1, 6, 3, 255);
    fill_rect(r, x + 3, y + 3, w - 6, h - 6);

    /* Subtle scan line effect */
    SDL_SetRenderDrawColor(r, 0, 10, 5, 30);
    for (int ly = y + 5; ly < y + h - 5; ly += 2) {
        SDL_RenderDrawLine(r, x + 5, ly, x + w - 5, ly);
    }

    /* Border - triple line for depth */
    SDL_SetRenderDrawColor(r, 30, 40, 35, 255);
    draw_rect(r, x + 2, y + 2, w - 4, h - 4);
    SDL_SetRenderDrawColor(r, 20, 30, 25, 255);
    draw_rect(r, x + 1, y + 1, w - 2, h - 2);
    set_color(r, COL_BORDER);
    draw_rect(r, x, y, w, h);

    /* Colors for 7-segment display */
    Color on_col = output_on ? COL_VFD_ON : COL_VFD_OFF;
    Color off_col = {0, 20, 12, 255}; /* Dim "ghost" segments */
    Color label_col = {200, 160, 60, 255}; /* Amber labels */
    Color unit_col = output_on ? (Color){0, 220, 110, 255} : (Color){0, 45, 25, 255};

    /* === OUTPUT INDICATOR AT TOP === */
    int header_y = y + 8;
    
    /* LED indicator with glow */
    int led_x = x + 15;
    int led_y = header_y + 8;
    int led_r = 5;
    
    if (output_on) {
        /* Outer glow */
        SDL_SetRenderDrawColor(r, 0, 255, 120, 30);
        for (int dy = -led_r - 4; dy <= led_r + 4; dy++) {
            for (int dx = -led_r - 4; dx <= led_r + 4; dx++) {
                int dist2 = dx * dx + dy * dy;
                if (dist2 <= (led_r + 4) * (led_r + 4) && dist2 > led_r * led_r) {
                    SDL_RenderDrawPoint(r, led_x + dx, led_y + dy);
                }
            }
        }
        /* Core LED */
        set_color(r, COL_VFD_ON);
        for (int dy = -led_r; dy <= led_r; dy++) {
            for (int dx = -led_r; dx <= led_r; dx++) {
                if (dx * dx + dy * dy <= led_r * led_r) {
                    SDL_RenderDrawPoint(r, led_x + dx, led_y + dy);
                }
            }
        }
        /* Bright center */
        SDL_SetRenderDrawColor(r, 150, 255, 200, 255);
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if (dx * dx + dy * dy <= 4) {
                    SDL_RenderDrawPoint(r, led_x + dx, led_y + dy);
                }
            }
        }
    } else {
        set_color(r, (Color){0, 25, 15, 255});
        for (int dy = -led_r; dy <= led_r; dy++) {
            for (int dx = -led_r; dx <= led_r; dx++) {
                if (dx * dx + dy * dy <= led_r * led_r) {
                    SDL_RenderDrawPoint(r, led_x + dx, led_y + dy);
                }
            }
        }
    }
    
    /* OUTPUT text */
    Color out_text_col = output_on ? COL_VFD_ON : (Color){0, 40, 25, 255};
    draw_text(r, g_app.font_medium, "OUTPUT", x + 28, header_y, out_text_col, 0);
    
    /* ON/OFF status */
    const char *status_str = output_on ? "ON" : "OFF";
    Color status_col = output_on ? (Color){0, 255, 120, 255} : (Color){80, 0, 0, 255};
    draw_text(r, g_app.font_medium, status_str, x + 95, header_y, status_col, 0);
    
    /* Separator line */
    SDL_SetRenderDrawColor(r, 0, 50, 30, 255);
    SDL_RenderDrawLine(r, x + 8, header_y + 22, x + w - 8, header_y + 22);

    /* Calculate content area (below header) */
    int content_y = header_y + 28;
    int content_h = h - (content_y - y) - 8;
    int row_h = content_h / 3;
    int label_x = x + 10;

    /* Dot-matrix VFD sizing */
    int dot_size = 2;      /* Pixel radius */
    int dot_gap = 1;       /* Gap between dots in character */
    int char_gap = 5;      /* Gap between characters */
    int dot_spacing = dot_size * 2 + dot_gap;
    int char_h = 7 * dot_spacing;  /* Character height */
    int char_w = 5 * dot_spacing;  /* Character width */

    /* Number display starts after label, leaves room for unit on right */
    int num_x = x + 95;    /* After "VOLTAGE" label */

    char buf[16];

    /* === ROW 1: VOLTAGE === Fixed format XX.XXX (00.000 - 99.999) */
    int row1_y = content_y + (row_h - char_h) / 2;
    draw_text(r, g_app.font_medium, "VOLTAGE", label_x, row1_y + char_h / 2 - 6, label_col, 0);

    snprintf(buf, sizeof(buf), "%06.3f", voltage);  /* XX.XXX format e.g. 12.450 */
    int vw = draw_vfd_number(r, num_x, row1_y, buf, dot_size, dot_gap, char_gap, on_col, off_col, true);

    /* Unit - positioned after numbers with gap */
    draw_text(r, g_app.font_large, "V", num_x + vw + 8, row1_y + char_h / 2 - 8, unit_col, 0);

    /* === ROW 2: CURRENT === Fixed format XX.XXX (00.000 - 99.999) */
    int row2_y = content_y + row_h + (row_h - char_h) / 2;
    draw_text(r, g_app.font_medium, "CURRENT", label_x, row2_y + char_h / 2 - 6, label_col, 0);

    snprintf(buf, sizeof(buf), "%06.3f", current);  /* XX.XXX format e.g. 01.500 */
    int aw = draw_vfd_number(r, num_x, row2_y, buf, dot_size, dot_gap, char_gap, on_col, off_col, true);

    draw_text(r, g_app.font_large, "A", num_x + aw + 8, row2_y + char_h / 2 - 8, unit_col, 0);

    /* === ROW 3: POWER === Fixed format XXX.XX (000.00 - 999.99) */
    int row3_y = content_y + 2 * row_h + (row_h - char_h) / 2;
    draw_text(r, g_app.font_medium, "POWER", label_x, row3_y + char_h / 2 - 6, label_col, 0);

    snprintf(buf, sizeof(buf), "%06.2f", power);  /* XXX.XX format e.g. 017.25 */
    int pw = draw_vfd_number(r, num_x, row3_y, buf, dot_size, dot_gap, char_gap, on_col, off_col, true);

    draw_text(r, g_app.font_large, "W", num_x + pw + 8, row3_y + char_h / 2 - 8, unit_col, 0);
}

static void draw_input_field(SDL_Renderer *r, int x, int y, int w, int h,
                             const char *value, const char *label, bool active, int input_idx) {
    draw_text(r, g_app.font_small, label, x, y - 16, COL_LABEL, 0);
    set_color(r, COL_INPUT_BG);
    fill_rect(r, x, y, w, h);
    set_color(r, active ? COL_INPUT_FOCUS : COL_INPUT_BORDER);
    draw_rect(r, x, y, w, h);
    if (active) draw_rect(r, x + 1, y + 1, w - 2, h - 2);
    draw_text(r, g_app.font_medium, value, x + 8, y + (h - 14) / 2, COL_TEXT, 0);
    if (input_idx >= 0 && input_idx < 4)
        g_inputs[input_idx] = (SDL_Rect){x, y, w, h};
}

static void draw_channel_panel(SDL_Renderer *r, int ch_idx, int x, int y) {
    channel_t *ch = &g_app.ch[ch_idx];
    psu_status_t *st = &ch->status;

    set_color(r, COL_BG_PANEL);
    fill_rounded_rect(r, x, y, PANEL_W, PANEL_H, 4);
    set_color(r, COL_BORDER);
    draw_rect(r, x, y, PANEL_W, PANEL_H);

    set_color(r, COL_BG_WIDGET);
    fill_rect(r, x + 1, y + 1, PANEL_W - 2, 32);

    char title[32];
    snprintf(title, sizeof(title), "OUTPUT %d", ch_idx + 1);
    draw_text(r, g_app.font_medium, title, x + 12, y + 8, COL_TEXT, 0);
    draw_text(r, g_app.font_small, "+36V / 6A", x + PANEL_W - 80, y + 10, COL_TEXT_DIM, 0);

    int vfd_x = x + 10, vfd_y = y + 40, vfd_w = PANEL_W - 20, vfd_h = 180;
    /* Use cached display values to prevent jumping */
    draw_vfd_display(r, vfd_x, vfd_y, vfd_w, vfd_h, ch->disp_v, ch->disp_a, ch->disp_p, st->out_on);

    int ctrl_y = vfd_y + vfd_h + 15;
    int btn_id = (ch_idx == 0) ? BTN_CH1_OUTPUT : BTN_CH2_OUTPUT;
    draw_button(r, x + 15, ctrl_y, 90, 30, "OUTPUT", st->out_on, g_app.hover_btn == btn_id, btn_id);
    draw_led(r, x + 120, ctrl_y + 15, 6, st->out_on, COL_SUCCESS);

    Color status_col = st->valid ? COL_SUCCESS : COL_ERROR;
    draw_text(r, g_app.font_small, "STATUS:", x + 145, ctrl_y + 8, COL_LABEL, 0);
    draw_text(r, g_app.font_small, st->valid ? "OK" : "ERR", x + 200, ctrl_y + 8, status_col, 0);

    int set_y = ctrl_y + 50;
    char v_str[16], a_str[16];
    snprintf(v_str, sizeof(v_str), "%.2f", st->set_v / 100.0f);
    snprintf(a_str, sizeof(a_str), "%.3f", st->set_a / 1000.0f);

    int v_idx = ch_idx * 2, a_idx = ch_idx * 2 + 1;
    bool v_active = (g_app.active_input == v_idx + 1);
    bool a_active = (g_app.active_input == a_idx + 1);

    draw_input_field(r, x + 15, set_y, 100, 28, v_active ? g_app.input_buf : v_str,
                     "SET VOLTAGE (V)", v_active, v_idx);
    int set_v_btn = (ch_idx == 0) ? BTN_CH1_SET_V : BTN_CH2_SET_V;
    draw_button(r, x + 120, set_y, 50, 28, "SET", false, g_app.hover_btn == set_v_btn, set_v_btn);

    draw_input_field(r, x + 200, set_y, 100, 28, a_active ? g_app.input_buf : a_str,
                     "SET CURRENT (A)", a_active, a_idx);
    int set_a_btn = (ch_idx == 0) ? BTN_CH1_SET_A : BTN_CH2_SET_A;
    draw_button(r, x + 305, set_y, 50, 28, "SET", false, g_app.hover_btn == set_a_btn, set_a_btn);

    /* CV/CC mode indicator */
    const char *mode_str = st->cvcc ? "CC" : "CV";
    Color mode_col = st->cvcc ? COL_WARNING : COL_SUCCESS;
    draw_text(r, g_app.font_small, "MODE:", x + 380, ctrl_y + 8, COL_LABEL, 0);
    draw_text(r, g_app.font_small, mode_str, x + 420, ctrl_y + 8, mode_col, 0);

    /* Square bar meters */
    int meter_y = set_y + 50;
    int meter_w = (PANEL_W - 45) / 2;
    int meter_h = 55;

    /* Voltage meter */
    draw_bar_meter(r, x + 15, meter_y, meter_w, meter_h,
                   st->out_v / 100.0f, 36.0f, "VOLTAGE", "V");

    /* Current meter */
    draw_bar_meter(r, x + 25 + meter_w, meter_y, meter_w, meter_h,
                   st->out_a / 1000.0f, 6.0f, "CURRENT", "A");

    /* Temperature gauge */
    int temp_y = meter_y + meter_h + 10;
    float temp_c = st->temp / 10.0f;  /* temp is °C * 10 */
    draw_temp_gauge(r, x + 15, temp_y, PANEL_W - 30, 70, temp_c, true);

    int scope_y = temp_y + 80;
    char scope_label[16];
    snprintf(scope_label, sizeof(scope_label), "CH%d", ch_idx + 1);
    draw_scope(r, x + 15, scope_y, PANEL_W - 30, 90, &ch->trace, scope_label);
}

static void draw_header(SDL_Renderer *r) {
    set_color(r, COL_HEADER);
    fill_rect(r, 0, 0, WIN_W, HEADER_H);
    set_color(r, COL_BORDER);
    SDL_RenderDrawLine(r, 0, HEADER_H - 1, WIN_W, HEADER_H - 1);

    draw_text(r, g_app.font_title, "DUAL OUTPUT DC POWER SUPPLY", 20, 10, COL_TEXT, 0);

    set_color(r, COL_ACCENT);
    fill_rounded_rect(r, 420, 10, 70, 24, 3);
    draw_text_centered(r, g_app.font_small, "36V/6A", 455, 22, (Color){255, 255, 255, 255});

    bool connected = psu_is_connected(&g_app.psu);
    Color status_col = connected ? COL_SUCCESS : (g_app.demo_mode ? COL_WARNING : COL_ERROR);
    const char *status_txt = connected ? "ONLINE" : (g_app.demo_mode ? "DEMO" : "OFFLINE");
    draw_led(r, WIN_W - 100, 22, 5, true, status_col);
    draw_text(r, g_app.font_medium, status_txt, WIN_W - 88, 14, status_col, 0);

    /* FPS and stats */
    char stats[64];
    uint32_t rx, err;
    psu_get_stats(&g_app.psu, &rx, &err);
    snprintf(stats, sizeof(stats), "FPS:%d RX:%u", g_app.fps, rx);
    draw_text(r, g_app.font_small, stats, WIN_W - 200, 30, COL_TEXT_DIM, 0);
}

/**
 * Draw keypad button with proper styling.
 */
static void draw_keypad_btn(SDL_Renderer *r, int x, int y, int w, int h,
                            const char *label, bool highlight, int id) {
    bool hover = (g_app.hover_btn == id);
    Color bg = highlight ? COL_ACCENT : (hover ? COL_BTN_HOVER : (Color){50, 50, 55, 255});
    Color border = highlight ? COL_ACCENT : COL_BORDER_LIGHT;
    Color text_col = highlight ? (Color){255, 255, 255, 255} : COL_TEXT;

    set_color(r, bg);
    fill_rounded_rect(r, x, y, w, h, 4);
    set_color(r, border);
    draw_rect(r, x, y, w, h);
    draw_text_centered(r, g_app.font_medium, label, x + w / 2, y + h / 2, text_col);
    add_button(x, y, w, h, id);
}

/**
 * Draw the common keypad panel - fills available height.
 */
static void draw_keypad(SDL_Renderer *r, int x, int y, int w, int h) {
    int margin = 8;
    int inner_w = w - margin * 2;

    /* Panel background */
    set_color(r, COL_BG_PANEL);
    fill_rounded_rect(r, x, y, w, h, 4);
    set_color(r, COL_BORDER);
    draw_rect(r, x, y, w, h);

    /* Title bar */
    set_color(r, COL_BG_WIDGET);
    fill_rect(r, x + 1, y + 1, w - 2, 30);
    draw_text(r, g_app.font_medium, "KEYPAD", x + margin, y + 8, COL_TEXT, 0);

    int cur_y = y + 38;

    /* Channel and Mode toggles - fit within panel */
    const char *ch_label = (g_app.keypad_channel == 0) ? "CH1" : "CH2";
    Color ch_col = (g_app.keypad_channel == 0) ? COL_SUCCESS : COL_ACCENT;
    int tog_gap = 6;
    int tog_btn_w = (inner_w - tog_gap) / 2;
    draw_keypad_btn(r, x + margin, cur_y, tog_btn_w, 28, ch_label, false, BTN_KEY_CH_TOG);
    draw_led(r, x + margin + tog_btn_w - 8, cur_y + 14, 4, true, ch_col);

    /* Mode toggle (V/A) - positioned correctly */
    const char *mode_label = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? "VOLTS" : "AMPS";
    Color mode_col = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? (Color){100, 200, 100, 255} : (Color){255, 200, 100, 255};
    draw_keypad_btn(r, x + margin + tog_btn_w + tog_gap, cur_y, tog_btn_w, 28, mode_label, false, BTN_KEY_MODE);

    cur_y += 42;

    /* Display current value - larger */
    int disp_h = 42;
    set_color(r, (Color){10, 15, 12, 255});
    fill_rect(r, x + margin, cur_y, inner_w, disp_h);
    set_color(r, COL_BORDER);
    draw_rect(r, x + margin, cur_y, inner_w, disp_h);

    /* Show current input value or placeholder */
    const char *disp_val = g_app.keypad_value;
    if (strlen(g_app.keypad_value) == 0) {
        disp_val = "0.00";
    }
    Color val_col = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? COL_VFD_ON : (Color){255, 200, 100, 255};
    draw_text(r, g_app.font_large, disp_val, x + w - margin - 8, cur_y + 8, val_col, 2);

    /* Unit indicator */
    const char *unit = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? "V" : "A";
    draw_text(r, g_app.font_medium, unit, x + margin + 5, cur_y + 12, mode_col, 0);

    cur_y += disp_h + 6;

    /* Target indicator */
    char target[32];
    snprintf(target, sizeof(target), "-> CH%d %s", g_app.keypad_channel + 1,
             (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? "Vset" : "Iset");
    draw_text(r, g_app.font_small, target, x + margin, cur_y, COL_TEXT_DIM, 0);

    cur_y += 22;

    /* Calculate button sizes to fill remaining space */
    int bottom_info_h = 70; /* Space for current values display */
    int avail_h = (y + h) - cur_y - bottom_info_h - margin;
    int btn_rows = 4;
    int btn_gap = 6;
    int btn_h = (avail_h - (btn_rows - 1) * btn_gap) / btn_rows;
    if (btn_h < 36) btn_h = 36;
    if (btn_h > 52) btn_h = 52;

    int btn_cols = 4;
    int btn_w = (inner_w - (btn_cols - 1) * btn_gap) / btn_cols;
    int pad_x = x + margin;

    /* Row 1: 7 8 9 CLR */
    draw_keypad_btn(r, pad_x + 0 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "7", false, BTN_KEY_7);
    draw_keypad_btn(r, pad_x + 1 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "8", false, BTN_KEY_8);
    draw_keypad_btn(r, pad_x + 2 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "9", false, BTN_KEY_9);
    draw_keypad_btn(r, pad_x + 3 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "C", false, BTN_KEY_CLR);

    /* Row 2: 4 5 6 BACK */
    cur_y += btn_h + btn_gap;
    draw_keypad_btn(r, pad_x + 0 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "4", false, BTN_KEY_4);
    draw_keypad_btn(r, pad_x + 1 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "5", false, BTN_KEY_5);
    draw_keypad_btn(r, pad_x + 2 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "6", false, BTN_KEY_6);
    draw_keypad_btn(r, pad_x + 3 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "<", false, BTN_KEY_BACK);

    /* Row 3: 1 2 3 + ENTER (spans 2 rows) */
    cur_y += btn_h + btn_gap;
    draw_keypad_btn(r, pad_x + 0 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "1", false, BTN_KEY_1);
    draw_keypad_btn(r, pad_x + 1 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "2", false, BTN_KEY_2);
    draw_keypad_btn(r, pad_x + 2 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "3", false, BTN_KEY_3);

    /* ENTER button spans 2 rows */
    int enter_h = btn_h * 2 + btn_gap;
    draw_keypad_btn(r, pad_x + 3 * (btn_w + btn_gap), cur_y, btn_w, enter_h, "OK", true, BTN_KEY_ENTER);

    /* Row 4: 0 (wide) . */
    cur_y += btn_h + btn_gap;
    int wide_btn = btn_w * 2 + btn_gap;
    draw_keypad_btn(r, pad_x, cur_y, wide_btn, btn_h, "0", false, BTN_KEY_0);
    draw_keypad_btn(r, pad_x + wide_btn + btn_gap, cur_y, btn_w, btn_h, ".", false, BTN_KEY_DOT);

    /* Current channel values display at bottom */
    cur_y += btn_h + 12;
    draw_text(r, g_app.font_small, "SETPOINTS:", x + margin, cur_y, COL_LABEL, 0);
    cur_y += 18;

    /* CH1 values */
    char val_str[32];
    psu_status_t *st1 = &g_app.ch[0].status;
    snprintf(val_str, sizeof(val_str), "CH1: %.2fV / %.3fA", st1->set_v / 100.0f, st1->set_a / 1000.0f);
    Color ch1_col = (g_app.keypad_channel == 0) ? COL_SUCCESS : COL_TEXT_DIM;
    draw_text(r, g_app.font_small, val_str, x + margin, cur_y, ch1_col, 0);

    /* CH2 values */
    cur_y += 16;
    psu_status_t *st2 = &g_app.ch[1].status;
    snprintf(val_str, sizeof(val_str), "CH2: %.2fV / %.3fA", st2->set_v / 100.0f, st2->set_a / 1000.0f);
    Color ch2_col = (g_app.keypad_channel == 1) ? COL_ACCENT : COL_TEXT_DIM;
    draw_text(r, g_app.font_small, val_str, x + margin, cur_y, ch2_col, 0);
}

static void draw_toolbar(SDL_Renderer *r) {
    int y = HEADER_H;
    set_color(r, COL_BG_WIDGET);
    fill_rect(r, 0, y, WIN_W, TOOLBAR_H);
    set_color(r, COL_BORDER);
    SDL_RenderDrawLine(r, 0, y + TOOLBAR_H - 1, WIN_W, y + TOOLBAR_H - 1);

    draw_text(r, g_app.font_small, "SYSTEM CONTROL", 20, y + 12, COL_LABEL, 0);
    draw_button(r, 140, y + 8, 100, 26, "TRACKING", g_app.tracking, g_app.hover_btn == BTN_TRACKING, BTN_TRACKING);
    draw_led(r, 250, y + 21, 5, g_app.tracking, COL_SUCCESS);
    draw_button(r, WIN_W - 100, y + 8, 80, 26, "REFRESH", false, g_app.hover_btn == BTN_REFRESH, BTN_REFRESH);
}

static void render(void) {
    SDL_Renderer *r = g_app.renderer;
    set_color(r, COL_BG_DARK);
    SDL_RenderClear(r);
    g_num_buttons = 0;

    draw_header(r);
    draw_toolbar(r);

    /* Layout: [CH1 Panel] [CH2 Panel] [Keypad] */
    int panels_y = HEADER_H + TOOLBAR_H + 10;
    int avail_h = WIN_H - panels_y - 10;
    int total_w = PANEL_W * 2 + PANEL_GAP + KEYPAD_W + PANEL_GAP;
    int start_x = (WIN_W - total_w) / 2;

    draw_channel_panel(r, 0, start_x, panels_y);
    draw_channel_panel(r, 1, start_x + PANEL_W + PANEL_GAP, panels_y);

    /* Keypad on the right - same height as panels */
    int keypad_x = start_x + PANEL_W * 2 + PANEL_GAP * 2;
    int keypad_y = panels_y;
    draw_keypad(r, keypad_x, keypad_y, KEYPAD_W, avail_h);

    SDL_RenderPresent(r);
}

/**
 * Update cached display values for a channel.
 * Only updates if values are non-zero and valid.
 */
static void update_display_values(int ch_idx) {
    channel_t *ch = &g_app.ch[ch_idx];
    psu_status_t *st = &ch->status;

    float new_v, new_a, new_p;

    if (st->out_on) {
        /* Output ON: show actual output values */
        new_v = st->out_v / 100.0f;
        new_a = st->out_a / 1000.0f;
        new_p = st->out_p / 100.0f;
    } else {
        /* Output OFF: show setpoints, power is 0 */
        new_v = st->set_v / 100.0f;
        new_a = st->set_a / 1000.0f;
        new_p = 0.0f;
    }

    /* Only update if we have valid non-zero data, or if output is OFF */
    /* This prevents jumping to zero when data stream has gaps */
    if (!st->out_on || (new_v > 0.01f || new_a > 0.001f)) {
        ch->disp_v = new_v;
        ch->disp_a = new_a;
        ch->disp_p = new_p;
    }
    /* If output is ON but values are zero, keep showing last good values */
}

static void update_from_psu(void) {
    for (int i = 0; i < 2; i++) {
        psu_status_t st;
        psu_get_status(&g_app.psu, i + 1, &st);

        if (st.valid) {
            /* Full update with valid data */
            g_app.ch[i].status = st;

            /* Update cached display values */
            update_display_values(i);

            /* Update trace only with valid output data */
            if (st.out_v > 0 || st.out_a > 0) {
                trace_t *tr = &g_app.ch[i].trace;
                tr->voltage[tr->head] = st.out_v / 100.0f;
                tr->current[tr->head] = st.out_a / 1000.0f;
                tr->head = (tr->head + 1) % TRACE_LEN;
                if (tr->count < TRACE_LEN) tr->count++;
            }
        }
        /* If not valid, keep showing old data - don't change anything */
    }
}

static void update_demo(void) {
    static float phase = 0.0f;
    phase += 0.1f;

    for (int i = 0; i < 2; i++) {
        psu_status_t *st = &g_app.ch[i].status;
        st->set_v = 1200;
        st->set_a = 1500;
        st->out_v = 1180 + (int)(40.0f * sinf(phase + i * 0.5f));
        st->out_a = 1480 + (int)(60.0f * sinf(phase * 0.7f + i));
        st->out_p = (uint16_t)((st->out_v / 100.0f) * (st->out_a / 1000.0f) * 100.0f);
        st->out_on = 1;
        st->valid = true;

        /* Temperature simulation - Ch2 runs hotter to demo warning */
        st->temp = 350 + (int)(100.0f * sinf(phase * 0.3f + i * 2.0f)); /* 35-45°C for ch1 */
        if (i == 1) {
            st->temp = 450 + (int)(150.0f * sinf(phase * 0.2f)); /* 30-60°C for ch2, triggers warning */
        }

        /* CV/CC mode - alternate based on current */
        st->cvcc = (st->out_a > 1500) ? 1 : 0;

        /* Update cached display values */
        update_display_values(i);

        trace_t *tr = &g_app.ch[i].trace;
        tr->voltage[tr->head] = st->out_v / 100.0f;
        tr->current[tr->head] = st->out_a / 1000.0f;
        tr->head = (tr->head + 1) % TRACE_LEN;
        if (tr->count < TRACE_LEN) tr->count++;
    }
}

static bool point_in_rect(int x, int y, SDL_Rect *rect) {
    return x >= rect->x && x < rect->x + rect->w && y >= rect->y && y < rect->y + rect->h;
}

/**
 * Append a character to keypad value if valid.
 */
static void keypad_append(char c) {
    size_t len = strlen(g_app.keypad_value);

    /* Limit length */
    if (len >= 8) return;

    /* Only one decimal point allowed */
    if (c == '.' && strchr(g_app.keypad_value, '.') != NULL) return;

    /* Limit decimal places */
    char *dot = strchr(g_app.keypad_value, '.');
    if (dot) {
        int dec_places = len - (dot - g_app.keypad_value) - 1;
        if (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE && dec_places >= 2) return;
        if (g_app.keypad_mode == KEYPAD_MODE_CURRENT && dec_places >= 3) return;
    }

    g_app.keypad_value[len] = c;
    g_app.keypad_value[len + 1] = '\0';
}

/**
 * Apply keypad value to the selected channel/mode.
 */
static void keypad_apply(void) {
    if (strlen(g_app.keypad_value) == 0) return;

    float val = atof(g_app.keypad_value);
    int ch = g_app.keypad_channel + 1; /* 1 or 2 */

    if (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) {
        if (val >= 0 && val <= 36) {
            if (!g_app.demo_mode) {
                psu_set_voltage(&g_app.psu, ch, val);
            } else {
                g_app.ch[g_app.keypad_channel].status.set_v = (uint16_t)(val * 100);
            }
        }
    } else {
        if (val >= 0 && val <= 6) {
            if (!g_app.demo_mode) {
                psu_set_current(&g_app.psu, ch, val);
            } else {
                g_app.ch[g_app.keypad_channel].status.set_a = (uint16_t)(val * 1000);
            }
        }
    }

    /* Clear keypad after apply */
    g_app.keypad_value[0] = '\0';
}

static void handle_click(int mx, int my) {
    for (int i = 0; i < 4; i++) {
        if (point_in_rect(mx, my, &g_inputs[i])) {
            g_app.active_input = i + 1;
            int ch = i / 2;
            if (i % 2 == 0)
                snprintf(g_app.input_buf, sizeof(g_app.input_buf), "%.2f", g_app.ch[ch].status.set_v / 100.0f);
            else
                snprintf(g_app.input_buf, sizeof(g_app.input_buf), "%.3f", g_app.ch[ch].status.set_a / 1000.0f);
            return;
        }
    }

    int btn = button_at(mx, my);
    if (btn == 0) { g_app.active_input = 0; return; }

    switch (btn) {
        case BTN_TRACKING:
            g_app.tracking = !g_app.tracking;
            if (g_app.tracking && !g_app.demo_mode) psu_link(&g_app.psu);
            break;
        case BTN_REFRESH:
            break;
        case BTN_CH1_OUTPUT:
        case BTN_CH2_OUTPUT: {
            int ch = (btn == BTN_CH1_OUTPUT) ? 0 : 1;
            bool new_state = !g_app.ch[ch].status.out_on;
            if (!g_app.demo_mode) psu_set_output(&g_app.psu, ch + 1, new_state);
            else g_app.ch[ch].status.out_on = new_state;
            break;
        }
        case BTN_CH1_SET_V:
        case BTN_CH2_SET_V: {
            int ch = (btn == BTN_CH1_SET_V) ? 0 : 1;
            int exp = ch * 2 + 1;
            if (g_app.active_input == exp) {
                float v = atof(g_app.input_buf);
                if (v >= 0 && v <= 36) {
                    if (!g_app.demo_mode) psu_set_voltage(&g_app.psu, ch + 1, v);
                    else g_app.ch[ch].status.set_v = (uint16_t)(v * 100);
                }
                g_app.active_input = 0;
            }
            break;
        }
        case BTN_CH1_SET_A:
        case BTN_CH2_SET_A: {
            int ch = (btn == BTN_CH1_SET_A) ? 0 : 1;
            int exp = ch * 2 + 2;
            if (g_app.active_input == exp) {
                float a = atof(g_app.input_buf);
                if (a >= 0 && a <= 6) {
                    if (!g_app.demo_mode) psu_set_current(&g_app.psu, ch + 1, a);
                    else g_app.ch[ch].status.set_a = (uint16_t)(a * 1000);
                }
                g_app.active_input = 0;
            }
            break;
        }

        /* Keypad buttons */
        case BTN_KEY_0: keypad_append('0'); break;
        case BTN_KEY_1: keypad_append('1'); break;
        case BTN_KEY_2: keypad_append('2'); break;
        case BTN_KEY_3: keypad_append('3'); break;
        case BTN_KEY_4: keypad_append('4'); break;
        case BTN_KEY_5: keypad_append('5'); break;
        case BTN_KEY_6: keypad_append('6'); break;
        case BTN_KEY_7: keypad_append('7'); break;
        case BTN_KEY_8: keypad_append('8'); break;
        case BTN_KEY_9: keypad_append('9'); break;
        case BTN_KEY_DOT: keypad_append('.'); break;

        case BTN_KEY_CLR:
            g_app.keypad_value[0] = '\0';
            break;

        case BTN_KEY_BACK: {
            size_t len = strlen(g_app.keypad_value);
            if (len > 0) g_app.keypad_value[len - 1] = '\0';
            break;
        }

        case BTN_KEY_ENTER:
            keypad_apply();
            break;

        case BTN_KEY_CH_TOG:
            g_app.keypad_channel = (g_app.keypad_channel == 0) ? 1 : 0;
            break;

        case BTN_KEY_MODE:
            g_app.keypad_mode = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ?
                                KEYPAD_MODE_CURRENT : KEYPAD_MODE_VOLTAGE;
            g_app.keypad_value[0] = '\0'; /* Clear on mode change */
            break;
    }
}

static void handle_key(SDL_Keycode key) {
    /* Keypad keyboard shortcuts (always active) */
    if (key == SDLK_TAB) {
        /* Tab toggles channel */
        g_app.keypad_channel = (g_app.keypad_channel == 0) ? 1 : 0;
        return;
    }
    if (key == SDLK_v) {
        /* V selects voltage mode */
        if (g_app.keypad_mode != KEYPAD_MODE_VOLTAGE) {
            g_app.keypad_mode = KEYPAD_MODE_VOLTAGE;
            g_app.keypad_value[0] = '\0';
        }
        return;
    }
    if (key == SDLK_a) {
        /* A selects current mode */
        if (g_app.keypad_mode != KEYPAD_MODE_CURRENT) {
            g_app.keypad_mode = KEYPAD_MODE_CURRENT;
            g_app.keypad_value[0] = '\0';
        }
        return;
    }

    /* Keypad backspace/enter/escape (when no input field active) */
    if (g_app.active_input == 0) {
        size_t klen = strlen(g_app.keypad_value);
        if (key == SDLK_BACKSPACE && klen > 0) {
            g_app.keypad_value[klen - 1] = '\0';
            return;
        }
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            keypad_apply();
            return;
        }
        if (key == SDLK_ESCAPE || key == SDLK_c) {
            g_app.keypad_value[0] = '\0';
            return;
        }
        return;
    }

    /* Input field handling */
    size_t len = strlen(g_app.input_buf);

    if (key == SDLK_BACKSPACE && len > 0) {
        g_app.input_buf[len - 1] = '\0';
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        int btn = 0;
        switch (g_app.active_input) {
            case 1: btn = BTN_CH1_SET_V; break;
            case 2: btn = BTN_CH1_SET_A; break;
            case 3: btn = BTN_CH2_SET_V; break;
            case 4: btn = BTN_CH2_SET_A; break;
        }
        if (btn) {
            for (int i = 0; i < g_num_buttons; i++) {
                if (g_buttons[i].id == btn) {
                    handle_click(g_buttons[i].rect.x + 1, g_buttons[i].rect.y + 1);
                    break;
                }
            }
        }
    } else if (key == SDLK_ESCAPE) {
        g_app.active_input = 0;
    }
}

static void handle_text(const char *text) {
    /* If no input field active, direct to keypad */
    if (g_app.active_input == 0) {
        for (const char *p = text; *p; p++) {
            if ((*p >= '0' && *p <= '9') || *p == '.') {
                keypad_append(*p);
            }
        }
        return;
    }

    /* Input field text entry */
    for (const char *p = text; *p; p++) {
        if ((*p >= '0' && *p <= '9') || *p == '.') {
            size_t len = strlen(g_app.input_buf);
            if (len < sizeof(g_app.input_buf) - 1) {
                g_app.input_buf[len] = *p;
                g_app.input_buf[len + 1] = '\0';
            }
        }
    }
}

static bool init_app(const char *serial_device) {
    memset(&g_app, 0, sizeof(g_app));
    g_app.running = true;
    g_app.keypad_channel = 0;
    g_app.keypad_mode = KEYPAD_MODE_VOLTAGE;
    g_app.keypad_value[0] = '\0';

    /* Initialize display values to reasonable defaults */
    for (int i = 0; i < 2; i++) {
        g_app.ch[i].disp_v = 0.0f;
        g_app.ch[i].disp_a = 0.0f;
        g_app.ch[i].disp_p = 0.0f;
    }

    if (!psu_init(&g_app.psu, serial_device, 115200)) {
        printf("Could not open %s, running in DEMO mode\n", serial_device);
        g_app.demo_mode = true;
    } else {
        printf("Connected to %s\n", serial_device);
        psu_set_poll_rate(&g_app.psu, 80); /* 80ms = ~12 Hz per channel */
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    g_app.window = SDL_CreateWindow("PSU Control",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!g_app.window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit(); SDL_Quit();
        return false;
    }

    g_app.renderer = SDL_CreateRenderer(g_app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_app.renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_app.window);
        TTF_Quit(); SDL_Quit();
        return false;
    }
    SDL_SetRenderDrawBlendMode(g_app.renderer, SDL_BLENDMODE_BLEND);

    const char *font_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        NULL
    };
    const char *font_path = NULL;
    for (int i = 0; font_paths[i]; i++) {
        FILE *f = fopen(font_paths[i], "r");
        if (f) { fclose(f); font_path = font_paths[i]; break; }
    }
    if (!font_path) {
        fprintf(stderr, "No suitable font found.\n");
        SDL_DestroyRenderer(g_app.renderer);
        SDL_DestroyWindow(g_app.window);
        TTF_Quit(); SDL_Quit();
        return false;
    }

    g_app.font_title = TTF_OpenFont(font_path, 20);
    g_app.font_large = TTF_OpenFont(font_path, 18);
    g_app.font_medium = TTF_OpenFont(font_path, 13);
    g_app.font_small = TTF_OpenFont(font_path, 11);
    g_app.font_vfd = TTF_OpenFont(font_path, 26);
    g_app.font_vfd_small = TTF_OpenFont(font_path, 16);

    if (!g_app.font_title || !g_app.font_large || !g_app.font_medium ||
        !g_app.font_small || !g_app.font_vfd || !g_app.font_vfd_small) {
        fprintf(stderr, "Failed to load fonts\n");
        SDL_DestroyRenderer(g_app.renderer);
        SDL_DestroyWindow(g_app.window);
        TTF_Quit(); SDL_Quit();
        return false;
    }

    g_app.last_fps_time = SDL_GetTicks();
    return true;
}

static void cleanup_app(void) {
    psu_shutdown(&g_app.psu);
    if (g_app.font_title) TTF_CloseFont(g_app.font_title);
    if (g_app.font_large) TTF_CloseFont(g_app.font_large);
    if (g_app.font_medium) TTF_CloseFont(g_app.font_medium);
    if (g_app.font_small) TTF_CloseFont(g_app.font_small);
    if (g_app.font_vfd) TTF_CloseFont(g_app.font_vfd);
    if (g_app.font_vfd_small) TTF_CloseFont(g_app.font_vfd_small);
    if (g_app.renderer) SDL_DestroyRenderer(g_app.renderer);
    if (g_app.window) SDL_DestroyWindow(g_app.window);
    TTF_Quit();
    SDL_Quit();
}

int main(int argc, char *argv[]) {
    const char *serial_device = "/dev/ttyUSB0";
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [serial_port]\n", argv[0]);
            printf("  Default: /dev/ttyUSB0\n");
            printf("  Runs in DEMO mode if port unavailable.\n");
            return 0;
        }
        serial_device = argv[1];
    }

    if (!init_app(serial_device)) return 1;

    while (g_app.running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: g_app.running = false; break;
                case SDL_MOUSEBUTTONDOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT) handle_click(ev.button.x, ev.button.y);
                    break;
                case SDL_MOUSEMOTION:
                    g_app.hover_btn = button_at(ev.motion.x, ev.motion.y);
                    break;
                case SDL_KEYDOWN: handle_key(ev.key.keysym.sym); break;
                case SDL_TEXTINPUT: handle_text(ev.text.text); break;
            }
        }

        /* Update from PSU or demo */
        if (g_app.demo_mode) update_demo();
        else update_from_psu();

        render();

        /* FPS counter */
        g_app.frame_count++;
        uint32_t now = SDL_GetTicks();
        if (now - g_app.last_fps_time >= 1000) {
            g_app.fps = g_app.frame_count;
            g_app.frame_count = 0;
            g_app.last_fps_time = now;
        }

        SDL_Delay(8); /* ~120 FPS max */
    }

    cleanup_app();
    return 0;
}
