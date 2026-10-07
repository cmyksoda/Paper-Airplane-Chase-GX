// SPDX-License-Identifier: GPL-3.0-only
#ifndef PAP_INPUT_H
#define PAP_INPUT_H
#include "platform.h"
#define INPUT_DEVICES 8
typedef struct {
    unsigned previous[INPUT_DEVICES];
    int owner,connected;
} InputState;
#define INPUT_STATE_INIT {{0},-1,1}
unsigned input_select(InputState *,const unsigned held[INPUT_DEVICES],unsigned present);
typedef struct {
    unsigned previous[INPUT_DEVICES],second_input;
    int player_one,player_two,connected;
} RaceInputState;
void input_race_begin(RaceInputState *,const InputState *);
unsigned input_race_select(RaceInputState *,const unsigned held[INPUT_DEVICES],unsigned present);
#endif
