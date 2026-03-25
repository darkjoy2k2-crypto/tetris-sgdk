#pragma once

#include <genesis.h>

// --- KONSTANTEN ---
#define BOARD_WIDTH      10
#define BOARD_HEIGHT     20
#define SPEED_LEVEL_MAX  9
#define GARBAGE_FREQ_MAX 9

// --- 1. ENUMS ---

typedef enum GameState {
    STATE_NONE = 0,
    STATE_TITLE,
    STATE_SELECT,
    STATE_GAME,
    STATE_VS,
    STATE_SOUNDTEST,
    STATE_GAMEOVER,
    STATE_HIGHSCORE,
    STATE_OPTIONS,
    STATE_SAVE,
    STATE_CHALLENGE,
    STATE_GFXTEST
} GameState;

typedef enum TitlePhase {
    PHASE_BLINK,
    PHASE_MENU
} TitlePhase;

// --- 2. STATE-SPEZIFISCHE STRUKTUREN ---

typedef struct TitleContext {
    TitlePhase phase;
    u16 cursor;
    u16 blinkTimer;
    bool textVisible;
    bool lastTextVisible;
    u16 idleTimer;
    bool needsRedraw;
} TitleContext;

typedef struct HighscoreContext {
    u16 scrollOffset;
    u16 displayTimer;
    bool needsRefresh;
} HighscoreContext;

typedef struct OptionsContext {
    u16 cursor;
    u16 subCursor;
    u16 flags;
    u16 thresholdLRInitial; // Zeit bis erste Auto-Bewegung
    u16 thresholdLRRepeat;  // Zeit zwischen Folge-Bewegungen
    u16 thresholdSD;        // Softdrop-Geschwindigkeit
    bool needsRedraw;
} OptionsContext;

typedef struct SelectContext {
    u16 cursor;
    char name[4];
    u16 nameCharIdx;
    u16 randMode;
    u16 speedLevel;
    u16 garbageFreq;
    u16 itemMode;
    u16 flags;
    bool needsRedraw;
} SelectContext;

typedef struct SoundTestContext {
    u16 currentID;
    bool needsDraw;
} SoundTestContext;

typedef struct GameContext {
    u32 score;         
    u32 lastScore;     
    u32 boardFlags;     
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
    u16 dasNextThreshold;
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
    u16 flags;                  
    u8 board[200];           
    u8 clearingLineBackup[200]; // Speichert Originalblöcke während Clear-Animation
    u32 clearingLineMask;       // Bitmask für Zeilen in der Blink-Animation (vor PENDING)
    u8 bag[7];
    u8 bagIndex;
    char lastComment[20];
} GameContext;

typedef struct SaveContext {
    u16 timer;
    bool textVisible;
    bool isLoading;
    u16 errorOccurred; // 0 = OK, 1 = Fehler
} SaveContext;

typedef struct ChallengeContext {
    u8 cursor_x;        // 0..15
    u8 cursor_y;        // 0..7
    u8 current_level_id; // y*16 + x
    bool needsRedraw;
    s8 holdDir;         // -2 up, -1 left, 0 none, 1 right, 2 down
    u16 holdTimer;
    u16 holdNextThreshold;
    u32 frontier_open[4]; // 128-bit: expanded neighbors of cleared, excluding cleared
} ChallengeContext;

typedef struct GfxTestContext {
    u8 bag[7];
    u8 bagIndex;
    bool needsRedraw;
} GfxTestContext;

typedef struct VsContext {
    GameContext left;
    GameContext right;
    u16 joy1;
    u16 joy1Last;
    u16 joy2;
    u16 joy2Last;
    bool leftDead;
    bool rightDead;
    bool rightAiEnabled;
    bool leftNeedsRedraw;
    bool rightNeedsRedraw;
    bool rightAiHasBest;
    bool rightAiPlanReady;
    bool rightAiHardDropArmed;
    u16  rightAiState;
    u16  rightAiScanRot;
    s16  rightAiTargetX;
    s16  rightAiScanX;
    s16  rightAiTargetY;
    u16  rightAiTargetRot;
    s16  rightAiPlannedType;
    s32  rightAiBestScore;
    u16  rightAiThinkBudget;
    u16  rightAiThinkTimer;
    bool rightAiUseSoftDrop;
    u16  rightAiPulseTimer;
    u16  leftGarbagePending;
    u16  rightGarbagePending;
    s16  leftGameOverAnimRow;
    s16  rightGameOverAnimRow;
    u16  gameOverTimer;
    bool matchOver;
    u16  winnerSide;
    bool leftLastRotate;
    bool rightLastRotate;
    u16  leftEventTimer;
    u16  rightEventTimer;
    char leftEventText[24];
    char rightEventText[24];
} VsContext;

// --- 3. DIE ZENTRALE UNION ---

typedef union StateUnion {
    TitleContext     title;
    SelectContext    select;
    GameContext      game;
    VsContext        vs;
    HighscoreContext highscore;
    OptionsContext   options;
    SoundTestContext soundtest;
    SaveContext      save;
    ChallengeContext challenge;
    GfxTestContext   gfxtest;
} StateUnion;

// --- 4. GLOBALER APP-KONTEXT & CONFIG ---

typedef enum SramOp{
    SRAM_NONE,
    SRAM_INIT,
    SRAM_LOAD,
    SRAM_SAVE
} SramOp;

#define GAME_MODE_FREEGAME        0
#define GAME_MODE_CHALLENGE       1

#define CHALLENGE_RESULT_NONE     0
#define CHALLENGE_RESULT_SUCCESS  1
#define CHALLENGE_RESULT_FAIL     2

typedef struct __attribute__((packed)) HighscoreEntry {
    u32 score;      // 4
    char name[4];   // 4
    u16 isNew;      // 2 -> Gesamt 10 Bytes (Aligned)
} HighscoreEntry;

// Der persistente Teil für den SRAM
typedef struct __attribute__((packed)) Serializable {
    u32 currentScore;
    char playerName[4];
    u16 randMode;
    u16 speedLevel;
    u16 garbageFreq;
    u16 itemMode;
    u16 flags;
    u16 thresholdLRInitial;
    u16 thresholdLRRepeat;
    u16 thresholdSD;
    HighscoreEntry highscores[10];
    
    /* Challenge Mode Progress (128-bit each: 4x u32) */
    u32 challenge_unlocked[4];  // unlocked levels
    u32 challenge_cleared[4];   // cleared levels
} Serializable;

// Runtime-only globals/config that must never be persisted to SRAM.
// Add any future global gameplay/session fields here, not in Serializable.
typedef struct RuntimeConfig {
    u16 gameMode;
    u16 challengeLevelId;
    u16 challengeResult;
} RuntimeConfig;

typedef struct GlobalConfig {
    union {
        Serializable serializable; // Für den Save-Manager
        struct {                   // Anonym für direkten Zugriff (kein Refactoring nötig)
            u32 currentScore;
            char playerName[4];
            u16 randMode;
            u16 speedLevel;
            u16 garbageFreq;
            u16 itemMode;
            u16 flags;
            u16 thresholdLRInitial;
            u16 thresholdLRRepeat;
            u16 thresholdSD;
            HighscoreEntry highscores[10];
            u32 challenge_unlocked[4];
            u32 challenge_cleared[4];
        };
    };

    // --- NOT SERIALIZABLE ---
    RuntimeConfig runtime;
    GameState preferredState;
    SramOp sramop;
} GlobalConfig;

// --- 5. SYSTEM-FLAGS & MAKROS ---

#define FLAG_SHADOW      (1 << 0)
#define FLAG_HOLD        (1 << 1)
#define FLAG_NEXT        (1 << 2)
#define FLAG_SOUND       (1 << 3)
#define FLAG_MUSIC       (1 << 4)
#define FLAG_BG          (1 << 5)
#define FLAG_DEBUG       (1 << 6)

#define SET_FLAG(v, f)    ((v) |= (f))
#define CLEAR_FLAG(v, f)  ((v) &= ~(f))
#define TOGGLE_FLAG(v, f) ((v) ^= (f))
#define GET_FLAG(v, f)    ((v) & (f))
#define SCALE_TO_PAL(v)   (((v) * 5) / 6)
#define GET_TICKS(v)      (IS_PAL_SYSTEM ? SCALE_TO_PAL(v) : (v))
// --- 6. EXTERNE VARIABLEN & PROTOTYPEN ---

typedef struct StateHandler {
    void (*init)();
    void (*init_draw)();
    void (*update)();
    void (*draw)();
    void (*cleanup)();
} StateHandler;

extern GameState currentState;
extern GameState lastState;
extern u16 joyState;
extern u16 lastJoyState;
extern GlobalConfig config;
extern StateUnion *sctx; 

void check_and_update_highscore(u32 finalScore);