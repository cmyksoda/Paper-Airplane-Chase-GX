// SPDX-License-Identifier: GPL-3.0-only
#ifndef PAP_AUDIO_H
#define PAP_AUDIO_H
#include "game.h"
#define AUDIO_RATE 32000
#define AUDIO_PLAYERS 16
#define AUDIO_UI_PLAYER 15
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    void *engine;
    int paused;
    uint64_t mixed_frames, ui_end_frame;
} Audio;

int audio_init(Audio *,Blob);
void audio_free(Audio *);
void audio_stop(Audio *);
void audio_pause(Audio *,int);
void audio_lock(Audio *);
void audio_unlock(Audio *);
void audio_play(Audio *,int,int);
void audio_commands(Audio *,Game *);
void audio_mix(Audio *,int16_t *,unsigned);
#ifdef __cplusplus
}
#endif
#endif
