#pragma once

#include <genesis.h>
#include "states/game/game_core.h"

/**
 * Verarbeitet alle Spielereingaben (DAS, Rotation, Hold, Drops).
 * Berücksichtigt Spezialeffekte wie EFFECT_REVERSED.
 * @return bool: True, wenn eine Bewegung stattgefunden hat (für needsBoardDraw).
 */
bool controls_update(GameContext *ctx);