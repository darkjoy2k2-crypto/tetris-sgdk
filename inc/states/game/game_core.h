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

// --- Effekt-Dauern (Zeitbasiert: 5 Sekunden = 300 Frames bei 60Hz) ---
#define DUR_NO_ROTATE_TICKS    300  
#define DUR_REVERSED_TICKS     300  
#define DUR_HOLD_LOCK_TICKS    300  
#define DUR_HIDE_NEXT_TICKS    300  
#define DUR_SHADOW_TICKS       300  
#define DUR_FREEZE_TICKS       300  

// --- Effekt-Dauern (Stückbasiert: 5 Pieces) ---
#define DUR_FULLSPEED_SPAWNS   5    
#define DUR_SAME_TILES_SPAWNS  5    
#define DUR_I_RAIN_SPAWNS      5

extern GameContext* ctx;

// Zwingend 'static inline' damit der Code direkt in die aufrufende Funktion kopiert wird
static inline void set_board_tile(s16 x, s16 y, u8 val) {
    ctx->board[x + ((y << 3) + (y << 1))] = val;
}

static inline u8 get_board_tile(s16 x, s16 y) {
    return ctx->board[x + ((y << 3) + (y << 1))];
}

static inline bool is_within_board(s16 x, s16 y) {
    return (x >= 0 && x < 10 && y >= 0 && y < 20);
}