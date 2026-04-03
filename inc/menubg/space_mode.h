#pragma once

#include "menu_bg.h"

void menubg_space_init_tiles(u8 intensity);
void menubg_space_apply(MenuBgState* state);
void menubg_space_update(MenuBgState* state);
void menubg_space_set_intensity(MenuBgState* state, u8 level, bool is_active);
