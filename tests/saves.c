// SPDX-License-Identifier: GPL-3.0-only
// Exercise the app's actual persistence code without launching video or audio.
#define main paperplane_main
#include "../source/main.c"
#undef main
#include <assert.h>
#include <unistd.h>

static unsigned current_video;
static int widescreen = 1;

int platform_240p(void) { return current_video & 1; }
int platform_pixel_scale(void) { return current_video & 2; }
int platform_pillarbox(void) { return widescreen && (current_video & 4); }
void platform_toggle_240p(void) { current_video ^= 1; }
void platform_scale(void) { current_video ^= 2; }
void platform_aspect(void) {
    if (widescreen) current_video ^= 4;
}

static size_t read_save(unsigned char *b) {
    FILE *f = fopen(savepath, "rb");
    assert(f);
    size_t n = fread(b, 1, 44, f);
    assert(fgetc(f) == EOF && !fclose(f));
    return n;
}

static void write_fixture(const unsigned char *b, size_t n) {
    FILE *f = fopen(savepath, "wb");
    assert(f && fwrite(b, 1, n, f) == n && !fclose(f));
}

static void graphics_checks(void) {
    unsigned char b[44], before[44];
    for (unsigned flags = 0; flags < 8; flags++) {
        assert(read_save(before) == 44);
        current_video = flags;
        video_save(1);
        assert(read_save(b) == 44 && !memcmp(b, before, 44));
        video_save(0);
        assert(!save_failed && read_save(b) == 44 && b[40] == flags);
        assert(!memcmp(b, before, 40));

        current_video = saved_video = 0;
        memset(game.high, 0, sizeof game.high);
        highs_read();
        assert(video_flags() == flags && saved_video == flags && game.high[0] == 777);
        highs_read();
        assert(video_flags() == flags); // Re-reading must not invert restored settings.
    }

    // Scores saved during an unconfirmed preview retain the last accepted settings.
    current_video = 0;
    highs_write();
    assert(read_save(b) == 44 && b[40] == 7);
    video_save(0);
    assert(read_save(b) == 44 && b[40] == 0);

    assert(mkdir("sd:/apps/paperplane/scores.dat.tmp", 0777) == 0);
    current_video = 7;
    video_save(0);
    assert(save_failed && read_save(b) == 44 && b[40] == 0);
    assert(rmdir("sd:/apps/paperplane/scores.dat.tmp") == 0);
    highs_write();
    assert(!save_failed && read_save(b) == 44 && b[40] == 7);

    memcpy(before, b, 44);
    for (size_t len = 40; len < 44; len++) {
        write_fixture(before, len);
        current_video = saved_video = 0;
        game.high[0] = 0;
        highs_read();
        assert(!video_flags() && !saved_video && game.high[0] == 777);
    }
    write_fixture(before, 40);
    current_video = 7;
    video_save(0);
    assert(read_save(b) == 44 && !memcmp(b, before, 40) && b[40] == 7);

    for (int i = 40; i < 44; i++) {
        memcpy(b, before, 44);
        b[i] = 255;
        write_fixture(b, 44);
        current_video = saved_video = 0;
        highs_read();
        assert(!video_flags() && !saved_video && game.high[0] == 777);
    }

    write_fixture(before, 44);
    widescreen = 0;
    highs_read();
    assert(video_flags() == 3 && saved_video == 3); // Aspect applies only on widescreen.
    widescreen = 1;
    puts("PASS graphics: all 8 combinations, menu confirmation, repeat restore, legacy/truncated/invalid extensions, score preservation and write-failure retry");
}

int main(void) {
    char root[] = "/tmp/paperplane-saves-XXXXXX";
    assert(mkdtemp(root) && chdir(root) == 0);

    highs_path("sd:/apps/paperplane/game.pak");
    assert(!strcmp(savepath, "sd:/apps/paperplane/scores.dat"));
    game.high[0] = 321;
    game.high[1] = 1234;
    highs_write();
    assert(save_failed && access("sd:", F_OK) != 0);
    assert(mkdir("sd:", 0777) == 0);
    highs_path("sd:/apps/paperplane-full/game.pak");
    game.high[0] = 222;
    highs_write();
    assert(!save_failed);
    highs_path("sd:/apps/paperplane/game.pak");
    memset(game.high, 0, sizeof game.high);
    highs_read();
    assert(game.high[0] == 222);
    game.high[0] = 321;
    game.high[1] = 1234;
    highs_write();
    assert(!save_failed);
    memset(game.high, 0, sizeof game.high);
    highs_read();
    assert(game.high[0] == 321 && game.high[1] == 1234);

    game.high[0] = 456;
    highs_write();
    assert(!save_failed);
    assert(mkdir("sd:/apps/paperplane/scores.dat.tmp", 0777) == 0);
    game.high[0] = 999;
    highs_write();
    assert(save_failed);
    memset(game.high, 0, sizeof game.high);
    highs_read();
    assert(game.high[0] == 456);
    assert(rmdir("sd:/apps/paperplane/scores.dat.tmp") == 0);
    game.high[0] = 777;
    highs_write();
    assert(!save_failed);

    highs_path("sd:/apps/custom/game.pak");
    game.high[0] = 123;
    highs_write();
    assert(!save_failed);
    memset(game.high, 0, sizeof game.high);
    highs_read();
    assert(game.high[0] == 123);
    highs_path("sd:/apps/paperplane/game.pak");
    highs_read();
    assert(game.high[0] == 777);
    graphics_checks();

    char longpath[1100];
    memset(longpath, 'a', sizeof longpath);
    longpath[1090] = '/';
    longpath[1099] = 0;
    highs_path(longpath);
    assert(!savepath[0]);
    highs_write();
    assert(save_failed);
    highs_path("game.pak");
    assert(!strcmp(savepath, "scores.dat"));
    highs_write();
    assert(!save_failed);

    assert(unlink("scores.dat") == 0);
    assert(unlink("sd:/apps/custom/scores.dat") == 0 && rmdir("sd:/apps/custom") == 0);
    assert(unlink("sd:/apps/paperplane-full/scores.dat") == 0
           && rmdir("sd:/apps/paperplane-full") == 0);
    assert(unlink("sd:/apps/paperplane/scores.dat") == 0 && rmdir("sd:/apps/paperplane") == 0);
    assert(rmdir("sd:/apps") == 0 && rmdir("sd:") == 0 && chdir("/") == 0 && rmdir(root) == 0);
    puts("PASS missing card, directory creation, legacy record migration, reload/replacement, failure preservation/retry, custom paths and long-path rejection");
    return 0;
}
