#pragma once
#define _STATES_H_

#include <genesis.h>

// Der Bauplan für die State-Maschine
typedef struct StateHandler{
    void (*init)();
    void (*init_draw)();
    void (*update)();
    void (*draw)();
    void (*cleanup)();
} StateHandler;

// Die Zustands-Liste
typedef enum GameState{
    STATE_NONE = 0,
    STATE_TITLE = 1,
    STATE_SELECT = 2,
    STATE_GAME = 3,
    STATE_SOUNDTEST = 4,
    STATE_GAMEOVER = 5,
    STATE_HIGHSCORE = 6,
    STATE_OPTIONS = 7
} GameState;

typedef struct HighscoreEntry{
    u32 score;
    char name[4];
    bool isNew; // Dieses Feld wird oben genutzt
} HighscoreEntry;

// NEU: Die Struktur für deine globalen Spieleinstellungen
// In der struct GlobalConfig in states.h ergänzen:
typedef struct GlobalConfig {
    char playerName[4];
    u32 currentScore;
    u16 randMode;
    u16 speedLevel;
    u16 garbageFreq;
    u16 itemMode;
    u16 flags;          // HIER sitzen jetzt alle Bools zusammen!
} GlobalConfig;

// Bit-Masken für das Flags-System
#define FLAG_SHADOW      (1 << 0)  // 00000001
#define FLAG_HOLD        (1 << 1)  // 00000010
#define FLAG_NEXT        (1 << 2)  // 00000100
#define FLAG_IS_PAL      (1 << 3)  // 00001000
#define FLAG_SOUND       (1 << 4)  // 00010000
#define FLAG_MUSIC       (1 << 5)  // 00100000
#define FLAG_BG          (1 << 6)  // 00100000

// ... du hast noch Platz bis Bit 15!

// Hilfs-Makros für die Abfrage (erhöht die Lesbarkeit massiv)
#define SET_FLAG(v, f)    ((v) |= (f))
#define CLEAR_FLAG(v, f)  ((v) &= ~(f))
#define TOGGLE_FLAG(v, f) ((v) ^= (f))
#define GET_FLAG(v, f)    ((v) & (f))

// Umrechnung von 60Hz (NTSC) auf 50Hz (PAL)
// Wert_PAL = (Wert_NTSC * 50) / 60  => (Wert * 5) / 6
#define SCALE_TO_PAL(v)   (((v) * 5) / 6)
#define GET_TICKS(v)      (GET_FLAG(config.flags, FLAG_IS_PAL) ? SCALE_TO_PAL(v) : (v))


// Globale Variablen (Deklaration)
extern GameState currentState;
extern GameState lastState;
extern u16 joyState;
extern u16 lastJoyState;
extern GlobalConfig config;
extern HighscoreEntry highscores[10]; // N
void check_and_update_highscore(u32 finalScore);
