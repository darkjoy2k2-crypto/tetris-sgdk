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
    // --- Scoring & Stats ---
    u32 score;         
    u32 lastScore;     
    u16 level;
    u16 lastLevel;
    u16 startLevel;
    u16 linesTotal;
    u16 lastLinesNext;
    u16 comboCount;
    u16 lastComboCount;

    // --- Piece & Position Data ---
    s16 pieceX;
    s16 pieceY;
    s16 ghostY;        
    u16 type;
    u16 rotation;
    s16 nextType;
    s16 lastNextType;
    s16 holdType;
    s16 lastHoldType;

    // --- Timers & Logic ---
    u16 moveTimer;
    u16 dasTimer;
    u16 dasDir;
    u16 clearTimer;             
    u16 garbageTimer;          
    u16 garbageNextThreshold;  
    u16 commentTimer;
    u16 itemSpawnCounter;

    // --- Effects & Items ---
    u16 itemSlot;
    u16 itemType; 
    s16 badEffectTimer;     
    u16 activeBadEffect;    
    u16 lastActiveBadEffect; 
    s16 lastBadEffectTimer;
    s16 forcedPieceType;    

    // --- Animation State ---
    s16 sortingRow;         // Der "Celeste"-Marker für die Reihen-Animationen
    bool needsBoardDraw;    // Das Dirty-Flag für flüssige 60 FPS
    bool heartTriggered; 
    bool skullTriggered; // Diese Zeile hier ergänzen

    // --- Flags ---
    bool canHold;
    bool b2bActive;
    bool pendingLines[BOARD_HEIGHT]; 
    bool controlsReversed;
    bool rotationLocked;
    bool holdBlocked;
    bool nextHidden;

        // --- World Data ---
    u8 board[BOARD_WIDTH][BOARD_HEIGHT];
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

extern GameContext* ctx;