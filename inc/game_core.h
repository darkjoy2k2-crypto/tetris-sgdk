#pragma once
#include <genesis.h>

// --- BILDSCHIRM-LAYOUT ---
#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20
#define RENDER_X     12   // Startposition des Spielfelds (X)
#define RENDER_Y     4    // Startposition des Spielfelds (Y)
#define UI_X         25   // X-Position für Next/Hold Fenster
#define NEXT_Y       5    // Y-Position für Next Fenster
#define HOLD_Y       12   // Y-Position für Hold Fenster

extern const s8 PIECES[7][4][4][2];

typedef struct GameContext {
    u8 board[BOARD_WIDTH][BOARD_HEIGHT];
    s16 pieceX, pieceY;
    u16 type, rotation;
    
    s16 nextType, holdType;
    bool canHold;

    u16 moveTimer;
    u32 score;
    u16 level;
    u16 linesTotal;

    u16 dasTimer;
    u16 dasDir;

    u16 comboCount;
    bool b2bActive;
    char lastComment[20];
    u16 commentTimer;

    u8 bag[7];
    u8 bagIndex;

    u16 clearTimer;             
    u16 garbageTimer;          // Frame-Zähler
    u16 garbageNextThreshold;  // Ziel-Frames (zwischen 600 und 1200 bei 60 FPS)
    bool pendingLines[BOARD_HEIGHT]; 
} GameContext;

extern GameContext* ctx;