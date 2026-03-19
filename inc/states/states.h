#pragma once

#include <genesis.h>

// --- KONSTANTEN ---
#define BOARD_WIDTH      10
#define BOARD_HEIGHT     20

// --- 1. STATE-SPEZIFISCHE STRUKTUREN ---
// Alle Strukturen aus src/state/*.c hierher verschieben

typedef enum TitlePhase{
    PHASE_BLINK,
    PHASE_MENU
} TitlePhase;

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
    u16 thresholdLR;  // Lokale Kopie für Links/Rechts Sensitivität
    u16 thresholdSD;  // Lokale Kopie für Softdrop Sensitivität
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
    u16 flags;          // Temporäre Flags für das Menü
    bool needsRedraw;
} SelectContext;

typedef struct SoundTestContext {
    u16 currentID;
    bool needsDraw; // Flag für selektives Neuzeichnen
} SoundTestContext;

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
    u8 board[200];           // BOARD_WIDTH x BOARD_HEIGHT (200 Bytes)
    u8 bag[7];
    u8 bagIndex;
    char lastComment[20];
} GameContext;

typedef struct {
    u16 timer;
    bool textVisible;
} SaveContext;

// --- 2. DIE ZENTRALE UNION ---
// Alle Context-Strukturen teilen sich denselben RAM

typedef union StateUnion {
    TitleContext     title;
    SelectContext    select;
    GameContext      game;
    HighscoreContext highscore;
    OptionsContext   options;
    SoundTestContext soundtest;
    SaveContext      save;
} StateUnion;

// --- 3. GLOBALER APP-KONTEXT & CONFIG ---

typedef struct HighscoreEntry {
    u32 score;      // 4 Bytes
    char name[4];   // 4 Bytes
    u16 isNew;      // 2 Bytes (Padding eingebaut, damit jeder Eintrag 10 Bytes groß ist)
} HighscoreEntry;   // Gesamt: 10 Bytes -> Sicher für das Array


typedef enum GameState {
    STATE_NONE = 0,
    STATE_TITLE,
    STATE_SELECT,
    STATE_GAME,
    STATE_SOUNDTEST,
    STATE_GAMEOVER,
    STATE_HIGHSCORE,
    STATE_OPTIONS,
    STATE_SAVE
} GameState;

typedef struct GlobalConfig {
    u32 currentScore;   // Offset 0
    char playerName[4]; // Offset 4
    u16 randMode;       // Offset 8
    u16 speedLevel;     // Offset 10
    u16 garbageFreq;    // Offset 12
    u16 itemMode;       // Offset 14
    u16 flags;          // Offset 16
    u16 thresholdLR;    // Offset 18
    u16 thresholdSD;    // Offset 20
    
    // Das Array ist jetzt 10 * 10 = 100 Bytes groß
    HighscoreEntry highscores[10]; // Offset 22 bis 121

    // GameState folgt auf Offset 122 (Gerade Adresse -> Sicher!)
    GameState preferredState; 
} GlobalConfig;





typedef struct StateHandler {
    void (*init)();
    void (*init_draw)();
    void (*update)();
    void (*draw)();
    void (*cleanup)();
} StateHandler;

// System-Flags
#define FLAG_SHADOW      (1 << 0)
#define FLAG_HOLD        (1 << 1)
#define FLAG_NEXT        (1 << 2)
#define FLAG_IS_PAL      (1 << 3)
#define FLAG_SOUND       (1 << 4)
#define FLAG_MUSIC       (1 << 5)
#define FLAG_BG          (1 << 6)
#define FLAG_DEBUG       (1 << 7)

// Makros
#define SET_FLAG(v, f)    ((v) |= (f))
#define CLEAR_FLAG(v, f)  ((v) &= ~(f))
#define TOGGLE_FLAG(v, f) ((v) ^= (f))
#define GET_FLAG(v, f)    ((v) & (f))
#define SCALE_TO_PAL(v)   (((v) * 5) / 6)
#define GET_TICKS(v)      (GET_FLAG(config.flags, FLAG_IS_PAL) ? SCALE_TO_PAL(v) : (v))

// Externe Variablen
extern GameState currentState;
extern GameState lastState;
extern u16 joyState;
extern u16 lastJoyState;
extern GlobalConfig config;
extern HighscoreEntry highscores[10];
extern StateUnion *sctx; 

// Prototypen
void check_and_update_highscore(u32 finalScore);
