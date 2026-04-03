#pragma once

#include "menu_bg.h"

void menubg_riistar_apply(MenuBgState* state);
void menubg_riistar_update(MenuBgState* state);
void menubg_riistar_set_stack_top(MenuBgState* state, s16 topRow);
void menubg_riistar_pulse(MenuBgState* state, u8 pulseType, u8 bg_mode, bool is_active);
