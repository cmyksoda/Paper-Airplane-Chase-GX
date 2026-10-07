// SPDX-License-Identifier: GPL-3.0-only
#include "input.h"
#include <string.h>

unsigned input_select(InputState *s, const unsigned held[INPUT_DEVICES], unsigned present) {
    int old = s->owner, next = -1, priority = 0;
    if (old >= 0 && !(present & (1u << old))) {
        s->owner = -1;
        s->connected = 0;
    }

    for (int i = 0; i < INPUT_DEVICES; i++) {
        unsigned value = present & (1u << i) ? held[i] : 0;
        unsigned pressed = value & ~s->previous[i];
        // A fresh button or stick direction takes over the whole controller.
        // A stick left held on an inactive controller cannot steal it back.
        int rank = 0;
        if (value && i != s->owner && (pressed || s->owner < 0)) rank = 1;
        // A deliberate start wins over simultaneous navigation on another pad.
        if ((pressed & (KEY_ACTION | KEY_PAUSE)) && !(value & KEY_BACK)) rank = 2;
        if (rank > priority) {
            next = i;
            priority = rank;
        }
        s->previous[i] = value;
    }

    if (next >= 0) {
        s->owner = next;
        s->connected = 1;
    }
    if (s->owner < 0) return 0;
    return held[s->owner] | (s->owner != old ? KEY_CONTROLLER_CHANGED : 0);
}

void input_race_begin(RaceInputState *s, const InputState *menu) {
    memcpy(s->previous, menu->previous, sizeof s->previous);
    s->player_one = menu->owner;
    s->player_two = -1;
    s->second_input = 0;
    s->connected = 1;
}

unsigned input_race_select(RaceInputState *s, const unsigned held[INPUT_DEVICES],
                           unsigned present) {
    unsigned pressed[INPUT_DEVICES];
    int joined = 0;
    for (int i = 0; i < INPUT_DEVICES; i++) {
        unsigned value = present & (1u << i) ? held[i] : 0;
        pressed[i] = value & ~s->previous[i];
        s->previous[i] = value;
    }

    if (s->player_one < 0)
        for (int i = 0; i < INPUT_DEVICES; i++)
            if (pressed[i]) {
                s->player_one = i;
                break;
            }
    if (s->player_two < 0)
        for (int i = 0; i < INPUT_DEVICES; i++)
            if (i != s->player_one && pressed[i]) {
                s->player_two = i;
                joined = 1;
                break;
            }

    unsigned first = s->player_one >= 0 ? s->previous[s->player_one] : 0;
    s->second_input = s->player_two >= 0 ? s->previous[s->player_two] : 0;
    // Joining with Plus/Home must not immediately pause or exit the new race.
    if (s->player_two >= 0 && !joined) first |= pressed[s->player_two] & (KEY_PAUSE | KEY_MENU);
    s->connected = s->player_one >= 0 && (present & (1u << s->player_one))
                   && (s->player_two < 0 || (present & (1u << s->player_two)));
    return first;
}
