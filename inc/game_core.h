#pragma once
#include <genesis.h>

#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20
#define RENDER_X     15 
#define RENDER_Y     4  
#define UI_X         26 
#define NEXT_Y       5  
#define HOLD_Y       12 

extern const s8 PIECES[7][4][4][2];

typedef struct GameContext {
    u32 score;         
    u32 lastScore;     

    s16 pieceX;
    s16 pieceY;
    s16 ghostY;        
    s16 nextType;
    s16 holdType;
    s16 lastNextType;
    s16 lastHoldType;

    u16 type;
    u16 rotation;
    u16 moveTimer;
    u16 level;
    u16 startLevel;
    u16 lastLevel;
    u16 linesTotal;
    u16 lastLinesNext;
    u16 lastComboCount; // Zum Vergleichen für das UI-Redraw
    u16 dasTimer;
    u16 dasDir;
    u16 comboCount;
    u16 clearTimer;             
    u16 garbageTimer;          
    u16 garbageNextThreshold;  
    u16 commentTimer;

    bool canHold;
    bool b2bActive;
    bool pendingLines[BOARD_HEIGHT]; 

    u8 board[BOARD_WIDTH][BOARD_HEIGHT];
    u8 bag[7];
    u8 bagIndex;

    char lastComment[20];
} GameContext;

extern GameContext* ctx;