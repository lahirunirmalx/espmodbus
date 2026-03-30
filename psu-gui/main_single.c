/*
 * Single-channel PSU GUI — compact layout, collapsible keypad.
 */
#define WIN_W 688
#define WIN_H 698
#define main dual_channel_main_unused
#include "main.c"
#undef main

#define SINGLE_MARGIN_X   10
#define SINGLE_GAP        6
#define KEYPAD_COLLAPSED_W 40

static bool g_keypad_expanded = true;
static SDL_Rect g_keypad_toggle_hit;

static bool point_in_rect_single(int x, int y, const SDL_Rect *rect) {
    return x >= rect->x && x < rect->x + rect->w && y >= rect->y && y < rect->y + rect->h;
}

static int single_window_width(void) {
    int kw = g_keypad_expanded ? KEYPAD_W : KEYPAD_COLLAPSED_W;
    return SINGLE_MARGIN_X + PANEL_W + SINGLE_GAP + kw + SINGLE_MARGIN_X;
}

static void single_sync_window_size(void) {
    if (!g_app.window) return;
    int w = single_window_width();
    int cur_w = 0, cur_h = 0;
    SDL_GetWindowSize(g_app.window, &cur_w, &cur_h);
    if (cur_w != w || cur_h != WIN_H)
        SDL_SetWindowSize(g_app.window, w, WIN_H);
}

static void draw_header_single(SDL_Renderer *r) {
    set_color(r, COL_HEADER);
    fill_rect(r, 0, 0, WIN_W, HEADER_H);
    set_color(r, COL_BORDER);
    SDL_RenderDrawLine(r, 0, HEADER_H - 1, WIN_W, HEADER_H - 1);

    draw_text(r, g_app.font_title, "DC POWER SUPPLY", SINGLE_MARGIN_X, 10, COL_TEXT, 0);

    set_color(r, COL_ACCENT);
    fill_rounded_rect(r, 200, 10, 70, 24, 3);
    draw_text_centered(r, g_app.font_small, "36V/6A", 235, 22, (Color){255, 255, 255, 255});

    bool connected = psu_is_connected(&g_app.psu);
    Color status_col = connected ? COL_SUCCESS : (g_app.demo_mode ? COL_WARNING : COL_ERROR);
    const char *status_txt = connected ? "ONLINE" : (g_app.demo_mode ? "DEMO" : "OFFLINE");
    draw_led(r, WIN_W - 92, 22, 5, true, status_col);
    draw_text(r, g_app.font_medium, status_txt, WIN_W - 80, 14, status_col, 0);

    char stats[64];
    uint32_t rx, err;
    psu_get_stats(&g_app.psu, &rx, &err);
    snprintf(stats, sizeof(stats), "FPS:%d RX:%u", g_app.fps, rx);
    draw_text(r, g_app.font_small, stats, WIN_W - 168, 30, COL_TEXT_DIM, 0);
}

static void draw_toolbar_single(SDL_Renderer *r) {
    int y = HEADER_H;
    set_color(r, COL_BG_WIDGET);
    fill_rect(r, 0, y, WIN_W, TOOLBAR_H);
    set_color(r, COL_BORDER);
    SDL_RenderDrawLine(r, 0, y + TOOLBAR_H - 1, WIN_W, y + TOOLBAR_H - 1);

    draw_text(r, g_app.font_small, "CONTROL", SINGLE_MARGIN_X, y + 12, COL_LABEL, 0);
    draw_button(r, WIN_W - 88, y + 8, 78, 26, "REFRESH", false, g_app.hover_btn == BTN_REFRESH, BTN_REFRESH);
}

static void draw_keypad_collapse_tab(SDL_Renderer *r, int x, int y, int w, int h, bool expanded) {
    set_color(r, COL_BG_PANEL);
    fill_rounded_rect(r, x, y, w, h, 4);
    set_color(r, COL_BORDER);
    draw_rect(r, x, y, w, h);
    const char *lbl = expanded ? "<" : ">";
    draw_text_centered(r, g_app.font_medium, lbl, x + w / 2, y + h / 2, COL_TEXT);
    g_keypad_toggle_hit = (SDL_Rect){x, y, w, h};
}

static void draw_keypad_single(SDL_Renderer *r, int x, int y, int w, int h) {
    int margin = 6;
    int inner_w = w - margin * 2;

    set_color(r, COL_BG_PANEL);
    fill_rounded_rect(r, x, y, w, h, 4);
    set_color(r, COL_BORDER);
    draw_rect(r, x, y, w, h);

    int tab_h = 26;
    int body_y = y + tab_h;
    draw_keypad_collapse_tab(r, x, y, w, tab_h, true);

    set_color(r, COL_BG_WIDGET);
    fill_rect(r, x + 1, body_y + 1, w - 2, 26);
    draw_text(r, g_app.font_medium, "KEYPAD", x + margin, body_y + 6, COL_TEXT, 0);

    int cur_y = body_y + 32;
    const char *mode_label = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? "VOLTS" : "AMPS";
    Color mode_col = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? (Color){100, 200, 100, 255} : (Color){255, 200, 100, 255};
    draw_keypad_btn(r, x + margin, cur_y, inner_w, 26, mode_label, false, BTN_KEY_MODE);
    cur_y += 34;

    int disp_h = 36;
    set_color(r, (Color){10, 15, 12, 255});
    fill_rect(r, x + margin, cur_y, inner_w, disp_h);
    set_color(r, COL_BORDER);
    draw_rect(r, x + margin, cur_y, inner_w, disp_h);

    const char *disp_val = g_app.keypad_value;
    if (strlen(g_app.keypad_value) == 0)
        disp_val = "0.00";
    Color val_col = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? COL_VFD_ON : (Color){255, 200, 100, 255};
    draw_text(r, g_app.font_large, disp_val, x + w - margin - 6, cur_y + 6, val_col, 2);

    const char *unit = (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? "V" : "A";
    draw_text(r, g_app.font_medium, unit, x + margin + 4, cur_y + 8, mode_col, 0);
    cur_y += disp_h + 4;

    draw_text(r, g_app.font_small, (g_app.keypad_mode == KEYPAD_MODE_VOLTAGE) ? "Vset" : "Iset", x + margin, cur_y, COL_TEXT_DIM, 0);
    cur_y += 18;

    int bottom_info_h = 58;
    int avail_h = (y + h) - cur_y - bottom_info_h - margin;
    int btn_rows = 4;
    int btn_gap = 4;
    int btn_h = (avail_h - (btn_rows - 1) * btn_gap) / btn_rows;
    if (btn_h < 32) btn_h = 32;
    if (btn_h > 48) btn_h = 48;

    int btn_cols = 4;
    int btn_w = (inner_w - (btn_cols - 1) * btn_gap) / btn_cols;
    int pad_x = x + margin;

    draw_keypad_btn(r, pad_x + 0 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "7", false, BTN_KEY_7);
    draw_keypad_btn(r, pad_x + 1 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "8", false, BTN_KEY_8);
    draw_keypad_btn(r, pad_x + 2 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "9", false, BTN_KEY_9);
    draw_keypad_btn(r, pad_x + 3 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "C", false, BTN_KEY_CLR);

    cur_y += btn_h + btn_gap;
    draw_keypad_btn(r, pad_x + 0 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "4", false, BTN_KEY_4);
    draw_keypad_btn(r, pad_x + 1 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "5", false, BTN_KEY_5);
    draw_keypad_btn(r, pad_x + 2 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "6", false, BTN_KEY_6);
    draw_keypad_btn(r, pad_x + 3 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "<", false, BTN_KEY_BACK);

    cur_y += btn_h + btn_gap;
    draw_keypad_btn(r, pad_x + 0 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "1", false, BTN_KEY_1);
    draw_keypad_btn(r, pad_x + 1 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "2", false, BTN_KEY_2);
    draw_keypad_btn(r, pad_x + 2 * (btn_w + btn_gap), cur_y, btn_w, btn_h, "3", false, BTN_KEY_3);

    int enter_h = btn_h * 2 + btn_gap;
    draw_keypad_btn(r, pad_x + 3 * (btn_w + btn_gap), cur_y, btn_w, enter_h, "OK", true, BTN_KEY_ENTER);

    cur_y += btn_h + btn_gap;
    int wide_btn = btn_w * 2 + btn_gap;
    draw_keypad_btn(r, pad_x, cur_y, wide_btn, btn_h, "0", false, BTN_KEY_0);
    draw_keypad_btn(r, pad_x + wide_btn + btn_gap, cur_y, btn_w, btn_h, ".", false, BTN_KEY_DOT);

    cur_y += btn_h + 8;
    draw_text(r, g_app.font_small, "SETPOINTS:", x + margin, cur_y, COL_LABEL, 0);
    cur_y += 16;

    char val_str[32];
    psu_status_t *st1 = &g_app.ch[0].status;
    snprintf(val_str, sizeof(val_str), "%.2fV / %.3fA", st1->set_v / 100.0f, st1->set_a / 1000.0f);
    draw_text(r, g_app.font_small, val_str, x + margin, cur_y, COL_SUCCESS, 0);
}

static void draw_keypad_collapsed_strip(SDL_Renderer *r, int x, int y, int w, int h) {
    draw_keypad_collapse_tab(r, x, y, w, h, false);
}

static void render_single(void) {
    SDL_Renderer *r = g_app.renderer;
    set_color(r, COL_BG_DARK);
    SDL_RenderClear(r);
    g_num_buttons = 0;
    g_keypad_toggle_hit.w = 0;

    draw_header_single(r);
    draw_toolbar_single(r);

    int panels_y = HEADER_H + TOOLBAR_H + 4;
    int avail_h = WIN_H - panels_y - 6;
    int start_x = SINGLE_MARGIN_X;
    int keypad_x = start_x + PANEL_W + SINGLE_GAP;

    draw_channel_panel(r, 0, start_x, panels_y);

    if (g_keypad_expanded) {
        draw_keypad_single(r, keypad_x, panels_y, KEYPAD_W, avail_h);
    } else {
        draw_keypad_collapsed_strip(r, keypad_x, panels_y, KEYPAD_COLLAPSED_W, avail_h);
    }

    SDL_RenderPresent(r);
}

static bool try_single_ui_click(int mx, int my) {
    if (g_keypad_toggle_hit.w > 0 && point_in_rect_single(mx, my, &g_keypad_toggle_hit)) {
        g_keypad_expanded = !g_keypad_expanded;
        single_sync_window_size();
        return true;
    }
    return false;
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
    g_app.keypad_channel = 0;
    single_sync_window_size();

    while (g_app.running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: g_app.running = false; break;
                case SDL_MOUSEBUTTONDOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        if (!try_single_ui_click(ev.button.x, ev.button.y))
                            handle_click(ev.button.x, ev.button.y);
                    }
                    g_app.keypad_channel = 0;
                    break;
                case SDL_MOUSEMOTION:
                    g_app.hover_btn = button_at(ev.motion.x, ev.motion.y);
                    break;
                case SDL_KEYDOWN:
                    handle_key(ev.key.keysym.sym);
                    g_app.keypad_channel = 0;
                    break;
                case SDL_TEXTINPUT:
                    handle_text(ev.text.text);
                    break;
            }
        }

        if (g_app.demo_mode) update_demo();
        else update_from_psu();

        g_app.keypad_channel = 0;
        render_single();

        g_app.frame_count++;
        uint32_t now = SDL_GetTicks();
        if (now - g_app.last_fps_time >= 1000) {
            g_app.fps = g_app.frame_count;
            g_app.frame_count = 0;
            g_app.last_fps_time = now;
        }
        SDL_Delay(8);
    }

    cleanup_app();
    return 0;
}
