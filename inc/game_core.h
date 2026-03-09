#pragma once
#include <genesis.h>

#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20
#define RENDER_X     15 
#define RENDER_Y     5  
#define UI_X         27 
#define NEXT_Y       6  
#define HOLD_Y       10 

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
    u16 itemSlot;
    u16 itemType; 
    u16 itemSpawnCounter;
    s16 badEffectTimer;     // Wie lange hält der Effekt (in Pieces oder Frames)
    u16 activeBadEffect;    // Welcher Effekt ist aktiv (0 = keiner)
    s16 forcedPieceType;    // Für den "Same Tiles" Effekt 
    u16 lastActiveBadEffect; 
    s16 lastBadEffectTimer;
    s16 sortingRow;    // <--- Diese Zeile muss rein!

    bool canHold;
    bool b2bActive;
    bool pendingLines[BOARD_HEIGHT]; 
    bool controlsReversed;
    bool rotationLocked;
    bool holdBlocked;
    bool nextHidden;
    bool heartTriggered; // Neues Flag

    u8 board[BOARD_WIDTH][BOARD_HEIGHT];
    u8 bag[7];
    u8 bagIndex;

    char lastComment[20];
} GameContext;

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

extern GameContext* ctx;