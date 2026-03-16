#pragma once
#pragma once

#include <genesis.h>
#include "states/states.h" // Enthält das Makro GET_TICKS

// Animations-Geschwindigkeiten (Ticks)
#define NOROT_NEXTFRAME       10
#define NOROT_NETXANIM        60
#define SKULL_NEXTFRAME       2
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

// --- Sprite Z-Order & Priority ---
#define DEPTH_FOREGROUND     0    // Ganz vorne (z.B. No-Rotate Icon)
#define DEPTH_DEFAULT        128  // Standard Ebene
#define DEPTH_BACKGROUND     255  // Hinter High-Prio Tiles (z.B. Speed-Schädel)

#define PRIO_LOW             FALSE // VDP Low Priority
#define PRIO_HIGH            TRUE  // VDP High Priority

typedef struct {
    Sprite* vdpSprite;    // 4 Bytes - Jetzt auf Offset 0 (Sicher!)
    s16 x, y;             // 4 Bytes
    s16 offsetX, offsetY; // 4 Bytes
    u16 frame;            // 2 Bytes
    u16 animation;        // 2 Bytes
    u16 animTimer;        // 2 Bytes
    u16 stateTimer;       // 2 Bytes
    u16 attr;             // 2 Bytes
    s16 animDir;          // 2 Bytes
    u8  type;             // 1 Byte
    u8  padding;          // 1 Byte - Füllt auf 26 Bytes Gesamtlänge auf
} GameSprite;

// Deklarationen für den Manager (sprite.c)
extern GameSprite gameSprites[10];

void sprites_init();
void sprites_update();
void sprites_set_visible(u8 index, bool visible);