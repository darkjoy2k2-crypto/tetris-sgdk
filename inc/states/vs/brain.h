#pragma once

#include <genesis.h>
#include "states/states.h"

void vs_brain_reset(VsContext* vctx);
void vs_brain_update_player(VsContext* vctx, GameContext* player, bool* deadFlag, bool* needsRedraw);
