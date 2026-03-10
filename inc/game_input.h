#pragma once
#include <genesis.h>
#include "game_core.h"

// Struktur für die bereinigten Eingabebefehle eines Frames
typedef struct {
    bool moveLeft;
    bool moveRight;
    bool rotateCW;
    bool rotateCCW;
    bool softDrop;
    bool hardDrop;
    bool hold;
} PlayerInput;

// Aktualisiert interne Timer (DAS)
void input_update(GameContext* ctx);

// Liefert den gemappten Input (berücksichtigt Effekte)
PlayerInput input_get_mapped_state(GameContext* ctx);