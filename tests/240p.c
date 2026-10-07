// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Game g;
static uint16_t low[640 * 240];
static int wide, pillarbox, scale;
int platform_wide(void) { return wide; }
int platform_pillarbox(void) { return pillarbox; }
int platform_pixel_scale(void) { return scale; }
int platform_240p(void) { return 1; }
int platform_connected(void) { return 1; }
int platform_waiting(void) { return 0; }

static void capture(const char *folder, const char *name, int mode) {
    if (!folder) return;
    char path[1024];
    snprintf(path, sizeof path, "%s/%s-%d.ppm", folder, name, mode);
    FILE *f = fopen(path, "wb");
    assert(f);
    game_scanout_240p(g.menu_pixels, low);
    // Repeat scanlines only for a 4:3 preview of the 640x240 scanout.
    fprintf(f, "P6\n640 480\n255\n");
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++) {
            unsigned c = low[(y / 2) * 640 + x];
            unsigned char rgb[] = {(c >> 11) * 255 / 31,
                                   ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31};
            assert(fwrite(rgb, 1, 3, f) == 3);
        }
    assert(!fclose(f));
}

static void moving_line(int mode) {
    g.mode = mode;
    int sh = mode == 2 ? HEIGHT : 2 * HEIGHT;
    DisplayRect v = display_rect(wide, pillarbox, scale, 0);
    int height = mode == 2 ? v.h / 4 : v.h / 2;
    int top = mode == 2 ? v.y / 2 + v.h / 8 : v.y / 2;
    int x = mode == 2 ? v.x + v.w / 4 : v.x + v.w / 2;
    unsigned min_energy = 1000, max_energy = 0;
    memset(g.screens, 0, sizeof g.screens);
    for (int row = 0; row < sh; row++) {
        int screen = mode == 2 ? 1 : 1 - row / HEIGHT;
        for (int sx = 0; sx < WIDTH; sx++) g.screens[screen][row % HEIGHT * WIDTH + sx] = 0xffff;
        game_compose_240p(&g, wide, pillarbox, scale);
        game_scanout_240p(g.menu_pixels, low);
        unsigned energy = 0;
        for (int y = top; y < top + height; y++) energy += low[y * 640 + x] >> 11;
        assert(energy > 0); // Every input row remains visible at every scrolling phase.
        if (energy < min_energy) min_energy = energy;
        if (energy > max_energy) max_energy = energy;
        for (int sx = 0; sx < WIDTH; sx++) g.screens[screen][row % HEIGHT * WIDTH + sx] = 0;
    }
    assert(max_energy - min_energy <= 2); // Only RGB565 quantization may vary total brightness.
    printf("PASS moving ledge mode=%d wide=%d pillarbox=%d fixed=%d: energy %u..%u, no lost rows\n",
           mode, wide, pillarbox, scale, min_energy, max_energy);
}

int main(int argc, char **argv) {
    for (wide = 0; wide < 2; wide++)
        for (pillarbox = 0; pillarbox <= wide; pillarbox++)
            for (scale = 0; scale < 2; scale++) {
                moving_line(0);
                moving_line(2);
            }
    assert(argc > 1 && game_load(&g, argv[1]));
    const char *folder = argc > 2 ? argv[2] : NULL;
    wide = pillarbox = 0;
    scale = 1;
    for (int mode = 0; mode < 3; mode++) {
        assert(game_start(&g, mode, 1234));
        for (int i = 0; i < 20; i++) assert(game_tick(&g, 0));
        game_render_native(&g);
        game_compose_240p(&g, 0, 0, 1);
        if (mode == 2) ui_race(&g);
        capture(folder, "gameplay-fixed", mode);
        ui_pause(&g, 0);
        capture(folder, "pause", mode);
        game_scanout_240p(g.menu_pixels, low);
        for (int y = 0; y < HEIGHT; y++)
            for (int x = 0; x < WIDTH; x++)
                if (g.pixels[y * WIDTH + x] != 0x0020)
                    for (int dx = 0; dx < 2; dx++)
                        assert(low[(24 + y) * 640 + 64 + 2 * x + dx] == g.pixels[y * WIDTH + x]);
        scale = 0;
        game_compose_240p(&g, 0, 0, 0);
        if (mode == 2) ui_race(&g);
        capture(folder, "gameplay-fill", mode);
        ui_title(&g, mode, 0);
        capture(folder, "title", mode);
        scale = 1;
    }
    game_free(&g);
    puts("PASS native 240-line scanout, complete menu glyphs");
}
