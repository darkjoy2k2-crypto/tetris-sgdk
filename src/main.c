#include <genesis.h>
#include "states.h"
#include "title.h"
#include "game_select.h"
#include "game.h"
#include "sound_test.h"
#include "gameover.h"
#include "highscore.h"

// Hier wird der Speicher für die globalen Variablen reserviert
GameState currentState = STATE_TITLE;
GameState lastState = STATE_NONE;
u16 joyState = 0;
u16 lastJoyState = 0;

// Hier setzen wir die Standardwerte für das Spiel
GlobalConfig config = {
    "JOE",  // Standardname
    0,      // Fair Random
    2,      // Medium Speed
    1,      // Slow Garbage
    true,   // Shadow On
    true,   // Hold On
    true    // Next On
};

StateHandler states[7]; 

void initStateMachine() {
    states[STATE_TITLE]     = (StateHandler){ title_init, title_update, title_cleanup };
    states[STATE_SELECT]    = (StateHandler){ select_init, select_update, select_cleanup };
    states[STATE_GAME]      = (StateHandler){ game_init, game_update, game_cleanup };
    states[STATE_SOUNDTEST] = (StateHandler){ sound_test_init, sound_test_update, sound_test_cleanup };
    states[STATE_GAMEOVER]  = (StateHandler){ gameover_init, gameover_update, gameover_cleanup };
}


HighscoreEntry highscores[10]; 

void initHighscores() {
    char* names[] = {"PET", "SGK", "CPU", "VDP", "ACE", "SKY", "DAN", "EVA", "MAX", "JOE"};
    for (u16 i = 0; i < 10; i++) {
        strncpy(highscores[i].name, names[i], 3);
        highscores[i].name[3] = '\0';
        // 10.000 (Platz 1) bis 1.000 (Platz 10)
        highscores[i].score = (10 - i) * 1000;
    }
}

int main() {
    JOY_init();
    initHighscores(); // Standardwerte setzen
    initStateMachine();

    // Kaltstart-Fix: Input synchronisieren
    joyState = JOY_readJoypad(JOY_1);
    lastJoyState = joyState; 

    while(1) {
        joyState = JOY_readJoypad(JOY_1);

        if (currentState != lastState) {
            if (lastState != STATE_NONE) {
                states[lastState].cleanup();
            }
            // Beim Wechsel lastJoyState angleichen, um "Bleeding" zu verhindern
            lastJoyState = joyState; 
            states[currentState].init();
            lastState = currentState;
        }

        states[currentState].update();
        lastJoyState = joyState;

        SYS_doVBlankProcess();
    }
    return 0;
}