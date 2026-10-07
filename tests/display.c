// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include <assert.h>
#include <stdio.h>

static Game g;
static int waiting;

int platform_wide(void) {
    return 0;
}

int platform_pillarbox(void) {
    return 0;
}

int platform_pixel_scale(void) {
    return 2;
}

int platform_240p(void) {
    return 0;
}

int platform_connected(void) {
    return 1;
}

int platform_waiting(void) {
    return waiting;
}

static void capture(const char *folder, const char *name) {
    if (!folder) return;
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.ppm", folder, name);
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n640 480\n255\n");
    for (int i = 0; i < 640 * 480; i++) {
        unsigned c = g.menu_pixels[i];
        unsigned char rgb[] = {(c >> 11) * 255 / 31, ((c >> 5) & 63) * 255 / 63,
                               (c & 31) * 255 / 31};
        assert(fwrite(rgb, 1, 3, f) == 3);
    }
    assert(!fclose(f));
}

int main(int argc, char **argv) {
    assert(argc >= 2 && game_load(&g, argv[1]));
    const char *folder = argc > 2 ? argv[2] : NULL;
    for (int mode = 0; mode < 3; mode++) {
        assert(game_start(&g, mode, 1234));
        for (int i = 0; i < 20; i++)
            assert(game_tick(&g, 0));
        game_render(&g);
        // Sample the actual composition at GX's 4:3 Fixed pixel centers.
        for (int y = 0; y < (mode == 2 ? 192 : 384); y++)
            for (int x = 0; x < (mode == 2 ? 512 : 256); x++) {
                int vx = mode == 2 ? x : 128 + x;
                int vy = mode == 2 ? 96 + y : y;
                int cx = (2 * vx + 1) * 640 / (2 * 512);
                int cy = (2 * vy + 1) * 480 / (2 * 384);
                int sx = x % 256;
                int screen = mode == 2 ? 1 - x / 256 : 1 - y / 192;
                unsigned expected = mode == 2 && (sx < 24 || sx >= 232)
                                        ? 0x0843
                                        : g.screens[screen][(y % 192) * 256 + sx];
                assert(g.menu_pixels[cy * 640 + cx] == expected);
            }
        if (mode == 2) {
            ui_race(&g);
            for (int y = 360; y < 480; y++)
                for (int x = 0; x < 640; x++)
                    assert(g.menu_pixels[y * 640 + x] == 0x0843);
            waiting = 1;
            game_render(&g);
            ui_race(&g);
            int prompt = 0;
            for (int y = 360; y < 400; y++)
                for (int x = 0; x < 640; x++)
                    prompt |= g.menu_pixels[y * 640 + x] != 0x0843;
            assert(prompt);
            waiting = 0;
        }
        game_render(&g);
        ui_pause(&g, 0);
        for (int y = 0; y < HEIGHT; y++)
            for (int x = 0; x < WIDTH; x++)
                if (g.pixels[y * WIDTH + x] != 0x0020)
                    for (int dy = 0; dy < 2; dy++)
                        for (int dx = 0; dx < 2; dx++)
                            assert(g.menu_pixels[(48 + 2 * y + dy) * 640 + 64 + 2 * x + dx]
                                   == g.pixels[y * WIDTH + x]);
    }
    assert(game_start(&g, 0, 1234));
    for (int i = 0; i < 20; i++)
        assert(game_tick(&g, 0));
    ui_title(&g, 0, 0);
    capture(folder, "title");
    game_render(&g);
    ui_pause(&g, 0);
    capture(folder, "pause");
    game_render(&g);
    ui_graphics(&g, 0);
    capture(folder, "graphics");
    game_free(&g);
    puts(
        "PASS display: native 1x Fixed gameplay in all modes, 2x menu pixels, Race hint removed and join prompt retained");
}
