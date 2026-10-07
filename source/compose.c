// SPDX-License-Identifier: GPL-3.0-only
#include "game.h"
#include "display.h"
#include <string.h>

static unsigned min_u(unsigned a, unsigned b) { return a < b ? a : b; }
static unsigned max_u(unsigned a, unsigned b) { return a > b ? a : b; }

// Each destination row integrates its whole source footprint. Selecting just one
// row makes scrolling one-pixel ledges alternate between white and invisible.
static void area_view(Game *g, int screen, int x, int y, int w, int h) {
    unsigned sh = screen < 0 ? 2 * HEIGHT : HEIGHT;
    uint16_t row[WIDTH];
    for (int dy = 0; dy < h; dy++) {
        unsigned top = dy * sh, bottom = (dy + 1) * sh;
        for (int sx = 0; sx < WIDTH; sx++) {
            unsigned r = 0, gr = 0, b = 0;
            for (unsigned sy = top / h; sy <= (bottom - 1) / h; sy++) {
                unsigned weight = min_u(bottom, (sy + 1) * h) - max_u(top, sy * h);
                unsigned c = screen < 0 ? g->screens[1 - sy / HEIGHT][(sy % HEIGHT) * WIDTH + sx]
                                        : g->screens[screen][sy * WIDTH + sx];
                if (screen >= 0 && (sx < 24 || sx >= 232)) c = 0x0843;
                r += (c >> 11) * weight;
                gr += ((c >> 5) & 63) * weight;
                b += (c & 31) * weight;
            }
            row[sx] = ((r + sh / 2) / sh) << 11 | ((gr + sh / 2) / sh) << 5
                      | (b + sh / 2) / sh;
        }
        uint16_t *out = g->menu_pixels + (y + dy) * 2 * 640 + x;
        for (int dx = 0; dx < w; dx++) {
            if (w >= WIDTH) {
                out[dx] = row[(2 * dx + 1) * WIDTH / (2 * w)];
            } else {
                unsigned left = dx * WIDTH, right = (dx + 1) * WIDTH;
                unsigned r = 0, gr = 0, b = 0;
                for (unsigned sx = left / w; sx <= (right - 1) / w; sx++) {
                    unsigned weight = min_u(right, (sx + 1) * w) - max_u(left, sx * w);
                    unsigned c = row[sx];
                    r += (c >> 11) * weight;
                    gr += ((c >> 5) & 63) * weight;
                    b += (c & 31) * weight;
                }
                out[dx] = ((r + WIDTH / 2) / WIDTH) << 11
                          | ((gr + WIDTH / 2) / WIDTH) << 5 | (b + WIDTH / 2) / WIDTH;
            }
        }
        // UI uses 480-line coordinates; paired rows preserve this finished 240p image.
        memcpy(out + 640, out, w * sizeof *out);
    }
}

void game_compose_240p(Game *g, int wide, int pillarbox, int scale) {
    DisplayRect view = display_rect(wide, pillarbox, scale, 0);
    game_canvas(g, CANVAS_FULLSCREEN);
    for (int i = 0; i < 640 * 480; i++) g->menu_pixels[i] = 0x0843;
    if (g->mode < 2) {
        area_view(g, -1, view.x + view.w / 4, view.y / 2, view.w / 2, view.h / 2);
    } else {
        for (int player = 0; player < 2; player++)
            area_view(g, 1 - player, view.x + player * view.w / 2,
                      view.y / 2 + view.h / 8, view.w / 2, view.h / 4);
    }
}

void game_scanout_240p(const uint16_t *src, uint16_t *out) {
    for (int y = 0; y < 240; y++)
        for (int x = 0; x < 640; x++) {
            unsigned a = src[(2 * y) * 640 + x], b = src[(2 * y + 1) * 640 + x];
            // Resolve UI/logo rows once, rather than let GX discard every other row.
            out[y * 640 + x] = (((a >> 11) + (b >> 11) + 1) / 2) << 11
                              | ((((a >> 5) & 63) + ((b >> 5) & 63) + 1) / 2) << 5
                              | ((a & 31) + (b & 31) + 1) / 2;
        }
}
