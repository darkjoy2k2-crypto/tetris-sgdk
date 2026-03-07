#pragma once
#include <genesis.h>

// --- BILDSCHIRM-LAYOUT ---
#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20
#define RENDER_X     12 
#define RENDER_Y     4  
#define UI_X         25 
#define NEXT_Y       5  
#define HOLD_Y       12 

extern const s8 PIECES[7][4][4][2];

typedef struct GameContext {
    u8 board[BOARD_WIDTH][BOARD_HEIGHT];
    s16 pieceX, pieceY;
    u16 type, rotation;
    
    s16 nextType, holdType;
    bool canHold;

    u16 moveTimer;
    u32 score;         // 32-bit unsigned integer (entspricht long)
    u16 level;
    u16 linesTotal;

    // --- CACHING FÜR PERFORMANCE (HUD) ---
    u32 lastScore;     // Verhindert unnötiges VDP_drawText
    u16 lastLevel;
    s16 lastNextType;
    s16 lastHoldType;

    u16 dasTimer;
    u16 dasDir;

    u16 comboCount;
    bool b2bActive;
    char lastComment[20];
    u16 commentTimer;

    u8 bag[7];
    u8 bagIndex;

    u16 clearTimer;             
    u16 garbageTimer;          
    u16 garbageNextThreshold;  
    bool pendingLines[BOARD_HEIGHT]; 
} GameContext;

extern GameContext* ctx;