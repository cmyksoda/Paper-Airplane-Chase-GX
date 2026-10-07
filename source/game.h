// SPDX-License-Identifier: GPL-3.0-only
#ifndef PAP_GAME_H
#define PAP_GAME_H

#include "vm.h"

#define SCENE 0x02100000u
#define WIDTH 256
#define HEIGHT 192
#define CANVAS_FULLSCREEN 2
#define MAX_SPRITES 512

typedef struct {
    uint8_t *p;
    uint32_t size;
} Blob;

typedef struct {
    uint32_t ptr;
    int started, done;
} Task;

typedef struct {
    VM vm;
    uint8_t *pack;
    size_t pack_size;
    Blob blobs[67];
    uint8_t draws[MAX_SPRITES][224];
    unsigned draw_count;
    uint32_t sound_objects[16];
    int course, finished;
    unsigned elapsed, end_frame;
    int bg_x[8], bg_y[8], bg_gfx[8], bg_pri[8], bg_width[8], bg_height[8];
    unsigned enabled;
    uint16_t screens[2][WIDTH * HEIGHT];
    uint32_t heap, frame, previous, high[9];
    uint32_t free_blocks[256];
    Task tasks[64];
    unsigned window_active[2], window_in[2], window_out[2];
    int window_rect[2][4];
    int mode, dead, winner;
    uint32_t bank_screen[4];
    unsigned blend_first[2], blend_second[2], brightness_layers[2];
    int blend_a[2], blend_b[2], brightness[2], master_brightness[2];
    uint32_t audio_ticks[16];
    int audio_active[16], audio_sequence[16];
    int sound_queue[32], sound_channel[32], sound_count;
    uint16_t pixels[WIDTH * HEIGHT];
    uint16_t menu_pixels[640 * 480];
    int menu_frame;
} Game;

#ifdef __cplusplus
extern "C" {
#endif

uint16_t le16(const void *);
uint32_t le32(const void *);
int game_load(Game *,const char *);
int game_load_memory(Game *,const void *,size_t);
int game_start(Game *,int,uint32_t);
int game_tick(Game *,uint32_t);
void game_render(Game *);
void game_render_native(Game *);
void game_compose_240p(Game *, int wide, int pillarbox, int scale);
void game_scanout_240p(const uint16_t *, uint16_t *);
void game_free(Game *);
void text_draw_scaled(Game *,int,int,const char *,uint16_t,int);
void game_canvas(Game *,int);
unsigned game_score(Game *);
void game_art(Game *,int,int,int,int,float,float,float);
void game_background(Game *,int,int,int);
int game_animation_cell(Game *,int,int,unsigned);
void game_cell(Game *,int,int,int,int,int,int);

#ifdef __cplusplus
}
#endif
#endif
