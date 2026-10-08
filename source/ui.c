// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include "title_logo.h"
#define INK 0x1129
#define PAPER 0xffbc
#define SELECT_BLUE 0x7e7e

static void rect(Game *g, int x, int y, int w, int h, uint16_t c) {
    int cw = g->menu_frame ? 640 : WIDTH, ch = g->menu_frame ? 480 : HEIGHT;
    uint16_t *out = g->menu_frame ? g->menu_pixels : g->pixels;
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if ((unsigned)i < cw && (unsigned)j < ch) out[j * cw + i] = c;
}

static void box(Game *g, int x, int y, int w, int h, uint16_t c) {
    rect(g, x + 3, y, w - 6, h, c);
    rect(g, x, y + 3, w, h - 6, c);
}

static void center(Game *g, int y, const char *s, uint16_t c, int scale) {
    text_draw_scaled(g, ((g->menu_frame ? 640 : WIDTH) - (int)strlen(s) * 6 * scale + scale) / 2,
                     y, s, c, scale);
}

static void roundbox(Game *g, int x, int y, int w, int h, uint16_t c) {
    rect(g, x + 3, y, w - 6, h, c);
    rect(g, x + 1, y + 1, w - 2, h - 2, c);
    rect(g, x, y + 3, w, h - 6, c);
}

static void title_logo(Game *g, int top) {
    unsigned at = 0;
    for (unsigned i = 0; i < sizeof title_logo_runs / sizeof *title_logo_runs; i++) {
        unsigned a = title_logo_runs[i].a;
        for (unsigned j = 0; j < title_logo_runs[i].count; j++, at++) {
            if (!a) continue;
            int x = (640 - TITLE_LOGO_W) / 2 + at % TITLE_LOGO_W, y = top + at / TITLE_LOGO_W;
            if ((unsigned)x >= 640 || (unsigned)y >= 480) continue;
            uint16_t *out = &g->menu_pixels[y * 640 + x];
            unsigned c = *out;
            unsigned r =
                (((c >> 11) * 255 / 31) * (255 - a) + title_logo_runs[i].r * a + 127) / 255;
            unsigned gr =
                ((((c >> 5) & 63) * 255 / 63) * (255 - a) + title_logo_runs[i].g * a + 127) / 255;
            unsigned b = (((c & 31) * 255 / 31) * (255 - a) + title_logo_runs[i].b * a + 127) / 255;
            *out = (r >> 3) << 11 | (gr >> 2) << 5 | (b >> 3);
        }
    }
}

// Leaves the original 256x192 backdrop in g->pixels, so callers can restore from it.
static void backdrop(Game *g, uint16_t *out) {
    game_canvas(g, 0);
    game_background(g, 36, 27, 24);
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++)
            out[y * 640 + x] = g->pixels[(y / 2 % 192) * 256 + x / 2 % 256];
    game_canvas(g, CANVAS_FULLSCREEN);
}

void ui_title(Game *g, int mode, unsigned tick) {
    static uint16_t title[640 * 480];
    static int ready;
    const uint16_t gray = 0x528a;

    if (!ready) {
        backdrop(g, title);
        memcpy(g->menu_pixels, title, sizeof title);
        title_logo(g, 36);
        memcpy(title, g->menu_pixels, sizeof title);
        ready = 1;
    }

    game_canvas(g, CANVAS_FULLSCREEN);
    memcpy(g->menu_pixels, title, sizeof title);
    center(g, 174, "LEFT / RIGHT: CHOOSE GAME", gray, 2);
    for (int i = 0; i < 3; i++) {
        int x = 124 + i * 196;
        game_art(g, 46, 32, 23, game_animation_cell(g, 52, 15 + i, i == mode ? tick : 0), x, 252,
                 1.75f);
        if (i == 0) {
            char score[8];
            snprintf(score, sizeof score, "%03u", g->high[0]);
            rect(g, x + 30, 235, 44, 14, 0xffdf);
            text_draw_scaled(g, x + 34, 235, score, 0, 2);
        }
    }
    game_art(g, 46, 32, 23, game_animation_cell(g, 52, 0, tick), 138 + mode * 196, 319, 1.5f);

    if (mode == 0)
        center(g, 368, "FLY AS FAR AS YOU CAN", gray, 2);
    else if (mode == 1)
        center(g, 368, "BEAT THE CLOCK ON EIGHT COURSES", gray, 2);
    else {
        center(g, 358, "STARTING CONTROLLER IS PLAYER 1", gray, 2);
        center(g, 386, "PLAYER 2 JOINS WITH ANY INPUT", gray, 2);
    }

    center(g, 450, "A / 2: START    HOME / Z: EXIT", gray, 2);
}

// The original Time Attack grid: eight Stage tiles and a Menu tile, at 2x.
void ui_courses(Game *g, int selected, unsigned tick) {
    const uint16_t gray = 0x528a, tile_purple = 0x725f, button_gray = 0x6b4d;
    char s[16];

    backdrop(g, g->menu_pixels);
    center(g, 20, "TIME ATTACK", gray, 2);
    for (int i = 0; i < 9; i++) {
        int x = 160 + i % 3 * 160, y = 104 + i / 3 * 128;
        unsigned t = i < 8 ? g->high[i + 1] : 0;
        if (i == 8) {
            // The original writes the label into this strip at runtime; it overhangs the button.
            game_art(g, 46, 32, 23, 183, x, y, 2);
            for (int yy = y - 16; yy <= y + 16; yy++)
                for (int xx = x - 76; xx <= x + 115; xx++)
                    g->menu_pixels[yy * 640 + xx] =
                        xx <= x + 75 ? button_gray : g->pixels[(yy / 2 % 192) * 256 + xx / 2 % 256];
            text_draw_scaled(g, x - 23, y - 7, "MENU", 0xffdf, 2);
        } else if (t) {
            game_art(g, 46, 32, 23, 175 + i, x, y, 2);
            rect(g, x - 44, y + 7, 96, 20, tile_purple);
            snprintf(s, sizeof s, "%u'%02u\"%02u", t / 3600, t / 60 % 60, t % 60 * 100 / 60);
            text_draw_scaled(g, x + 4 - ((int)strlen(s) * 12 - 2) / 2, y + 10, s, 0, 2);
        } else
            game_art(g, 46, 32, 23, 184 + i, x, y, 2);
    }

    int x = 160 + selected % 3 * 160, y = 104 + selected / 3 * 128;
    game_art(g, 46, 32, 23, game_animation_cell(g, 52, 4, tick), x, y, 2);
    game_art(g, 46, 32, 23, game_animation_cell(g, 52, 0, tick), x - 18, y + 22, 2);
    center(g, 450, "A / 2: START    B: BACK", gray, 2);
}

void ui_setup(Game *g, const char *heading, const char *line1, const char *line2, int busy) {
    const uint16_t ink = 0x2104, paper = 0xff9c, muted = 0x6b2a;
    game_canvas(g, CANVAS_FULLSCREEN);
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++)
            g->menu_pixels[y * 640 + x] = ((x / 8 + y / 8) & 1) ? 0xe71c : 0xef5d;

    title_logo(g, (128 - TITLE_LOGO_H) / 2);
    roundbox(g, 42, 171, 556, 224, ink);
    roundbox(g, 44, 173, 552, 220, paper);
    roundbox(g, 64, 193, 512, 42, SELECT_BLUE);

    center(g, 207, heading, ink, 2);
    center(g, 265, line1, ink, 2);
    center(g, 291, line2, ink, 2);
    center(g, 352, busy ? "PLEASE WAIT" : "HOME / Z TO EXIT", muted, 2);
}

static void menu_canvas(Game *g) {
    if (g->menu_frame == CANVAS_FULLSCREEN) return;
    DisplayRect view =
        display_rect(platform_wide(), platform_pillarbox(), platform_pixel_scale(), 0);
    if (view.x || view.y) {
        static uint16_t scene[640 * 480];
        memcpy(scene, g->menu_pixels, sizeof scene);
        for (int i = 0; i < 640 * 480; i++)
            g->menu_pixels[i] = 0x0843;
        for (int y = 0; y < view.h; y++)
            for (int x = 0; x < view.w; x++)
                g->menu_pixels[(view.y + y) * 640 + view.x + x] =
                    scene[((2 * y + 1) * 480 / (2 * view.h)) * 640
                          + (2 * x + 1) * 640 / (2 * view.w)];
    }
    // Game scaling applies to the backdrop. UI strokes must survive 240p intact.
    game_canvas(g, CANVAS_FULLSCREEN);
}

// Original game palettes cannot produce this exact RGB565 value.
#define CLEAR 0x0020

static void panel(Game *g, const char *heading, int y, int h) {
    menu_canvas(g);
    for (int i = 0; i < 640 * 480; i++) {
        unsigned c = g->menu_pixels[i];
        g->menu_pixels[i] = ((c >> 11) / 3) << 11 | (((c >> 5) & 63) / 3) << 5 | (c & 31) / 3;
    }
    game_canvas(g, 0);
    for (int i = 0; i < WIDTH * HEIGHT; i++)
        g->pixels[i] = CLEAR;

    box(g, 35, y + 4, 188, h, INK);
    box(g, 33, y, 190, h, INK);
    box(g, 34, y + 1, 188, h - 2, PAPER);
    center(g, y + 12, heading, INK, 1);
    rect(g, 44, y + 26, 168, 1, 0xd653);
}

static void panel_compose(Game *g) {
    // Match Bird & Beans: a native 256x192 menu layer, always at Fixed size.
    // Scale the game independently so Fill never stretches the menu's font.
    DisplayRect view = display_rect(platform_wide(), platform_pillarbox(), 1, 0);
    int sample_y = platform_240p() ? 0 : 1;
    for (int y = 0; y < view.h; y++)
        for (int x = 0; x < view.w; x++) {
            uint16_t c = g->pixels[((2 * y + sample_y) * HEIGHT / (2 * view.h)) * WIDTH
                                   + (2 * x + 1) * WIDTH / (2 * view.w)];
            if (c != CLEAR) g->menu_pixels[(view.y + y) * 640 + view.x + x] = c;
        }
    game_canvas(g, CANVAS_FULLSCREEN);
}

void ui_race(Game *g) {
    DisplayRect view =
        display_rect(platform_wide(), platform_pillarbox(), platform_pixel_scale(), 0);
    menu_canvas(g);
    for (int i = 0; i < 2; i++)
        text_draw_scaled(g, view.x + view.w * (1 + i * 2) / 4 - 47, view.y + view.h / 4 - 40,
                         i ? "PLAYER 2" : "PLAYER 1", PAPER, 2);
    if (platform_waiting())
        center(g, view.y + view.h * 3 / 4 + 30, "PLAYER 2: PRESS ANY BUTTON", PAPER, 2);
}

void ui_pause(Game *g, int selected) {
    panel(g, "PAUSED", 25, 144);
    game_art(g, 15, 5, 3, 8, 201, 44, 1);
    if (!platform_connected())
        center(g, 53, g->mode == 2 ? "CONNECT BOTH CONTROLLERS" : "CONNECT A CONTROLLER", INK, 1);
    const char *labels[] = {"CONTINUE", "RESTART", "GRAPHICS OPTIONS", "RETURN TO TITLE"};
    for (int i = 0; i < 4; i++) {
        int y = 65 + i * 22;
        if (i == selected) box(g, 44, y - 5, 168, 18, SELECT_BLUE);
        center(g, y, labels[i], INK, 1);
    }
    center(g, 157, "A / 2 SELECT   B BACK", INK, 1);
    panel_compose(g);
}

void ui_graphics(Game *g, int selected) {
    int wide = platform_wide(), top = wide ? 25 : 36, h = wide ? 144 : 121;
    panel(g, "GRAPHICS OPTIONS", top, h);
    const char *labels[] = {platform_240p() ? "240P MODE: ON" : "240P MODE: OFF",
                            platform_pixel_scale() ? "SCALE: FIXED" : "SCALE: FILL",
                            wide ? (platform_pillarbox() ? "ASPECT: 4:3" : "ASPECT: 16:9") : "BACK",
                            "BACK"};
    int n = wide ? 4 : 3;
    for (int i = 0; i < n; i++) {
        int y = top + 36 + i * 22;
        if (i == selected) box(g, 44, y - 5, 168, 18, SELECT_BLUE);
        center(g, y, labels[i], INK, 1);
    }
    center(g, top + h - 12, "A / 2 CHANGE   B BACK", INK, 1);
    panel_compose(g);
}

void ui_over(Game *g) {
    panel(g, g->finished ? "COURSE COMPLETE" : "GAME OVER", 49, 94);
    char s[80];
    if (g->mode == 0)
        snprintf(s, sizeof s, "SCORE %03u   BEST %03u", game_score(g), g->high[0]);
    else if (g->mode == 1)
        snprintf(s, sizeof s, "COURSE %d   TIME %u.%02u", g->course + 1, g->elapsed / 60,
                 (g->elapsed % 60) * 100 / 60);
    else
        snprintf(s, sizeof s, "PLAYER %d WINS", g->winner);
    center(g, 85, s, INK, 1);
    center(g, 111, "A / 2 PLAY AGAIN", INK, 1);
    center(g, 129, "HOME / Z RETURN TO TITLE", INK, 1);
    panel_compose(g);
}

void ui_error(Game *g, const char *message) {
    panel(g, "CORE ERROR", 25, 144);
    for (int y = 65; *message && y < 145; y += 10) {
        char line[29];
        size_t n = strlen(message);
        if (n > 28) n = 28;
        if (message[n])
            for (size_t i = n; i > 0; i--)
                if (message[i] == ' ') {
                    n = i;
                    break;
                }
        memcpy(line, message, n);
        line[n] = 0;
        center(g, y, line, INK, 1);
        message += n;
        while (*message == ' ')
            message++;
    }
    center(g, 157, "HOME / Z RETURN TO TITLE", INK, 1);
    panel_compose(g);
}

void ui_save_warning(Game *g) {
    menu_canvas(g);
    box(g, 8, 440, 208, 28, INK);
    text_draw_scaled(g, 16, 448, "SAVE FAILED", PAPER, 2);
}
