#pragma once

#include <genesis.h>

#define NOROT_NEXTFRAME     10
#define NOROT_NETXANIM      60

// System Flags (Bits 0-3)
#define SPRITE_ATTR_VISIBLE   (1 << 0)
#define SPRITE_ATTR_FLIPX     (1 << 1)
#define SPRITE_ATTR_FLIPY     (1 << 2)
#define SPRITE_ATTR_PRIORITY  (1 << 3)

// Anzeige-Typen (Bits 4-7)
#define SPRITE_FLAG_TETROMINO (1 << 4)
#define SPRITE_FLAG_SHADOW    (1 << 5)
#define SPRITE_FLAG_NEXT      (1 << 6)
#define SPRITE_FLAG_HOLD      (1 << 7)

typedef struct {
    s16 x, y;
    s16 offsetX, offsetY;
    u16 frame;
    u16 animation;
    u16 animTimer;
    u16 stateTimer;
    u16 attr;          // Kombiniertes Bitfield für System und Typ
    Sprite* vdpSprite;
} GameSprite;

extern GameSprite gameSprites[10];

void sprites_init();
void sprites_update();
void sprites_set_visible(u8 index, bool visible);