#pragma once
#pragma once

#include <genesis.h>
#include "states/states.h" // Enthält das Makro GET_TICKS

// Animations-Geschwindigkeiten (Ticks)
#define NOROT_NEXTFRAME       10
#define NOROT_NETXANIM        60
#define SKULL_NEXTFRAME       5
#define EFFECT_DURATION_NOROT GET_TICKS(180)

// Sprite-Verhaltens-Typen
#define SPRITE_TYPE_NOROTATE  0
#define SPRITE_TYPE_SKULL     1

// System Attr Flags (Bits 0-3)
#define SPRITE_ATTR_VISIBLE   (1 << 0)
#define SPRITE_ATTR_FLIPX     (1 << 1)
#define SPRITE_ATTR_FLIPY     (1 << 2)
#define SPRITE_ATTR_PRIORITY  (1 << 3)

// Anzeige-Logik Flags (Bits 4-7)
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
    u16 attr;
    s16 animDir;
    u8  type;
    Sprite* vdpSprite;
} GameSprite;

// Deklarationen für den Manager (sprite.c)
extern GameSprite gameSprites[10];

void sprites_init();
void sprites_update();
void sprites_set_visible(u8 index, bool visible);