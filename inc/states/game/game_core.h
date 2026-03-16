#pragma once
#include <genesis.h>

#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20
#define RENDER_X     15 
#define RENDER_Y      5  
#define UI_X         27 
#define NEXT_Y        6  
#define HOLD_Y       10 
#include "states/states.h"
#define UI_X_NEXT 26  // Spalte für Next-Fenster
#define UI_Y_NEXT 4   // Zeile für Next-Fenster
#define UI_X_HOLD 26  // Spalte für Hold-Fenster
#define UI_Y_HOLD 10  // Zeile für Hold-Fenster

extern const s8 PIECES[7][4][4][2];

typedef struct GameContext {
    // --- 32-Bit (u32 / f32) ---
    u32 score;         
    u32 lastScore;     
    u32 boardFlags;             // Board-Zustand (f32): NEEDS_DRAW (Bit 0), PENDING_LINES (Bit 1-20)

    // --- 16-Bit (u16 / s16 / f16) ---
    u16 level;
    u16 lastLevel;
    u16 startLevel;
    u16 linesTotal;
    u16 lastLinesNext;
    u16 comboCount;
    u16 lastComboCount;
    s16 pieceX;
    s16 pieceY;
    s16 ghostY;        
    u16 type;
    u16 rotation;
    s16 nextType;
    s16 lastNextType;
    s16 holdType;
    s16 lastHoldType;
    u16 moveTimer;
    u16 dasTimer;
    u16 dasDir;
    u16 clearTimer;             
    u16 garbageTimer;          
    u16 garbageNextThreshold;  
    u16 commentTimer;
    u16 itemSpawnCounter;
    u16 itemSlot;
    u16 itemType; 
    s16 badEffectTimer;     
    u16 activeBadEffect;    
    u16 lastActiveBadEffect; 
    s16 lastBadEffectTimer;
    s16 forcedPieceType;    
    s16 sortingRow; 
    u16 flags;                  // Verhaltens-Flags (f16): CAN_HOLD, B2B, REVERSED, etc.

    // --- 8-Bit (u8 / Arrays / Strings) ---
    u8 board[10][20];           // BOARD_WIDTH x BOARD_HEIGHT (200 Bytes)
    u8 bag[7];
    u8 bagIndex;
    char lastComment[20];
} GameContext;

// --- Effekt-Definitionen ---
#define EFFECT_NONE         0
#define EFFECT_FULLSPEED    1
#define EFFECT_SAME_TILES   2
#define EFFECT_REVERSED     3
#define EFFECT_NO_ROTATE    4
#define EFFECT_HOLD_LOCK    5
#define EFFECT_HIDE_NEXT    6
#define EFFECT_I_RAIN       7
#define EFFECT_FREEZE       8
#define EFFECT_MULTIPLIER   9
#define EFFECT_RAINBOW      10
#define EFFECT_SHADOW_BOARD 11

// --- Verhaltens-Flags (ctx->flags / u16 / f16) ---
#define GF_CAN_HOLD      (1U << 0)
#define GF_B2B_ACTIVE    (1U << 1)
#define GF_REVERSED      (1U << 2)
#define GF_ROT_LOCKED    (1U << 3)
#define GF_HOLD_LOCKED   (1U << 4)
#define GF_NEXT_HIDDEN   (1U << 5)
#define GF_HEART_TRIG    (1U << 6)
#define GF_SKULL_TRIG    (1U << 7)
#define GF_CLEARING      (1U << 8)

// --- Board-Flags (ctx->boardFlags / u32 / f32) ---
#define GF_NEEDS_DRAW    (1UL << 0)

// Masken für Pending Lines (Bits 1 bis 20)
#define GF_PENDING_SHIFT 1
#define GF_PENDING_MASK  (0xFFFFFUL << GF_PENDING_SHIFT)

// Hilfs-Makros für Pending Lines
#define SET_LINE_PENDING(y) (ctx->boardFlags |= (1UL << (y + GF_PENDING_SHIFT)))
#define GET_LINE_PENDING(y) (ctx->boardFlags & (1UL << (y + GF_PENDING_SHIFT)))
#define CLEAR_ALL_PENDING() (ctx->boardFlags &= ~GF_PENDING_MASK)

extern GameContext* ctx;