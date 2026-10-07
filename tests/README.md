# Core checks

## Controller checks

`make check-input` checks all 56 distinct pairings of the eight Wii input slots,
including starter priority over simultaneous navigation, waiting for fresh Player 2
activity, consuming the join press, independent steering, ignored third controllers,
fixed assignments on disconnect/reconnect, and single-player handoff.

The production Wii build was also checked in Dolphin with three GameCube pads and
a bare Wii Remote: the starter owns Player 1, the countdown freezes until Player 2
joins, either player can pause, other pads cannot steal a race, and restart preserves
the pair. Endless and Time Attack also pass Remote-to-GameCube-to-Remote handoff
with the old direction still held, both upright and sideways steering, and pause-menu
navigation. These checks exercise mapped inputs, not physical controller hardware.

## Gameplay checks

Run from the source directory with your own extracted pack:

```sh
make check PACK=/absolute/path/game.pak
```

The test starts each game repeatedly and exercises every Time Attack course. It checks crash paths, injected goals, both race winners, and corrupt-cache rejection. The last run replays `course1-inputs.txt` with a fixed seed, completes course 1 with ordinary left/right input, and asserts the finish time of 1,044 game frames. The replay contains only generated controller input, no game data.

Optionally check native importer parity:

```sh
~/.cache/paperplane-build/check-core /path/game.pak /path/your.nds /tmp/paperplane-native.pak
cmp /path/game.pak /tmp/paperplane-native.pak
```

The test's injected goals verify completion transitions; they do not establish that every course has been played through. These host checks do not replace testing the Wii executable, controllers, SD card, sound or video output on hardware.

## Audio checks

```sh
make check-audio PACK=/absolute/path/game.pak
python3 tests/audio-queue.py
```

The audio test checks an audible UI chime while gameplay sequence time stays frozen,
bit-exact resumed music, and concurrent mixing with 2,000 pause/play/stop/volume command
batches. The queue test compiles the actual Wii refill code against a fake ASND backend
that rejects every ninth refill. It verifies ordered samples without drops or repeats,
buffer ownership, and reset bookkeeping. It needs no game data.

Neither host test measures DSP output or replaces audio testing on the Wii.

## Record saving checks

```sh
make check-saves
```

This uses the app's actual save functions in an isolated temporary directory, with
no game data. It covers a missing device root, directory creation for a first save,
legacy embedded-record migration, existing-record replacement and reload, preserving
records after a forced write failure, retry, custom app paths and oversized paths.

Graphics persistence checks cover all eight 240p/Fixed/pillarbox combinations,
confirmation when leaving Graphics Options, relaunch and repeated restore,
40-byte legacy saves, incomplete/invalid settings extensions, preservation of the
score prefix, unconfirmed previews, 4:3 aspect handling and failed-write retry.
Video toggles are stubbed: these tests exercise the actual save/load code without
changing a display mode or launching Dolphin.

The embedded Wii executable was also tested in Dolphin against a blank FAT SD image.
The previous build failed at `sd:/apps/paperplane-full/scores.dat`; v0.1.3 creates
`sd:/apps/paperplane/` and writes a valid record file, including a subsequent save.
The revised title prompts were visually checked in the same Wii executable.

## Fixed display checks

`make check-display PACK=/path/to/game.pak` compares the actual gameplay composition against every original source pixel at normal 4:3 Fixed sample positions, in all three modes. It also checks 2× menu blocks, removal of the Race steering hint, and retention of the Player 2 join prompt. The older corner-based intermediate sampler fails this comparison; centered sampling passes. This does not claim integer gameplay scaling at 240p or in widescreen.

## 240p display checks

`make check-240p PACK=/path/to/game.pak` moves a one-pixel white ledge through every
source row in the stacked and Race layouts, with Fixed/Fill, 4:3, widescreen and
pillarboxing. Every row must contribute to the final 640×240 image; total line
brightness may vary only within RGB565 rounding tolerance. It also checks exact
pause-menu pixels using actual game data.

For static 4:3 host previews, pass an existing output directory as the test binary's
second argument. Each physical 240p row is repeated twice in these PPM previews to
show the intended display proportions. These are CPU composition checks, not
Dolphin captures or measurements of a Wii/TV signal. Jaxi subsequently tested the
October 7 revision on her Wii and confirmed improved 240p output and readable
original score/time text.
