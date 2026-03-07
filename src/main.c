#include <genesis.h>
#include "states.h"

// Alle State-Header inkludieren
#include "title.h"
#include "game_select.h"
#include "game.h"
#include "sound_test.h"
#include "gameover.h"

// Definitionen der globalen Variablen (Hier wird der Speicher reserviert)
GameState currentState = STATE_TITLE;
GameState lastState = STATE_NONE;
u16 joyState = 0;
u16 lastJoyState = 0;

// Das Array mit dem Bauplan "StateHandler"
StateHandler states[6]; 

void initStateMachine() {
    // Zuordnung der Funktionen zu den Zuständen
    states[STATE_TITLE]     = (StateHandler){ title_init, title_update, title_cleanup };
    states[STATE_SELECT]    = (StateHandler){ select_init, select_update, select_cleanup };
    states[STATE_GAME]      = (StateHandler){ game_init, game_update, game_cleanup };
    states[STATE_SOUNDTEST] = (StateHandler){ sound_test_init, sound_test_update, sound_test_cleanup };
    states[STATE_GAMEOVER]  = (StateHandler){ gameover_init, gameover_update, gameover_cleanup };
}

int main() {
    // SGDK Initialisierung
    JOY_init();
    initStateMachine();

    while(1) {
        // 1. Input lesen
        joyState = JOY_readJoypad(JOY_1);

        // 2. State-Wechsel Logik
        if (currentState != lastState) {
            // Cleanup des alten States (nur wenn es nicht der allererste ist)
            if (lastState != STATE_NONE) {
                states[lastState].cleanup();
            }

            // Initialisierung des neuen States
            states[currentState].init();
            lastState = currentState;
        }

        // 3. Update des aktuellen States
        states[currentState].update();

        // 4. Input für den nächsten Frame merken
        lastJoyState = joyState;

        // Warten auf VBlank
        SYS_doVBlankProcess();
    }

    return 0;
}