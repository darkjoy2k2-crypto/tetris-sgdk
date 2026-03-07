#ifndef _STATES_H_
#define _STATES_H_

#include <genesis.h>

// Der Bauplan für die State-Maschine
typedef struct {
    void (*init)();
    void (*update)();
    void (*cleanup)();
} StateHandler;

// Die Zustands-Liste
typedef enum {
    STATE_NONE = 0,
    STATE_TITLE = 1,
    STATE_SELECT = 2,
    STATE_GAME = 3,
    STATE_SOUNDTEST = 4,
    STATE_GAMEOVER = 5
} GameState;

// NEU: Die Struktur für deine globalen Spieleinstellungen
typedef struct {
    char playerName[4]; // 3 Buchstaben + \0
    u16 randMode;       // 0: Fair, 1: Chaos
    u16 speedLevel;     // 0-3
    u16 garbageFreq;    // 0-3
    bool showShadow;
    bool allowHold;
    bool showNext;
} GlobalConfig;

// Globale Variablen (Deklaration)
extern GameState currentState;
extern GameState lastState;
extern u16 joyState;
extern u16 lastJoyState;
extern GlobalConfig config; // <--- Das fehlte dem Compiler!

#endif