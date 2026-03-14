#pragma once

#include <genesis.h>

typedef struct {
    s16 x, y;
    u16 frame;        // 0-3 für die Rotation
    u16 animation;    // 0 = Normal, 1 = Durchgestrichen
    u16 animTimer;    // Timer für 1/2 Sekunde (Frames)
    u16 stateTimer;   // Timer für den Wechsel jede Sekunde
    u16 attr;         // Bitfield: [0: visible, 1: flipX, 2: flipY, 3: priority]
    Sprite* vdpSprite; // Link zum SGDK Sprite-Objekt
} GameSprite;

extern GameSprite gameSprites[10];

void sprites_init();
void sprites_update();
void sprites_set_visible(u8 index, bool visible);

