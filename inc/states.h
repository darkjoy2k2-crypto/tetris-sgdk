#pragma once
#define _STATES_H_

#include <genesis.h>

// Der Bauplan für die State-Maschine
typedef struct StateHandler{
    void (*init)();
    void (*update)();
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
    STATE_HIGHSCORE = 6 // NEU
} GameState;

typedef struct HighscoreEntry{
    u32 score;
    char name[4];
} HighscoreEntry;

// NEU: Die Struktur für deine globalen Spieleinstellungen
typedef struct GlobalConfig{
    char playerName[4]; // 3 Buchstaben + \0
    u32 currentScore;   // NEU: Damit der GameOver-Screen den letzten Score kennt
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
extern GlobalConfig config;
extern HighscoreEntry highscores[10]; // N
void check_and_update_highscore(u32 finalScore);
