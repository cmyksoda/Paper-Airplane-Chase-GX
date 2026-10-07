// SPDX-License-Identifier: GPL-3.0-only
#include "input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    for (int p1 = 0; p1 < INPUT_DEVICES; p1++)
        for (int p2 = 0; p2 < INPUT_DEVICES; p2++)
            if (p1 != p2) {
                InputState menu = INPUT_STATE_INIT;
                RaceInputState race;
                unsigned held[INPUT_DEVICES] = {0}, present = (1u << INPUT_DEVICES) - 1;

                // The starter wins even if another controller navigates simultaneously.
                held[p2] = KEY_RIGHT;
                held[p1] = KEY_ACTION;
                assert(input_select(&menu, held, present) & KEY_ACTION);
                assert(menu.owner == p1);
                input_race_begin(&race, &menu);
                input_race_select(&race, held, present);
                assert(race.player_one == p1 && race.player_two == -1 && race.connected);

                memset(held, 0, sizeof held);
                input_race_select(&race, held, present);
                assert(race.player_two == -1); // Connected but idle devices cannot join.
                held[p2] = KEY_PAUSE;
                assert(!(input_race_select(&race, held, present) & KEY_PAUSE));
                assert(race.player_two == p2);
                assert(!(input_race_select(&race, held, present) & KEY_PAUSE));
                held[p2] = 0;
                input_race_select(&race, held, present);
                held[p2] = KEY_PAUSE;
                assert(input_race_select(&race, held, present) & KEY_PAUSE);

                held[p1] = KEY_LEFT;
                held[p2] = KEY_RIGHT;
                unsigned first = input_race_select(&race, held, present);
                assert((first & (KEY_LEFT | KEY_RIGHT)) == KEY_LEFT
                       && race.second_input == KEY_RIGHT);

                int third = (p2 + 1) % INPUT_DEVICES;
                if (third == p1) third = (third + 1) % INPUT_DEVICES;
                held[third] = KEY_ACTION | KEY_LEFT | KEY_MENU;
                first = input_race_select(&race, held, present);
                assert(first == KEY_LEFT && race.second_input == KEY_RIGHT);
                assert(race.player_one == p1 && race.player_two == p2);

                input_race_select(&race, held, present & ~(1u << p2));
                assert(!race.connected && race.second_input == 0 && race.player_two == p2);
                input_race_select(&race, held, present);
                assert(race.connected);
                input_race_select(&race, held, present & ~(1u << p1));
                assert(!race.connected);
                input_race_select(&race, held, present);
                assert(race.connected);
            }

    // Ordinary single-player handoff still requires new activity.
    InputState menu = INPUT_STATE_INIT;
    unsigned held[INPUT_DEVICES] = {0};
    held[0] = KEY_LEFT;
    input_select(&menu, held, 3);
    assert(menu.owner == 0);
    held[1] = KEY_RIGHT;
    input_select(&menu, held, 3);
    assert(menu.owner == 1);
    input_select(&menu, held, 3);
    assert(menu.owner == 1);
    held[0] = KEY_LEFT | KEY_PAUSE;
    assert(input_select(&menu, held, 3) & KEY_PAUSE);
    assert(menu.owner == 0);
    held[1] = 0;
    input_select(&menu, held, 2);
    assert(!menu.connected);

    puts("PASS input: all 56 Wii/GameCube slot pairings, starter priority, activity-only join, held-input suppression, independent steering, fixed ownership, disconnect/reconnect and single-player handoff");
}
