DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
BUILD ?= $(HOME)/.cache/paperplane-build
COMMON = source/vm.c source/game.c source/render.c source/compose.c source/ui.c source/rom.c source/input.c source/main.c
AUDIO = source/audio.cpp $(wildcard source/sseq/*.cpp)
HEADERS = $(wildcard source/*.h source/sseq/*.h)
WARN = -Wall -Wextra -Wno-misleading-indentation -Wno-sign-compare -Wno-implicit-fallthrough
WFLAGS = -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float -O2 $(WARN) -I$(DEVKITPRO)/libogc/include
WLIBS = -L$(DEVKITPRO)/libogc/lib/wii -lasnd -lfat -lwiiuse -lbte -logc -lm
WOBJ = $(patsubst %.c,$(BUILD)/wii/%.o,$(COMMON) source/platform_wii.c) $(patsubst %.cpp,$(BUILD)/wii/%.o,$(AUDIO))
HOBJ = $(patsubst %.c,$(BUILD)/host/%.o,$(COMMON) source/platform_host.c) $(patsubst %.cpp,$(BUILD)/host/%.o,$(AUDIO))
.PHONY: all wii host full FORCE
all: wii
wii: $(BUILD)/boot.dol
host: $(BUILD)/paperplane-host
full: $(BUILD)/full/boot.dol
$(BUILD)/wii/%.o: %.c $(HEADERS)
	@mkdir -p "$(@D)"
	$(DEVKITPPC)/bin/powerpc-eabi-gcc $(WFLAGS) -std=gnu11 -c "$<" -o "$@"
$(BUILD)/wii/%.o: %.cpp $(HEADERS)
	@mkdir -p "$(@D)"
	$(DEVKITPPC)/bin/powerpc-eabi-g++ $(WFLAGS) -std=gnu++17 -c "$<" -o "$@"
$(BUILD)/host/%.o: %.c $(HEADERS)
	@mkdir -p "$(@D)"
	$(CC) -O2 -std=gnu11 $(WARN) $$(sdl2-config --cflags) -c "$<" -o "$@"
$(BUILD)/host/%.o: %.cpp $(HEADERS)
	@mkdir -p "$(@D)"
	$(CXX) -O2 -std=gnu++17 $(WARN) -c "$<" -o "$@"
$(BUILD)/boot.elf: $(WOBJ)
	$(DEVKITPPC)/bin/powerpc-eabi-g++ $(WFLAGS) $^ -Wl,-Map,$(BUILD)/boot.map $(WLIBS) -o "$@"
$(BUILD)/boot.dol: $(BUILD)/boot.elf
	$(DEVKITPRO)/tools/bin/elf2dol "$<" "$@"
$(BUILD)/paperplane-host: $(HOBJ)
	$(CXX) $^ $$(sdl2-config --libs) -lm -o "$@"
# PACK is a local extractor output. This target embeds user-supplied game data.
$(BUILD)/embedded.S: $(PACK) FORCE
	@mkdir -p "$(@D)"
	@test -n "$(PACK)" || (echo 'Set PACK=/absolute/path/to/game.pak'; exit 1)
	printf '.section .rodata\n.balign 32\n.global game_data,game_data_end\ngame_data:\n.incbin "$(abspath $(PACK))"\ngame_data_end:\n' > "$@"
$(BUILD)/full/main.o: source/main.c $(HEADERS)
	@mkdir -p "$(@D)"
	$(DEVKITPPC)/bin/powerpc-eabi-gcc $(WFLAGS) -std=gnu11 -DEMBEDDED_GAME -c "$<" -o "$@"
$(BUILD)/full/boot.elf: $(filter-out $(BUILD)/wii/source/main.o,$(WOBJ)) $(BUILD)/full/main.o $(BUILD)/embedded.S
	$(DEVKITPPC)/bin/powerpc-eabi-g++ $(WFLAGS) $^ $(WLIBS) -o "$@"
$(BUILD)/full/boot.dol: $(BUILD)/full/boot.elf
	$(DEVKITPRO)/tools/bin/elf2dol "$<" "$@"

.PHONY: check check-audio check-saves check-input check-display check-240p
$(BUILD)/check-240p: tests/240p.c source/compose.c source/ui.c source/game.c source/vm.c source/render.c $(HEADERS)
	@mkdir -p "$(@D)"
	$(CC) -O2 -std=gnu11 $(WARN) -Isource tests/240p.c source/compose.c source/ui.c source/game.c source/vm.c source/render.c -lm -o "$@"
check-240p: $(BUILD)/check-240p
	@test -n "$(PACK)" || (echo 'Set PACK=/absolute/path/to/game.pak'; exit 1)
	"$(BUILD)/check-240p" "$(PACK)"
$(BUILD)/check-display: tests/display.c source/ui.c source/game.c source/vm.c source/render.c $(HEADERS)
	@mkdir -p "$(@D)"
	$(CC) -O2 -std=gnu11 $(WARN) -Isource tests/display.c source/ui.c source/game.c source/vm.c source/render.c -lm -o "$@"
check-display: $(BUILD)/check-display
	@test -n "$(PACK)" || (echo 'Set PACK=/absolute/path/to/game.pak'; exit 1)
	"$(BUILD)/check-display" "$(PACK)"
$(BUILD)/check-input: tests/input.c source/input.c $(HEADERS)
	@mkdir -p "$(@D)"
	$(CC) -O2 -std=gnu11 $(WARN) -Isource tests/input.c source/input.c -o "$@"
check-input: $(BUILD)/check-input
	"$(BUILD)/check-input"
$(BUILD)/check-core: tests/core.c source/game.c source/vm.c source/render.c source/rom.c $(HEADERS)
	@mkdir -p "$(@D)"
	$(CC) -O2 -std=gnu11 $(WARN) -Isource tests/core.c source/game.c source/vm.c source/render.c source/rom.c -lm -o "$@"
check: $(BUILD)/check-core
	@test -n "$(PACK)" || (echo 'Set PACK=/absolute/path/to/game.pak'; exit 1)
	"$(BUILD)/check-core" "$(PACK)"

$(BUILD)/check-audio: tests/audio.cpp $(patsubst %.cpp,$(BUILD)/host/%.o,$(AUDIO)) $(addprefix $(BUILD)/host/source/,game.o vm.o render.o)
	$(CXX) -O2 -std=gnu++17 $(WARN) -Isource $^ -pthread -lm -o "$@"
check-audio: $(BUILD)/check-audio
	@test -n "$(PACK)" || (echo 'Set PACK=/absolute/path/to/game.pak'; exit 1)
	"$(BUILD)/check-audio" "$(PACK)"

$(BUILD)/check-saves: tests/saves.c source/main.c source/game.c source/vm.c source/render.c $(HEADERS)
	@mkdir -p "$(@D)"
	$(CC) -O2 -std=gnu11 $(WARN) -ffunction-sections -fdata-sections -Isource tests/saves.c source/game.c source/vm.c source/render.c -Wl,--gc-sections -lm -o "$@"
check-saves: $(BUILD)/check-saves
	"$(BUILD)/check-saves"
