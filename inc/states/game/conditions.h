#pragma once

#include <genesis.h>
#include "states/states.h"

void game_conditions_set_from_select(const SelectContext *selectCtx);
void game_conditions_set_challenge_training(void);
void game_conditions_set_challenge_score_training(void);
void game_conditions_set_for_challenge_level(u8 levelId);

void game_conditions_update_progress_from_score(u32 score);
void game_conditions_on_lines_cleared(u16 linesCleared, u32 score);
bool game_conditions_is_success(u32 score);
