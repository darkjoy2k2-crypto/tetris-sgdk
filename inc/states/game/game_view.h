#pragma once

#include <genesis.h>
// DIESE ZEILE FEHLT:
#include "states/game/game_core.h" 

void load_background();
void view_init_cache();
void view_fade_in_frame();
void view_fade_out_frame();
void view_animate_grayscale();

void drawPreview(s16 type, u16 x, u16 y);
void view_update_ui(GameContext* ctx); // Jetzt kennt er GameContext
void drawBoard();
void view_draw_board_for_context(GameContext* gctx, u16 renderX, u16 renderY, u16 tileStart, u16 skullTileIdx, u16 heartTileIdx, u16* cache, bool drawActivePiece, bool drawShadow);
void view_draw_debug_bag(GameContext* ctx);
