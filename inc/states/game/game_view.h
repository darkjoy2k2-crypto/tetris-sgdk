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