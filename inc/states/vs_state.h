#pragma once

#include <genesis.h>
#include "states/states.h"

void vs_state_init();
void vs_state_init_draw();
void vs_state_update();
void vs_state_draw();
void vs_state_cleanup();
bool vs_spawn_piece_for_player(GameContext* player);
bool vs_lock_piece_for_player(VsContext* vs, GameContext* player, bool isLeft, u8 lockPulseType, bool lastMoveWasRotate);
