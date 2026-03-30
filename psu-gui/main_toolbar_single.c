/**
 * PSU toolbar GUI — single channel: strip with V/A, status, SET popup.
 * Same behavior as main_toolbar.c but one hardware channel only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "psu_protocol.h"

#define WIN_W           500
#define WIN_H_STRIP     80
#define HEADER_H        18
#define STRIP_BLOCK_H   (WIN_H_STRIP - HEADER_H - 8)
#define STRIP_TOP_Y     (HEADER_H + 4)
#define STRIP_BOTTOM_Y  (STRIP_TOP_Y + STRIP_BLOCK_H)
#define POPUP_PW           300
#define POPUP_PH           204
#define POPUP_BELOW_STRIP  8
#define POPUP_PAD_BOTTOM   12
#define WIN_H_POPUP        (STRIP_BOTTOM_Y + POPUP_BELOW_STRIP + POPUP_PH + POPUP_PAD_BOTTOM)

typedef struct { Uint8 r, g, b, a; } Color;

static const Color COL_BG_DARK   = {22, 22, 24, 255};
static const Color COL_BG_STRIP  = {32, 32, 36, 255};
static const Color COL_HEADER    = {20, 20, 22, 255};
static const Color COL_BORDER    = {55, 55, 60, 255};
static const Color COL_V_BRIGHT  = {0, 255, 140, 255};
static const Color COL_A_BRIGHT  = {120, 220, 255, 255};
static const Color COL_ON        = {0, 255, 100, 255};
static const Color COL_OFF       = {90, 35, 35, 255};
static const Color COL_CV        = {100, 255, 140, 255};
static const Color COL_CC        = {255, 200, 60, 255};
static const Color COL_ERR       = {255, 70, 70, 255};
static const Color COL_DIM       = {100, 100, 108, 255};
static const Color COL_BTN       = {50, 50, 55, 255};
static const Color COL_BTN_HOVER = {68, 68, 75, 255};
static const Color COL_BTN_HI    = {0, 140, 170, 255};
static const Color COL_POPUP_BG  = {40, 40, 44, 250};
static const Color COL_INPUT_BG  = {18, 18, 20, 255};
static const Color COL_INPUT_FG  = {220, 220, 225, 255};
static const Color COL_FOCUS     = {0, 180, 220, 255};

typedef struct {
    SDL_Rect rect;
    int id;
} button_t;

enum {
    BTN_CH_OUT = 10,
    BTN_CH_SET = 11,
    BTN_POP_APPLY = 30,
    BTN_POP_CANCEL = 31,
    BTN_POP_FIELD_V = 32,
    BTN_POP_FIELD_A = 33,
};

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font_label;
    TTF_Font *font_num;
    TTF_Font *font_stat;
    TTF_Font *font_pop;
    psu_context_t psu;
    psu_status_t ch;
    bool running;
    bool demo_mode;
    int hover_btn;

    bool popup_open;
    int popup_focus;        /* 0 = V field, 1 = A field */
    char popup_v[16];
    char popup_a[16];
} app_t;

static app_t g_app;
static button_t g_buttons[24];
static int g_num_buttons = 0;

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

static void draw_text(SDL_Renderer *r, TTF_Font *font, const char *text,
                      int x, int y, Color color, int align) {
    if (!font || !text || !*text) return;
    SDL_Color c = {color.r, color.g, color.b, color.a};
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, c);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }
    SDL_Rect dst = {x, y, surf->w, surf->h};
    if (align == 1) dst.x = x - surf->w / 2;
    else if (align == 2) dst.x = x - surf->w;
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static int add_button(int x, int y, int w, int h, int id) {
    if (g_num_buttons >= (int)(sizeof(g_buttons) / sizeof(g_buttons[0]))) return -1;
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

static void draw_led(SDL_Renderer *r, int cx, int cy, int rad, bool on, Color on_col) {
    Color col = on ? on_col : COL_DIM;
    set_color(r, col);
    for (int dy = -rad; dy <= rad; dy++) {
        for (int dx = -rad; dx <= rad; dx++) {
            if (dx * dx + dy * dy <= rad * rad)
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
        }
    }
}

static void draw_btn(int x, int y, int w, int h, const char *t, int id, bool primary) {
    bool hov = (id >= 0 && g_app.hover_btn == id);
    Color bg = primary ? COL_BTN_HI : (hov ? COL_BTN_HOVER : COL_BTN);
    Color fg = primary ? (Color){255, 255, 255, 255} : COL_INPUT_FG;
    set_color(g_app.renderer, bg);
    fill_rect(g_app.renderer, x, y, w, h);
    set_color(g_app.renderer, COL_BORDER);
    draw_rect(g_app.renderer, x, y, w, h);
    draw_text(g_app.renderer, g_app.font_label, t, x + w / 2, y + (h - 12) / 2, fg, 1);
    if (id >= 0) add_button(x, y, w, h, id);
}

/**
 * Single output column: label | big V | big A | status | SET | OUT
 */
static void draw_channel_strip(int x, int y, int w, int h, bool register_hits) {
    psu_status_t *st = &g_app.ch;
    set_color(g_app.renderer, COL_BG_STRIP);
    fill_rect(g_app.renderer, x, y, w, h);
    set_color(g_app.renderer, COL_BORDER);
    draw_rect(g_app.renderer, x, y, w, h);

    draw_text(g_app.renderer, g_app.font_label, "OUT", x + 6, y + 4, COL_DIM, 0);

    float v = st->out_v / 100.0f;
    float a = st->out_a / 1000.0f;
    char buf_v[20], buf_a[20];
    snprintf(buf_v, sizeof(buf_v), "%05.2f", v);
    snprintf(buf_a, sizeof(buf_a), "%05.3f", a);

    int btn_y = y + (h - 24) / 2;
    int set_x = x + w - 88;
    int out_x = x + w - 44;

    int num_x = x + 42;
    draw_text(g_app.renderer, g_app.font_num, buf_v, num_x, y + 16, st->valid ? COL_V_BRIGHT : COL_DIM, 0);
    int vw = 0, vh = 0;
    TTF_SizeText(g_app.font_num, buf_v, &vw, &vh);
    draw_text(g_app.renderer, g_app.font_label, "V", num_x + vw + 3, y + 28, COL_V_BRIGHT, 0);

    int ax = num_x + vw + 22;
    draw_text(g_app.renderer, g_app.font_num, buf_a, ax, y + 16, st->valid ? COL_A_BRIGHT : COL_DIM, 0);
    int aw = 0;
    TTF_SizeText(g_app.font_num, buf_a, &aw, &vh);
    draw_text(g_app.renderer, g_app.font_label, "A", ax + aw + 3, y + 28, COL_A_BRIGHT, 0);

    int stat_x = x + w - 196;
    if (stat_x < ax + aw + 50)
        stat_x = ax + aw + 50;
    const char *out_s = st->out_on ? "ON" : "OFF";
    Color out_c = st->out_on ? COL_ON : COL_OFF;
    draw_text(g_app.renderer, g_app.font_stat, out_s, stat_x, y + 3, out_c, 0);
    int ow = 0;
    TTF_SizeText(g_app.font_stat, out_s, &ow, &vh);
    const char *mode_s = st->cvcc ? "CC" : "CV";
    Color mode_c = st->cvcc ? COL_CC : COL_CV;
    draw_text(g_app.renderer, g_app.font_stat, mode_s, stat_x + ow + 12, y + 6, mode_c, 0);

    if (!st->valid) {
        draw_text(g_app.renderer, g_app.font_label, "ERR", stat_x + ow + 54, y + 7, COL_ERR, 0);
    }

    if (register_hits) {
        draw_btn(set_x, btn_y, 40, 24, "SET", BTN_CH_SET, false);
        draw_btn(out_x, btn_y, 36, 24, "OUT", BTN_CH_OUT, st->out_on);
    } else {
        draw_btn(set_x, btn_y, 40, 24, "SET", -1, false);
        draw_btn(out_x, btn_y, 36, 24, "OUT", -1, st->out_on);
    }
}

static void popup_close(void) {
    if (!g_app.popup_open) return;
    g_app.popup_open = false;
    SDL_SetWindowSize(g_app.window, WIN_W, WIN_H_STRIP);
}

static void popup_open_for(void) {
    g_app.popup_open = true;
    g_app.popup_focus = 0;
    snprintf(g_app.popup_v, sizeof(g_app.popup_v), "%.2f", g_app.ch.set_v / 100.0f);
    snprintf(g_app.popup_a, sizeof(g_app.popup_a), "%.3f", g_app.ch.set_a / 1000.0f);
    SDL_SetWindowSize(g_app.window, WIN_W, WIN_H_POPUP);
}

static void popup_apply(void) {
    float fv = (float)atof(g_app.popup_v);
    float fa = (float)atof(g_app.popup_a);

    if (fv >= 0.0f && fv <= 36.0f) {
        if (!g_app.demo_mode) psu_set_voltage(&g_app.psu, 1, fv);
        else g_app.ch.set_v = (uint16_t)(fv * 100.0f);
    }
    if (fa >= 0.0f && fa <= 6.0f) {
        if (!g_app.demo_mode) psu_set_current(&g_app.psu, 1, fa);
        else g_app.ch.set_a = (uint16_t)(fa * 1000.0f);
    }
    popup_close();
}

static void popup_get_panel_rect(int winw, SDL_Rect *r) {
    r->w = POPUP_PW;
    r->h = POPUP_PH;
    r->x = (winw - r->w) / 2;
    r->y = STRIP_BOTTOM_Y + POPUP_BELOW_STRIP;
}

static void draw_popup(void) {
    if (!g_app.popup_open) return;

    int winw = WIN_W, winh = WIN_H_POPUP;
    SDL_GetWindowSize(g_app.window, &winw, &winh);

    if (winh > STRIP_BOTTOM_Y) {
        set_color(g_app.renderer, (Color){0, 0, 0, 170});
        fill_rect(g_app.renderer, 0, STRIP_BOTTOM_Y, winw, winh - STRIP_BOTTOM_Y);
    }

    SDL_Rect pr;
    popup_get_panel_rect(winw, &pr);
    int px = pr.x, py = pr.y, pw = pr.w, ph = pr.h;

    set_color(g_app.renderer, COL_POPUP_BG);
    fill_rect(g_app.renderer, px, py, pw, ph);
    set_color(g_app.renderer, COL_FOCUS);
    draw_rect(g_app.renderer, px, py, pw, ph);
    draw_rect(g_app.renderer, px + 1, py + 1, pw - 2, ph - 2);

    draw_text(g_app.renderer, g_app.font_pop, "SET OUTPUT", px + 14, py + 12, COL_INPUT_FG, 0);

    int fw = pw - 28;
    int fh = 30;
    int field_gap = 18;
    int fy_v = py + 44;
    int fy_a = fy_v + fh + field_gap;

    bool fv = (g_app.popup_focus == 0);
    bool fa = (g_app.popup_focus == 1);
    set_color(g_app.renderer, COL_INPUT_BG);
    fill_rect(g_app.renderer, px + 14, fy_v, fw, fh);
    set_color(g_app.renderer, fv ? COL_FOCUS : COL_BORDER);
    draw_rect(g_app.renderer, px + 14, fy_v, fw, fh);
    draw_text(g_app.renderer, g_app.font_pop, "V", px + 14, fy_v - 14, COL_DIM, 0);
    draw_text(g_app.renderer, g_app.font_pop, g_app.popup_v, px + 22, fy_v + 6, COL_V_BRIGHT, 0);
    add_button(px + 14, fy_v, fw, fh, BTN_POP_FIELD_V);

    fill_rect(g_app.renderer, px + 14, fy_a, fw, fh);
    set_color(g_app.renderer, fa ? COL_FOCUS : COL_BORDER);
    draw_rect(g_app.renderer, px + 14, fy_a, fw, fh);
    draw_text(g_app.renderer, g_app.font_pop, "A", px + 14, fy_a - 14, COL_DIM, 0);
    draw_text(g_app.renderer, g_app.font_pop, g_app.popup_a, px + 22, fy_a + 6, COL_A_BRIGHT, 0);
    add_button(px + 14, fy_a, fw, fh, BTN_POP_FIELD_A);

    int by = fy_a + fh + 18;
    draw_btn(px + 14, by, 130, 30, "APPLY", BTN_POP_APPLY, true);
    draw_btn(px + pw - 144, by, 130, 30, "CANCEL", BTN_POP_CANCEL, false);
}

static void render(void) {
    SDL_Renderer *r = g_app.renderer;
    int winw = WIN_W, winh = WIN_H_STRIP;
    SDL_GetWindowSize(g_app.window, &winw, &winh);

    set_color(r, COL_BG_DARK);
    SDL_RenderClear(r);
    g_num_buttons = 0;

    set_color(g_app.renderer, COL_HEADER);
    fill_rect(g_app.renderer, 0, 0, winw, HEADER_H);
    set_color(g_app.renderer, COL_BORDER);
    SDL_RenderDrawLine(g_app.renderer, 0, HEADER_H - 1, winw, HEADER_H - 1);

    draw_text(g_app.renderer, g_app.font_label, "PSU", 8, 5, COL_DIM, 0);

    bool ok = psu_is_connected(&g_app.psu);
    Color dot = ok ? COL_ON : (g_app.demo_mode ? COL_CC : COL_ERR);
    draw_led(g_app.renderer, winw - 12, HEADER_H / 2, 4, true, dot);

    int y0 = STRIP_TOP_Y;
    int gap = 6;
    int col_w = winw - gap * 2;
    bool hits = !g_app.popup_open;
    draw_channel_strip(gap, y0, col_w, STRIP_BLOCK_H, hits);

    draw_popup();

    SDL_RenderPresent(r);
}

static bool point_in_popupBackdrop(int mx, int my) {
    if (!g_app.popup_open) return false;
    int winw = WIN_W;
    SDL_GetWindowSize(g_app.window, &winw, NULL);
    SDL_Rect pr;
    popup_get_panel_rect(winw, &pr);
    return mx < pr.x || mx >= pr.x + pr.w || my < pr.y || my >= pr.y + pr.h;
}

static void handle_click(int mx, int my) {
    if (g_app.popup_open) {
        if (point_in_popupBackdrop(mx, my)) {
            popup_close();
            return;
        }
        int b = button_at(mx, my);
        switch (b) {
            case BTN_POP_FIELD_V: g_app.popup_focus = 0; return;
            case BTN_POP_FIELD_A: g_app.popup_focus = 1; return;
            case BTN_POP_APPLY: popup_apply(); return;
            case BTN_POP_CANCEL:
                popup_close();
                return;
            default: return;
        }
    }

    int btn = button_at(mx, my);
    switch (btn) {
        case BTN_CH_SET: popup_open_for(); break;
        case BTN_CH_OUT: {
            bool en = !g_app.ch.out_on;
            if (!g_app.demo_mode) psu_set_output(&g_app.psu, 1, en);
            else g_app.ch.out_on = en ? 1 : 0;
            break;
        }
        default: break;
    }
}

static void append_to_popup(const char *text) {
    for (const char *p = text; *p; p++) {
        if ((*p < '0' || *p > '9') && *p != '.') continue;
        char *buf = (g_app.popup_focus == 0) ? g_app.popup_v : g_app.popup_a;
        size_t len = strlen(buf);
        if (len >= sizeof(g_app.popup_v) - 1) continue;
        if (*p == '.' && strchr(buf, '.') != NULL) continue;
        buf[len] = *p;
        buf[len + 1] = '\0';
    }
}

static void handle_key(SDL_Keycode key) {
    if (!g_app.popup_open) return;
    char *buf = (g_app.popup_focus == 0) ? g_app.popup_v : g_app.popup_a;
    size_t len = strlen(buf);

    if (key == SDLK_TAB) {
        g_app.popup_focus = 1 - g_app.popup_focus;
        return;
    }
    if (key == SDLK_BACKSPACE && len > 0) {
        buf[len - 1] = '\0';
        return;
    }
    if (key == SDLK_ESCAPE) {
        popup_close();
        return;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        popup_apply();
        return;
    }
}

static void handle_text(const char *text) {
    if (g_app.popup_open) append_to_popup(text);
}

static void update_from_psu(void) {
    psu_status_t st;
    psu_get_status(&g_app.psu, 1, &st);
    if (st.valid) g_app.ch = st;
}

static void update_demo(void) {
    static float phase = 0.0f;
    phase += 0.08f;
    psu_status_t *st = &g_app.ch;
    st->set_v = 1200;
    st->set_a = 1500;
    st->out_v = st->set_v - (uint16_t)(20.0f + 10.0f * SDL_sinf(phase));
    st->out_a = st->set_a - (uint16_t)(30.0f + 15.0f * SDL_sinf(phase * 0.7f));
    st->out_p = (uint16_t)((st->out_v / 100.0f) * (st->out_a / 1000.0f) * 100.0f);
    st->out_on = 1;
    st->cvcc = (st->out_a > st->set_a - 20) ? 1 : 0;
    st->valid = true;
}

static bool open_fonts(void) {
    const char *paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        NULL,
    };
    const char *path = NULL;
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (f) {
            fclose(f);
            path = paths[i];
            break;
        }
    }
    if (!path) return false;

    g_app.font_label = TTF_OpenFont(path, 12);
    g_app.font_num = TTF_OpenFont(path, 28);
    g_app.font_stat = TTF_OpenFont(path, 16);
    g_app.font_pop = TTF_OpenFont(path, 15);
    return g_app.font_label && g_app.font_num && g_app.font_stat && g_app.font_pop;
}

static bool init_app(const char *serial_device) {
    memset(&g_app, 0, sizeof(g_app));
    g_app.running = true;

    if (!psu_init(&g_app.psu, serial_device, 115200)) {
        printf("Could not open %s, running in DEMO mode\n", serial_device);
        g_app.demo_mode = true;
    } else {
        printf("Connected to %s\n", serial_device);
        psu_set_poll_rate(&g_app.psu, 100);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
    if (TTF_Init() < 0) {
        SDL_Quit();
        return false;
    }

    g_app.window = SDL_CreateWindow("PSU",
                                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    WIN_W, WIN_H_STRIP, SDL_WINDOW_SHOWN);
    if (!g_app.window) return false;

    g_app.renderer = SDL_CreateRenderer(g_app.window, -1,
                                        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_app.renderer) return false;
    SDL_SetRenderDrawBlendMode(g_app.renderer, SDL_BLENDMODE_BLEND);

    if (!open_fonts()) return false;

    return true;
}

static void cleanup_app(void) {
    psu_shutdown(&g_app.psu);
    if (g_app.font_label) TTF_CloseFont(g_app.font_label);
    if (g_app.font_num) TTF_CloseFont(g_app.font_num);
    if (g_app.font_stat) TTF_CloseFont(g_app.font_stat);
    if (g_app.font_pop) TTF_CloseFont(g_app.font_pop);
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
            return 0;
        }
        serial_device = argv[1];
    }

    if (!init_app(serial_device)) return 1;
    SDL_StartTextInput();

    while (g_app.running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: g_app.running = false; break;
                case SDL_MOUSEBUTTONDOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT)
                        handle_click(ev.button.x, ev.button.y);
                    break;
                case SDL_MOUSEMOTION:
                    g_app.hover_btn = button_at(ev.motion.x, ev.motion.y);
                    break;
                case SDL_KEYDOWN:
                    handle_key(ev.key.keysym.sym);
                    break;
                case SDL_TEXTINPUT:
                    handle_text(ev.text.text);
                    break;
            }
        }

        if (g_app.demo_mode) update_demo();
        else update_from_psu();
        render();

        SDL_Delay(12);
    }

    SDL_StopTextInput();
    cleanup_app();
    return 0;
}
