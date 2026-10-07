// SPDX-License-Identifier: GPL-3.0-only
#include "audio.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

static Game game;

int main(int argc, char **argv) {
    assert(argc == 2 && game_load(&game, argv[1]));
    Audio a{}, b{};
    assert(audio_init(&a, game.blobs[66]) && audio_init(&b, game.blobs[66]));

    int16_t x[16384], y[16384];
    audio_play(&a, 0, 1);
    audio_play(&b, 0, 1);
    audio_mix(&a, x, 8192);
    audio_mix(&b, y, 8192);
    assert(!memcmp(x, y, sizeof x));

    audio_commands(&b, &game);
    unsigned ticks = game.audio_ticks[0];
    audio_pause(&a, 1);
    audio_pause(&b, 1);
    audio_play(&b, AUDIO_UI_PLAYER, 4);
    bool chime = false;
    for (int i = 0; i < 8; i++) {
        audio_mix(&a, x, 8192);
        audio_mix(&b, y, 8192);
        assert(std::all_of(x, x + 16384, [](int16_t v) { return v == 0; }));
        chime |= std::any_of(y, y + 16384, [](int16_t v) { return v != 0; });
    }
    audio_commands(&b, &game);
    assert(chime && game.audio_ticks[0] == ticks);

    audio_pause(&a, 0);
    audio_pause(&b, 0);
    audio_mix(&a, x, 8192);
    audio_mix(&b, y, 8192);
    assert(!memcmp(x, y, sizeof x));

    // Exercise the same mixer/command contention as the Wii producer. Engine
    // teardown happens only after joining, just like platform_close().
    std::atomic<bool> ready{false}, done{false};
    std::atomic<unsigned> mixes{0};
    std::thread producer([&] {
        int16_t pcm[2048];
        ready = true;
        while (!done) {
            audio_mix(&b, pcm, 1024);
            mixes++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    while (!ready)
        std::this_thread::yield();

    for (int i = 0; i < 2000; i++) {
        audio_pause(&b, i & 1);
        audio_play(&b, AUDIO_UI_PLAYER, 4 + i % 3);
        if (i % 25 == 0) {
            audio_stop(&b);
            audio_play(&b, 0, 1);
        }
        game.sound_count = 1;
        game.sound_channel[0] = 0;
        game.sound_queue[0] = 0x10000 | (i % 128);
        audio_commands(&b, &game);
        assert(!game.sound_count);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    done = true;
    producer.join();
    assert(mixes > 0);

    audio_free(&a);
    audio_free(&b);
    game_free(&game);
    printf("PASS pause chime, frozen sequence, bit-exact resume, and 2000 concurrent command batches (%u mixes)\n",
           mixes.load());
}
