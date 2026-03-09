#ifndef _GAME_VIEW_H_
#define _GAME_VIEW_H_

#include <genesis.h>

void load_background();
void view_init_cache();
void drawBoard();
void drawPreview(s16 type, u16 x, u16 y);
void view_fade_in_frame();
void view_fade_out_frame();
void view_animate_grayscale();
void gfx_load_extra_tiles(u16 start_index);

extern u16 BG_TILE_START;
extern u16 GAME_TILE_START;

#endif