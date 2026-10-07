// SPDX-License-Identifier: GPL-3.0-only
#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define VTABLE 0x0210f000u
#define PALETTE_RAM 0x02320000u
#define PALETTE_RES 0x02330000u

uint16_t le16(const void *p) {
    const uint8_t *b = p;
    return b[0] | b[1] << 8;
}

uint32_t le32(const void *p) {
    const uint8_t *b = p;
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static uint32_t rd(Game *g, uint32_t a) {
    return vm_read(&g->vm, a, 4);
}

static void wr(Game *g, uint32_t a, uint32_t b) {
    vm_write(&g->vm, a, b, 4);
}

static void byte(Game *g, uint32_t a, unsigned b) {
    vm_write(&g->vm, a, b, 1);
}

static uint32_t alloc(Game *g, unsigned n) {
    n = (n + 31) & ~31u;
    uint32_t p = 0;
    for (int i = 0; i < 256; i++)
        if (g->free_blocks[i] && rd(g, g->free_blocks[i] - 4) >= n) {
            p = g->free_blocks[i];
            g->free_blocks[i] = 0;
            break;
        }

    if (!p) {
        p = g->heap + 32;
        if (n > 0x02300000u - p) {
            g->vm.fault = 1;
            snprintf(g->vm.error, 160, "Object heap exhausted");
            return 0;
        }
        g->heap = p + n;
        wr(g, p - 4, n);
    }

    memset(g->vm.mem + p - RAM_BASE, 0, n);
    wr(g, p, VTABLE);
    return p;
}

static void release(Game *g, uint32_t p) {
    if (p < SCENE + 0x10020 || p >= g->heap) return;
    for (int i = 0; i < 256; i++)
        if (g->free_blocks[i] == p) return;

    for (int i = 0; i < 256; i++)
        if (!g->free_blocks[i]) {
            g->free_blocks[i] = p;
            break;
        }
}

static int resource(Game *g, uint32_t a) {
    if (a < RAM_BASE || a >= RAM_BASE + RAM_SIZE - 100) return -1;
    const char *s = (char *)g->vm.mem + a - RAM_BASE;

    const struct {
        const char *name;
        int id;
    } names[] = {
        {"Plane/plane.ncl", 3},
        {"Plane/plane_bg.ncl", 4},
        {"Plane/plane.ncg", 5},
        {"Plane/plane_bg.ncg", 6},
        {"Plane/text_en.ncg", 7},
        {"Plane/darkness0.nsc", 10},
        {"Plane/darkness1.nsc", 11},
        {"Plane/side_wall.nsc", 12},
        {"Plane/sky.nsc", 13},
        {"Plane/wall.nsc", 14},
        {"Plane/plane.nce", 15},
        {"Plane/text_en.nce", 16},
    };

    for (unsigned i = 0; i < sizeof(names) / sizeof(*names); i++)
        if (!strcmp(s, names[i].name)) return names[i].id;
    return -1;
}

static int sound_channel(Game *g, uint32_t p) {
    for (int i = 0; i < 15; i++)
        if (g->sound_objects[i] == p) return i;

    for (int i = 0; i < 15; i++)
        if (!g->sound_objects[i]) {
            g->sound_objects[i] = p;
            return i;
        }
    return 14;
}

static void sound(Game *g, uint32_t p, int seq) {
    if (g->sound_count < 32) {
        g->sound_channel[g->sound_count] = sound_channel(g, p);
        g->sound_queue[g->sound_count++] = seq;
    }
}

static unsigned layer_mask(int l) {
    return 1u << (l < 4 ? l : l + 4);
}

static uint32_t palette_address(unsigned m) {
    return PALETTE_RAM + (m & 0x1000 ? 0 : m & 0xf00 ? 512 : m & 0x10 ? 1024 : 1536);
}

static void palette_upload(Game *g, uint32_t src, unsigned mask, unsigned from, unsigned to,
                           unsigned count) {
    if (count == ~0u) count = 512;
    if (to >= 512 || from >= 512) return;
    if (count > 512 - to) count = 512 - to;
    if (count > 512 - from) count = 512 - from;

    const unsigned masks[] = {0x1000, 0xf00, 0x10, 0xf};
    for (int i = 0; i < 4; i++)
        if (mask & masks[i])
            for (unsigned j = 0; j < count; j++)
                byte(g, PALETTE_RAM + i * 512 + to + j, vm_read(&g->vm, src + from + j, 1));
}

static void animate(Game *g, uint32_t p);

static int hook(VM *m, uint32_t a) {
    Game *g = m->user;
    uint32_t x = m->r[0], y = m->r[1], z = m->r[2], t = m->r[3], ret = x, sp = m->r[13];

    if (a == 0x0200b078 || a == 0x0200be1c) g->dead = 1;
    if ((a >= 0x02005708 && a < 0x02009080) || (a >= 0x0200930c && a < 0x0200c030)
        || (a >= 0x02036c48 && a < 0x02036d04) || (a >= 0x0203c064 && a < 0x0203d38c)
        || (a >= 0x020108e8 && a < 0x02010924) || (a >= 0x02013190 && a < 0x02013270)
        || (a >= 0x0201329c && a < 0x020132e4) || (a >= 0x02013388 && a < 0x02013498))
        return 0;

    switch (a) {
    case 0x0200526c:
        ret = 1;
        break;
    case 0x020121cc:
    case 0x02012124:
        ret = alloc(g, x);
        break;
    case 0x02012158:
        release(g, x);
        break;
    case 0x02011cb8:
        wr(g, x, VTABLE);
        wr(g, x + 4, y);
        wr(g, x + 0xc, 7);
        break;
    case 0x020120dc:
        wr(g, x, VTABLE);
        wr(g, x + 0xc, 7);
        byte(g, x + 0x4c, 1);
        break;
    case 0x02011e24:
        wr(g, x + 0xc, y);
        break;
    case 0x02012090:
        ret = y;
        for (int i = 0; i < 64; i++)
            if (!g->tasks[i].ptr) {
                g->tasks[i] = (Task){y, 0, 0};
                break;
            }
        break;
    case 0x02012044:
        for (int i = 0; i < 64; i++)
            if (g->tasks[i].ptr == x) g->tasks[i].done = 1;
        break;
    case 0x020120bc:
        wr(g, x + 36, y);
        wr(g, x + 40, z);
        wr(g, x + 44, t);
        wr(g, x + 48, rd(g, sp));
        break;
    case 0x02013270:
    case 0x02011dc4:
        break;
    case 0x02080000:
    case 0x02080004:
        ret = y;
        break;
    case 0x02031468:
        for (unsigned i = 0; i < z; i++)
            byte(g, x + i, vm_read(m, y + i, 1));
        break;
    case 0x02031480:
    case 0x0202aa50:
        for (unsigned i = 0; i < z; i++)
            byte(g, x + i, y);
        break;
    case 0x0202aa04:
        for (unsigned i = 0; i < z; i++)
            byte(g, y + i, vm_read(m, x + i, 1));
        break;
    case 0x02035da8:
        for (unsigned i = 0; i < y; i++)
            byte(g, x + i, 0);
        break;
    case 0x0202805c:
        ret = ((int64_t)(int32_t)x * (int32_t)y + 0x800) >> 12;
        break;
    case 0x02035928: {
        int64_t sx = (int32_t)x, sy = (int32_t)y;
        ret = sy ? sx / sy : 0;
        m->r[1] = sy ? sx % sy : 0;
        break;
    }
    case 0x02035b34:
        ret = y ? x / y : 0;
        m->r[1] = y ? x % y : 0;
        break;
    case 0x02012d0c:
        wr(g, x + 0x38, ~0u);
        break;
    case 0x02012d80:
        break;
    case 0x02012dac:
    case 0x02012d94:
        sound(g, x, y);
        wr(g, x + 0x38, y);
        ret = 1;
        break;
    case 0x02012de8:
        sound(g, x, -1);
        break;
    case 0x02012df4:
        sound(g, x, 0x80000 | (y & 1));
        break;
    case 0x02012e00:
        sound(g, x, 0x10000 | (y & 127));
        break;
    case 0x02012e14:
        sound(g, x, 0x10000 | (y & 127));
        break;
    case 0x02012e20:
        sound(g, x, 0x40000 | (y & 255));
        break;
    case 0x02012dd4:
        ret = 0;
        for (int i = 0; i < 15; i++)
            if (g->audio_active[i] && g->audio_sequence[i] == (int)y) ret = 1;
        break;
    case 0x02012e34:
        wr(g, x + 0x3c, y);
        break;
    case 0x02013308:
        if (z || vm_read(m, x + 0xac, 2) != y) {
            vm_write(m, x + 0xac, y, 2);
            wr(g, x + 0xd8, 0);
            wr(g, x + 0x58, 0);
            wr(g, x + 0x60, 0);
            wr(g, x + 0x5c, 1);
            wr(g, x + 0x64, 4096);
            wr(g, x + 0x68, 0);
        }
        break;
    case 0x02013364:
        wr(g, x + 0xd8, y);
        wr(g, x + 0x60, 0);
        if (!z) wr(g, x + 0x5c, 0);
        break;
    case 0x02013498:
        if (rd(g, x + 0xc) & 2) animate(g, x);
        break;
    case 0x020134dc:
        if ((rd(g, x + 0xc) & 4) && g->draw_count < MAX_SPRITES && x >= RAM_BASE
            && x <= RAM_BASE + RAM_SIZE - 224) {
            memcpy(g->draws[g->draw_count++], m->mem + x - RAM_BASE, 224);
        }
        break;
    case 0x02012e38:
        if (t < 4) g->bank_screen[t] = z;
        g->enabled |= z;
        break;
    case 0x0201442c:
        g->enabled |= y;
        break;
    case 0x02014494:
        ret = 0;
        for (int i = 0; i < 8; i++)
            if (y & layer_mask(i)) {
                ret = 0x02310000 + i * 4096;
                break;
            }
        break;
    case 0x02014520:
        ret = palette_address(y);
        break;
    case 0x02013eec: {
        int id = resource(g, x);
        ret = id >= 0 ? PALETTE_RES + id * 512 : 0;
        if (ret) palette_upload(g, ret, y, z, t, rd(g, sp));
        break;
    }
    case 0x02013f4c:
        palette_upload(g, x, y, z, t, rd(g, sp));
        break;
    case 0x02013f74:
        vm_write(m, palette_address(y) + z, x, 2);
        break;
    case 0x020136ec: {
        int id = resource(g, x);
        if (id >= 0)
            for (int l = 0; l < 8; l++)
                if (y & layer_mask(l)) g->bg_gfx[l] = id;
        break;
    }
    case 0x02014180: {
        int id = resource(g, x);
        if (id >= 0)
            for (int l = 0; l < 8; l++)
                if (y & layer_mask(l)) {
                    Blob b = g->blobs[id];
                    g->bg_width[l] = le16(b.p + 24);
                    g->bg_height[l] = le16(b.p + 26);
                    unsigned n = b.size - 36;
                    if (n > 4096) n = 4096;
                    memcpy(m->mem + 0x310000 + l * 4096, b.p + 36, n);
                }
        break;
    }
    case 0x020141e0:
        break;
    case 0x02014e00:
        for (int i = 0; i < 2; i++)
            if (y & (i ? 0x1f : 0x1f00)) g->window_active[i] = z;
        break;
    case 0x02014e38:
        for (int i = 0; i < 2; i++)
            if (y & (i ? 0x1f : 0x1f00)) {
                g->window_in[i] = (y >> (i ? 0 : 8)) & 31;
                for (int j = 0; j < 4; j++)
                    g->window_rect[i][j] = rd(g, z + j * 4);
            }
        break;
    case 0x02014ebc:
        for (int i = 0; i < 2; i++)
            if (y & (i ? 0x1f : 0x1f00)) g->window_out[i] = (y >> (i ? 0 : 8)) & 31;
        break;
    case 0x020152f8:
        for (int i = 0; i < 8; i++)
            if (y & layer_mask(i)) g->bg_x[i] = (int32_t)z;
        break;
    case 0x02015368:
        for (int i = 0; i < 8; i++)
            if (y & layer_mask(i)) {
                g->bg_y[i] = (int32_t)z;
            }
        break;
    case 0x0201539c:
        for (int i = 0; i < 8; i++)
            if (y & layer_mask(i)) {
                g->bg_y[i] += (int32_t)z;
            }
        break;
    case 0x02015478:
        for (int i = 0; i < 8; i++)
            if (y & layer_mask(i)) g->bg_pri[i] = z;
        break;
    case 0x02014b88:
        for (int i = 0; i < 2; i++) {
            unsigned shift = i ? 0 : 8;
            if ((y | z) & (0x3fu << shift)) {
                g->blend_first[i] = (y >> shift) & 63;
                g->blend_second[i] = (z >> shift) & 63;
                g->blend_a[i] = t;
                g->blend_b[i] = rd(g, sp);
                g->brightness_layers[i] = 0;
            }
        }
        break;
    case 0x02014c10:
        for (int i = 0; i < 2; i++) {
            unsigned shift = i ? 0 : 8;
            if (y & (0x3fu << shift)) {
                g->brightness_layers[i] = (y >> shift) & 63;
                g->brightness[i] = (int32_t)z;
                g->blend_first[i] = g->blend_second[i] = 0;
            }
        }
        break;
    case 0x02014acc:
        for (int i = 0; i < 2; i++)
            if (y & (i ? 0x3f : 0x3f00)) g->master_brightness[i] = (int32_t)z;
        break;
    case 0x02010edc:
        g->dead = 1;
        break;
    case 0x02010f10:
    case 0x02010f58:
    case 0x02010f3c:
    case 0x02015a80:
        break;
    case 0x02009278:
        if (y < 8 && z && (!g->high[y + 1] || z < g->high[y + 1])) g->high[y + 1] = z;
        break;
    case 0x0200924c:
        if (y > g->high[0]) g->high[0] = y;
        break;
    default:
        snprintf(m->error, sizeof(m->error), "unmapped service %08x from %08x", a, m->r[14]);
        m->fault = 1;
        break;
    }

    m->r[0] = ret;
    return 1;
}

static uint32_t crc(const uint8_t *p, size_t n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (int b = 0; b < 8; b++)
            c = (c >> 1) ^ (0xedb88320u & -(c & 1));
    }
    return ~c;
}

static int validate(Game *g) {
    size_t n = g->pack_size;
    if (memcmp(g->pack, "PAP1", 4) || le32(g->pack + 4) != 67) goto invalid;

    for (unsigned i = 0; i < 67; i++) {
        uint8_t *e = g->pack + 8 + i * 12;
        uint32_t off = le32(e), len = le32(e + 4);
        if (off < 812 || off > (unsigned)n || len > (unsigned)n - off
            || crc(g->pack + off, len) != le32(e + 8))
            goto invalid;
        g->blobs[i] = (Blob){g->pack + off, len};
    }

    if (g->blobs[65].size != 0x51880) goto invalid;
    g->vm.hook = hook;
    g->vm.user = g;
    return 1;

invalid:
    snprintf(g->vm.error, sizeof(g->vm.error), "Invalid or damaged game.pak");
    game_free(g);
    return 0;
}

int game_load_memory(Game *g, const void *data, size_t n) {
    memset(g, 0, sizeof(*g));
    if (n < 812 || n > 8 * 1024 * 1024) return 0;

    g->pack = malloc(n);
    g->vm.mem = calloc(1, RAM_SIZE);
    g->pack_size = n;
    if (!g->pack || !g->vm.mem) {
        game_free(g);
        return 0;
    }

    memcpy(g->pack, data, n);
    return validate(g);
}

int game_load(Game *g, const char *path) {
    memset(g, 0, sizeof(*g));
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(g->vm.error, sizeof(g->vm.error), "game.pak missing");
        return 0;
    }

    if (fseek(f, 0, SEEK_END)) {
        fclose(f);
        return 0;
    }
    long n = ftell(f);
    rewind(f);
    if (n < 812 || n > 8 * 1024 * 1024) {
        fclose(f);
        return 0;
    }

    g->pack = malloc(n);
    g->vm.mem = calloc(1, RAM_SIZE);
    g->pack_size = n;
    if (!g->pack || !g->vm.mem || fread(g->pack, 1, n, f) != (size_t)n) {
        fclose(f);
        game_free(g);
        return 0;
    }
    fclose(f);
    return validate(g);
}

static void animate(Game *g, uint32_t p) {
    static const int banks[] = {19, 20, 19, 20};
    {
        unsigned bank = rd(g, p + 0xa8);
        if (bank > 3 || !rd(g, p + 0x5c)) return;
        Blob b = g->blobs[banks[bank]];
        unsigned seq = vm_read(&g->vm, p + 0xac, 2);
        if (seq >= le16(b.p + 24)) return;
        uint8_t *s = b.p + 24 + le32(b.p + 28) + seq * 16;
        unsigned count = le16(s);
        if (!count) return;

        unsigned fr = rd(g, p + 0xd8);
        if (fr >= count) fr = count - 1;
        uint8_t *frames = b.p + 24 + le32(b.p + 32) + le32(s + 12);
        int speed = (int32_t)rd(g, p + 0x64), reverse = !!rd(g, p + 0x58);
        unsigned elapsed = rd(g, p + 0x60) + (speed < 0 ? -(int64_t)speed : speed);
        unsigned mode = rd(g, p + 0x68);
        if (!mode) mode = le32(s + 8);
        unsigned loop = le16(s + 2);
        if (loop >= count) loop = 0;

        while (rd(g, p + 0x5c)) {
            unsigned duration = le16(frames + fr * 8 + 4) * 4096u;
            if (!duration || elapsed < duration) break;
            elapsed -= duration;
            int forward = (speed > 0) ^ reverse, next = (int)fr + (forward ? 1 : -1);
            if (next >= (int)count || next < (int)loop) {
                if (mode == 3 || mode == 4) {
                    reverse ^= 1;
                    wr(g, p + 0x58, reverse);
                }
                if ((mode == 3 || mode == 4) && next >= (int)loop)
                    next = count - 1;
                else if (mode == 2 || mode == 4) {
                    next = ((speed > 0) ^ reverse) ? loop : count - 1;
                    elapsed = 0;
                } else {
                    next = next < 0 ? 0 : count - 1;
                    wr(g, p + 0x5c, 0);
                }
            }
            fr = next;
        }

        wr(g, p + 0x60, elapsed);
        wr(g, p + 0xd8, fr);
    }
}

int game_start(Game *g, int mode, uint32_t seed) {
    if (mode < 0 || mode > 2 || g->course < 0 || g->course > 7) return 0;

    VM *m = &g->vm;
    memset(m->mem, 0, RAM_SIZE);
    memcpy(m->mem + 0x4000, g->blobs[65].p, g->blobs[65].size);
    memset(m->r, 0, sizeof m->r);
    m->fault = 0;
    m->error[0] = 0;
    m->instructions = 0;

    memset(g->free_blocks, 0, sizeof g->free_blocks);
    memset(g->sound_objects, 0, sizeof g->sound_objects);
    g->heap = SCENE + 0x10000;
    g->frame = 0;
    g->mode = mode;
    g->previous = 0;
    g->dead = 0;
    g->sound_count = 0;
    g->draw_count = 0;
    g->enabled = 0;

    memset(g->bg_x, 0, sizeof g->bg_x);
    memset(g->bg_y, 0, sizeof g->bg_y);
    memset(g->bg_gfx, 0, sizeof g->bg_gfx);
    memset(g->bg_pri, 0, sizeof g->bg_pri);
    memset(g->blend_first, 0, sizeof g->blend_first);
    memset(g->blend_second, 0, sizeof g->blend_second);
    memset(g->brightness_layers, 0, sizeof g->brightness_layers);
    memset(g->master_brightness, 0, sizeof g->master_brightness);

    memset(g->audio_active, 0, sizeof g->audio_active);
    memset(g->audio_ticks, 0, sizeof g->audio_ticks);
    g->winner = 0;
    memset(m->mem + 0x55544, 0, 0x9c);
    for (int i = 0; i < 8; i++)
        g->bg_width[i] = g->bg_height[i] = 256;
    memset(g->tasks, 0, sizeof g->tasks);
    memset(g->window_active, 0, sizeof g->window_active);

    for (int i = 0; i < 256; i += 4)
        wr(g, VTABLE + i, 0x02080005);
    for (int id = 3; id <= 4; id++)
        memcpy(m->mem + PALETTE_RES + id * 512 - RAM_BASE, g->blobs[id].p + 40, 512);

    const uint32_t globals[] = {0x02055544, 0x02055548, 0x0205554c, 0x02055550,
                                0x02055554, 0x02055558, 0x0205555c, 0x02055584,
                                0x02055588, 0x020555bc, 0x020555c0, 0x020555c4};
    for (unsigned i = 0; i < sizeof globals / sizeof *globals; i++)
        wr(g, globals[i], alloc(g, 0x3000));

    wr(g, rd(g, 0x02055584) + 0x219c, 1);
    wr(g, 0x0205558c + 0x28, g->high[0]);
    wr(g, 0x0205558c + 0x30, g->course);
    wr(g, 0x020555cc, seed);
    wr(g, 0x020555d0, 0x5d588b65);
    wr(g, 0x020555d4, 0x00269ec3);
    for (int i = 0; i < 8; i++)
        wr(g, 0x0205558c + i * 4, g->high[i + 1] ? g->high[i + 1] : 3600);

    g->finished = 0;
    g->elapsed = 0;
    g->end_frame = 0;

    for (uint32_t a = 0x0203d3d8; a <= 0x0203d424 && !m->fault; a += 4)
        vm_call(m, rd(g, a), 0, 0, 0, 0);
    if (!vm_call(m, mode == 0 ? 0x0200a82c : mode == 1 ? 0x0200b424 : 0x02009748, SCENE, 0, 0, 0))
        return 0;
    vm_call(m, rd(g, rd(g, SCENE) + 8), SCENE, 0, 0, 0);
    return !m->fault;
}

int game_tick(Game *g, uint32_t input) {
    VM *m = &g->vm;
    if (m->fault) return 0;

    g->sound_count = 0;
    g->draw_count = 0;

    uint32_t k = rd(g, 0x02055548);
    wr(g, k + 4, input & ~g->previous);
    wr(g, k + 8, input);
    g->previous = input;
    g->frame++;

    vm_call(m, rd(g, rd(g, SCENE) + 12), SCENE, 0, 0, 0);
    vm_call(m, rd(g, rd(g, SCENE) + 16), SCENE, 0, 0, 0);

    for (int i = 0; i < 64 && !m->fault; i++)
        if (g->tasks[i].ptr) {
            Task *task = &g->tasks[i];
            uint32_t p = task->ptr, vt = rd(g, p);
            if (!task->started) {
                task->started = 1;
                vm_call(m, rd(g, vt + 8), p, 0, 0, 0);
            }
            if (rd(g, p + 0xc) & 2) vm_call(m, rd(g, vt + 16), p, 0, 0, 0);
            if (task->done) {
                vm_call(m, rd(g, vt + 24), p, 0, 0, 0);
                task->ptr = 0;
                uint32_t cb = rd(g, p + 40);
                if (cb) vm_call(m, cb, rd(g, p + 36), rd(g, p + 48), 0, 0);
                release(g, p);
            }
        }

    vm_call(m, rd(g, rd(g, SCENE) + 20), SCENE, 0, 0, 0);

    if (g->mode == 0) {
        unsigned score = rd(g, SCENE + 0x342c);
        if (score > g->high[0]) g->high[0] = score;
    }
    if (g->mode == 1) {
        g->elapsed = rd(g, SCENE + 0x3f80);
        if (rd(g, SCENE + 0x9d30)) {
            if (!g->finished) {
                g->finished = 1;
                g->end_frame = g->frame + 120;
                unsigned *best = &g->high[g->course + 1];
                if (g->elapsed && (!*best || g->elapsed < *best)) *best = g->elapsed;
            }
            if (g->frame >= g->end_frame) g->dead = 1;
        }
    } else if (g->mode == 2 && rd(g, SCENE + 0x350)) {
        if (!g->finished) {
            g->finished = 1;
            g->winner = rd(g, SCENE + 0x164) + 1;
            g->end_frame = g->frame + 120;
        }
        if (g->frame >= g->end_frame) g->dead = 1;
    }

    return !m->fault;
}

unsigned game_score(Game *g) {
    return g->mode == 0 ? rd(g, SCENE + 0x342c) : g->elapsed;
}

void game_free(Game *g) {
    free(g->pack);
    free(g->vm.mem);
    g->pack = NULL;
    g->vm.mem = NULL;
}
