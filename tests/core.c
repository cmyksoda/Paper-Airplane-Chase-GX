// SPDX-License-Identifier: GPL-3.0-only
// Goal injection checks transitions; the final input replay completes a real course.
#include "game.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static Game g;

static unsigned rd(unsigned a) {
    return vm_read(&g.vm, a, 4);
}

static void wr(unsigned a, unsigned b) {
    vm_write(&g.vm, a, b, 4);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s game.pak [ROM native.pak]\n", argv[0]);
        return 2;
    }
    if (argc == 4) assert(rom_extract(argv[2], argv[3]) == ROM_IMPORT_OK);
    assert(game_load(&g, argv[1]));

    for (int mode = 0; mode < 3; mode++)
        for (int course = 0; course < (mode == 1 ? 8 : 1); course++)
            for (int run = 0; run < 4; run++) {
                g.course = course;
                if (!game_start(&g, mode, 1234 + run)) {
                    fprintf(stderr, "mode=%d course=%d start: %s\n", mode, course, g.vm.error);
                    return 1;
                }
                for (int i = 0; i < 1500 && !g.dead; i++) {
                    if (run == 3 && i == 20 && mode == 1)
                        wr(SCENE + 0xc4, rd(SCENE + 0x370) + 4096);
                    if (run >= 2 && i == 200 && mode == 2) {
                        int p = run - 2;
                        wr(SCENE + 0xc4 + p * 0x58,
                           rd(rd(SCENE + 0x160) + p * 0x617c + 0x474) + 4096);
                    }
                    assert(game_tick(
                        &g, run == 0 ? 0 : (i / 33 % 2 ? 32 : 16) | (i / 25 % 2 ? 2048 : 1)));
                    if (!(i % 31)) game_render(&g);
                    if (g.vm.fault) {
                        puts(g.vm.error);
                        return 1;
                    }
                }

                if (run == 3 && mode == 1) {
                    assert(g.finished);
                    assert(g.high[course + 1]);
                    assert(g.dead);
                }
                if (run >= 2 && mode == 2) {
                    assert(g.finished);
                    assert(g.winner == run - 1);
                    assert(g.dead);
                }
                printf("mode=%d course=%d run=%d frame=%u score=%u finish=%d winner=%d dead=%d high=%u\n",
                       mode, course, run, g.frame, game_score(&g), g.finished, g.winner, g.dead,
                       g.high[course + 1]);
            }

    g.course = 0;
    assert(game_start(&g, 1, 0x12345678));
    FILE *replay = fopen("tests/course1-inputs.txt", "r");
    assert(replay);
    int c;
    while ((c = fgetc(replay)) != EOF) {
        if (c != 'L' && c != 'R' && c != '-') continue;
        assert(game_tick(&g, c == 'L' ? 32 : c == 'R' ? 16 : 0));
    }
    fclose(replay);
    assert(g.finished && g.dead && g.elapsed == 1044 && !g.vm.fault);

    unsigned char *damaged = malloc(g.pack_size);
    assert(damaged);
    memcpy(damaged, g.pack, g.pack_size);
    damaged[g.pack_size - 1] ^= 1;
    Game *bad = calloc(1, sizeof *bad);
    assert(bad);
    assert(!game_load_memory(bad, damaged, g.pack_size));
    game_free(bad);
    free(bad);
    free(damaged);

    game_free(&g);
    puts("PASS core: all courses, restart, crashes, injected goals, both race winners, natural finish and cache CRC");
}
