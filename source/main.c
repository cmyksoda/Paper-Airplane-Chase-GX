// SPDX-License-Identifier: GPL-3.0-only
#include "ui.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

static Game game;
static Audio sound;
static char savepath[1024];
static int save_failed;
static unsigned saved_video;

static unsigned video_flags(void) {
    return (platform_240p() ? 1 : 0) | (platform_pixel_scale() ? 2 : 0)
           | (platform_pillarbox() ? 4 : 0);
}

static void highs_path(const char *pak) {
    const char *slash = strrchr(pak, '/');
    int n = snprintf(savepath, sizeof savepath, "%.*sscores.dat",
                     slash ? (int)(slash - pak + 1) : 0, pak);
    if (n < 0 || n >= (int)sizeof savepath) {
        savepath[0] = 0;
        save_failed = 1;
    }
}

static int highs_directory(void) {
    if (!savepath[0]) return 0;

    char path[sizeof savepath];
    strcpy(path, savepath);

    // Wiiload supplies only the executable. Its save directory may not exist.
    for (char *p = path; *p; p++)
        if (*p == '/' && p != path && p[-1] != ':') {
            *p = 0;
            int ok = mkdir(path, 0777) == 0 || errno == EEXIST;
            *p = '/';
            if (!ok) return 0;
        }

    return 1;
}

static void highs_read(void) {
    unsigned char b[44];
    saved_video = video_flags();
    FILE *f = fopen(savepath, "rb");
    // Preserve records from older embedded launches when adopting the shared path.
    if (!f && !strcmp(savepath, "sd:/apps/paperplane/scores.dat"))
        f = fopen("sd:/apps/paperplane-full/scores.dat", "rb");
    if (!f) return;

    size_t len = fread(b, 1, sizeof b, f);
    fclose(f);
    if (len < 40 || memcmp(b, "PAS1", 4)) return;

    for (int i = 0; i < 9; i++) {
        unsigned n = le32(b + 4 + i * 4);
        if (n <= (i ? 360000 : 999)) game.high[i] = n;
    }

    // Older PAS1 files contain only scores; incomplete/invalid video flags are ignored.
    if (len == sizeof b && b[40] <= 7 && !b[41] && !b[42] && !b[43]) {
        unsigned changed = video_flags() ^ b[40];
        if (changed & 1) platform_toggle_240p();
        if (changed & 2) platform_scale();
        if (changed & 4) platform_aspect();
    }
    saved_video = video_flags();
}

static void highs_write(void) {
    if (!highs_directory()) {
        save_failed = 1;
        return;
    }

    // Keep the original 40-byte score prefix readable by older builds.
    unsigned char b[44] = {'P', 'A', 'S', '1'};
    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 4; j++)
            b[4 + i * 4 + j] = game.high[i] >> (j * 8);
    b[40] = saved_video;

    char tmp[1040], bak[1040];
    snprintf(tmp, sizeof tmp, "%s.tmp", savepath);
    snprintf(bak, sizeof bak, "%s.bak", savepath);

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        save_failed = 1;
        return;
    }
    int ok = fwrite(b, 1, sizeof b, f) == sizeof b;
    if (fclose(f)) ok = 0;
    if (!ok) {
        save_failed = 1;
        return;
    }

    if (!rename(tmp, savepath)) {
        save_failed = 0;
        return;
    }
    remove(bak);
    if (rename(savepath, bak)) {
        save_failed = 1;
        return;
    }
    if (rename(tmp, savepath)) {
        rename(bak, savepath);
        save_failed = 1;
        return;
    }
    remove(bak);
    save_failed = 0;
}

static void video_save(int graphics) {
    // Confirm on leaving Graphics Options, so quitting during a blank 240p trial
    // does not save a video mode the display cannot show.
    if (!graphics && video_flags() != saved_video) {
        saved_video = video_flags();
        highs_write();
    }
}

static void start(int mode, int deterministic) {
    audio_stop(&sound);
    platform_audio_reset(&sound);
    platform_multiplayer(mode == 2);
    game_start(&game, mode, deterministic ? 0x12345678 : (uint32_t)time(NULL));
    audio_commands(&sound, &game);
}

static void title_music(void) {
    platform_multiplayer(0);
    audio_stop(&sound);
    audio_play(&sound, 0, 0);
}

int main(int argc, char **argv) {
    if (!platform_init()) return 1;

    const char *pak = "game.pak";
#ifdef HW_RVL
    pak = "sd:/apps/paperplane/game.pak";
#endif

    int smoke = 0, smoke_mode = 0, smoke_frames = 600;
    const char *capture = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--smoke"))
            smoke = 1;
        else if (!strcmp(argv[i], "--capture") && i + 1 < argc)
            capture = argv[++i];
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc)
            smoke_mode = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            smoke_frames = atoi(argv[++i]);
        else
            pak = argv[i];
    }

#ifdef HW_RVL
    char path[1024];
    snprintf(path, sizeof(path), "%s", argc ? argv[0] : "");
    char *boot_slash = strrchr(path, '/');
    if (boot_slash) {
        boot_slash[1] = 0;
        strncat(path, "game.pak", sizeof(path) - strlen(path) - 1);
        pak = path;
    }
#endif

    highs_path(pak);
    int import_status = ROM_IMPORT_OK;
#ifdef EMBEDDED_GAME
    extern const unsigned char game_data[], game_data_end[];
    int loaded = game_load_memory(&game, game_data, game_data_end - game_data);
#else
    int loaded = game_load(&game, pak);
    if (!loaded) {
        game_free(&game);
        ui_setup(&game, "PREPARING YOUR GAME", "FIRST LAUNCH MAY TAKE A MOMENT.",
                 "KEEP THE SD CARD INSERTED.", 1);
        platform_present(&game);
        import_status = rom_import(pak);
        if (import_status == ROM_IMPORT_OK) loaded = game_load(&game, pak);
    }
#endif

    if (!loaded || !audio_init(&sound, game.blobs[66])) {
        fprintf(stderr, "PaperPlane import=%d: %s\n", import_status, game.vm.error);
        const char *heading = "COULD NOT READ GAME DATA",
                   *line1 = "REMOVE GAME.PAK TO REBUILD IT.",
                   *line2 = "KEEP YOUR ROM BESIDE BOOT.DOL.";

        if (import_status == ROM_IMPORT_MISSING) {
            heading = "ROM NEEDED";
            line1 = "PUT YOUR USA .NDS ROM";
            line2 = "BESIDE BOOT.DOL, THEN RELAUNCH.";
        } else if (import_status == ROM_IMPORT_UNSUPPORTED) {
            heading = "UNSUPPORTED ROM";
            line1 = "USE THE UNMODIFIED USA RELEASE.";
            line2 = "OTHER REGIONS ARE NOT SUPPORTED.";
        } else if (import_status == ROM_IMPORT_IO) {
            heading = "COULD NOT PREPARE GAME";
            line1 = "CHECK THAT THE SD CARD IS WRITABLE.";
            line2 = "THEN RELAUNCH THE APP.";
        } else if (import_status == ROM_IMPORT_NOMEM) {
            heading = "NOT ENOUGH MEMORY";
            line1 = "RESTART THE APP AND TRY AGAIN.";
            line2 = "";
        }

        ui_setup(&game, heading, line1, line2, 0);
        if (capture) platform_capture(capture, &game);
        do {
            platform_present(&game);
        } while (!(platform_input() & (KEY_MENU | KEY_QUIT)) && !smoke);
        game_free(&game);
        platform_close();
        audio_free(&sound);
        return 1;
    }

    platform_audio_reset(&sound);
    highs_read();

    int mode = smoke ? smoke_mode : 0, menu = !smoke, pause = 0, selection = 0, graphics = 0,
        graphics_selection = 0, was_dead = 0;
    unsigned prev = 0, total = 0, uiticks = 0;
    start(mode, smoke);
    if (menu) title_music();
    double last = platform_seconds(), acc = 0, uiacc = 0;
    const double step = 1.0 / 59.8261;
    fprintf(stderr, "PaperPlane ready: mode %d\n", mode);

    while (1) {
        unsigned held = platform_input();
        if (held & KEY_CONTROLLER_CHANGED) {
            prev = 0;
            game.previous = 0;
        }
        unsigned down = held & ~prev;
        prev = held;
        if (held & KEY_QUIT) break;

        double now = platform_seconds(), dt = now - last;
        last = now;
        if (dt > 0.1) dt = 0.1;
        if (dt < 0) dt = 0;
        acc += dt;
        uiacc += dt;
        while (uiacc >= step) {
            uiticks++;
            uiacc -= step;
        }

        if (menu) {
            if (down & KEY_MENU) break;
            if (down & KEY_LEFT) {
                mode = (mode + 2) % 3;
                audio_play(&sound, AUDIO_UI_PLAYER, 6);
            }
            if (down & KEY_RIGHT) {
                mode = (mode + 1) % 3;
                audio_play(&sound, AUDIO_UI_PLAYER, 6);
            }
            if (mode == 1 && (down & (KEY_UP | KEY_DOWN))) {
                game.course = (game.course + (down & KEY_DOWN ? 1 : 7)) % 8;
                audio_play(&sound, AUDIO_UI_PLAYER, 6);
            }
            if ((down & (KEY_ACTION | KEY_PAUSE)) && !(down & KEY_BACK)) {
                start(mode, 0);
                menu = pause = graphics = was_dead = 0;
                acc = 0;
            }
        } else if (pause) {
            if (graphics) {
                int rows = platform_wide() ? 4 : 3;
                if (graphics_selection >= rows) graphics_selection = rows - 1;

                if (down & KEY_UP) {
                    graphics_selection = (graphics_selection + rows - 1) % rows;
                    audio_play(&sound, AUDIO_UI_PLAYER, 6);
                }
                if (down & KEY_DOWN) {
                    graphics_selection = (graphics_selection + 1) % rows;
                    audio_play(&sound, AUDIO_UI_PLAYER, 6);
                }

                if (down & (KEY_PAUSE | KEY_MENU)) {
                    graphics = pause = 0;
                    audio_play(&sound, AUDIO_UI_PLAYER, 5);
                    acc = 0;
                } else if (down & KEY_BACK)
                    graphics = 0;
                else if (down & (KEY_ACTION | KEY_LEFT | KEY_RIGHT)) {
                    if (graphics_selection == 0)
                        platform_toggle_240p();
                    else if (graphics_selection == 1)
                        platform_scale();
                    else if (platform_wide() && graphics_selection == 2)
                        platform_aspect();
                    else if (down & KEY_ACTION)
                        graphics = 0;
                    audio_play(&sound, AUDIO_UI_PLAYER, 6);
                }
            } else {
                if (down & KEY_UP) {
                    selection = (selection + 3) % 4;
                    audio_play(&sound, AUDIO_UI_PLAYER, 6);
                }
                if (down & KEY_DOWN) {
                    selection = (selection + 1) % 4;
                    audio_play(&sound, AUDIO_UI_PLAYER, 6);
                }

                if (down & (KEY_PAUSE | KEY_BACK | KEY_MENU)) {
                    pause = 0;
                    audio_play(&sound, AUDIO_UI_PLAYER, 5);
                    acc = 0;
                } else if (down & KEY_ACTION) {
                    if (selection == 0) {
                        pause = 0;
                        audio_play(&sound, AUDIO_UI_PLAYER, 5);
                        acc = 0;
                    } else if (selection == 1) {
                        highs_write();
                        start(mode, 0);
                        pause = was_dead = 0;
                        acc = 0;
                    } else if (selection == 2) {
                        graphics = 1;
                        graphics_selection = 0;
                    } else {
                        highs_write();
                        menu = 1;
                        pause = 0;
                        title_music();
                    }
                }
            }
        } else if (game.dead || game.vm.fault) {
            if (down & KEY_MENU) {
                highs_write();
                menu = 1;
                title_music();
            } else if (down & KEY_ACTION) {
                highs_write();
                start(mode, 0);
                was_dead = 0;
                acc = 0;
            }
        } else if ((down & (KEY_PAUSE | KEY_MENU)) || !platform_connected()) {
            pause = 1;
            selection = graphics = 0;
            audio_play(&sound, AUDIO_UI_PLAYER, 4);
        }

        video_save(graphics);

        if (smoke) {
            held = ((total / 180) % 2 ? 32 : 16);
            acc = step;
            pause = 0;
        }

        if (!menu && !pause && !game.dead && !game.vm.fault && !platform_waiting()) {
            while (acc >= step) {
                unsigned input = held & 48;
                if (mode == 2) {
                    unsigned p2 = platform_input2();
                    if (p2 & KEY_LEFT) input |= 2048;
                    if (p2 & KEY_RIGHT) input |= 1;
                }
                if (!game_tick(&game, input)) break;
                audio_commands(&sound, &game);
                acc -= step;
                total++;
            }
        } else {
            acc = 0;
            if (smoke) total++;
        }

        if (game.dead && !was_dead) {
            highs_write();
            was_dead = 1;
        }

        audio_pause(&sound, pause || game.vm.fault || platform_waiting());
        platform_audio(&sound);
        audio_commands(&sound, &game);

        if (menu)
            ui_title(&game, mode, uiticks);
        else {
            if (platform_240p()) {
                game_render_native(&game);
                game_compose_240p(&game, platform_wide(), platform_pillarbox(),
                                  platform_pixel_scale());
            } else {
                game_render(&game);
            }
            if (mode == 2) ui_race(&game);
            if (game.vm.fault)
                ui_error(&game, game.vm.error);
            else if (pause) {
                if (graphics)
                    ui_graphics(&game, graphics_selection);
                else
                    ui_pause(&game, selection);
            } else if (game.dead)
                ui_over(&game);
        }

        if (save_failed) ui_save_warning(&game);
        platform_present(&game);
        if (smoke && (total >= (unsigned)smoke_frames || game.vm.fault)) {
            if (capture) platform_capture(capture, &game);
            printf("SMOKE frames=%u score=%u instructions=%u fault=%d %s\n", total,
                   game_score(&game), game.vm.instructions, game.vm.fault, game.vm.error);
            break;
        }
    }

    if (!smoke && !menu) highs_write();
    int result = game.vm.fault ? 1 : 0;
    platform_close();
    audio_free(&sound);
    game_free(&game);
    return result;
}
