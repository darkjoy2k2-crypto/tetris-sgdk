#include <genesis.h>
#include <string.h>

#include "states/states.h"
#include "states/title.h"
#include "states/game_select.h"
#include "states/game.h"
#include "sound_manager.h"
#include "states/sound_test.h"
#include "states/gameover.h"
#include "states/highscore.h"
#include "states/options.h"
#include "menu_bg.h"
#include "fonts.h"

// --- GLOBALE VARIABLEN ---
GameState currentState = STATE_TITLE;
GameState lastState = STATE_NONE;
u16 joyState = 0;
u16 lastJoyState = 0;

StateUnion *sctx = NULL; 

// WICHTIG: Die Reihenfolge muss exakt der struct GlobalConfig in states.h entsprechen.
// Falls currentScore in states.h VOR playerName steht, müssen die Werte getauscht werden.
// Hier basierend auf deiner Fehlermeldung (config.currentScore bekommt "ABC"):
GlobalConfig config = {
    0,                                      // 1. currentScore
    "ABC",                                  // 2. playerName
    0,                                      // 3. randMode
    1,                                      // 4. speedLevel
    1,                                      // 5. garbageFreq
    1,                                      // 6. itemMode
    FLAG_SHADOW | FLAG_HOLD | FLAG_NEXT | 
    FLAG_MUSIC | FLAG_SOUND | FLAG_BG,      // 7. flags (DEBUG ist aus)
    6,                                     // 8. thresholdLR (Standard DAS)
    3                                       // 9. thresholdSD (Standard Softdrop)
};


StateHandler states[8]; 
HighscoreEntry highscores[10]; 

void initStateMachine() {
    states[STATE_TITLE]     = (StateHandler){ title_init,      title_init_draw,      title_update,      title_draw,      title_cleanup };
    states[STATE_SELECT]    = (StateHandler){ select_init,     select_init_draw,     select_update,     select_draw,     select_cleanup };
    states[STATE_GAME]      = (StateHandler){ game_init,       game_init_draw,       game_update,       game_draw,       game_cleanup };
    states[STATE_SOUNDTEST] = (StateHandler){ sound_test_init, sound_test_init_draw, sound_test_update, sound_test_draw, sound_test_cleanup };
    states[STATE_GAMEOVER]  = (StateHandler){ gameover_init,   gameover_init_draw,   gameover_update,   gameover_draw,   gameover_cleanup };
    states[STATE_HIGHSCORE] = (StateHandler){ highscore_init,  highscore_init_draw,  highscore_update,  highscore_draw,  highscore_cleanup };
    states[STATE_OPTIONS]   = (StateHandler){ options_init,    options_init_draw,    options_update,    options_draw,    options_cleanup };
}

void initHighscores() {
    char* names[] = {"PET", "SGK", "CPU", "VDP", "ACE", "SKY", "DAN", "EVA", "MAX", "JOE"};
    for (u16 i = 0; i < 10; i++) {
        strncpy(highscores[i].name, names[i], 3);
        highscores[i].name[3] = '\0';
        highscores[i].score = (10 - i) * 1000;
    }
}

int main() {
    // 1. Hardware-Basis-Inits
    JOY_init();
    
    // Speicher für die Union reservieren
    sctx = MEM_alloc(sizeof(StateUnion));
    if (sctx == NULL) {
        VDP_drawText("FATAL: MEMORY FULL", 10, 10);
        while(1); 
    }
    memset(sctx, 0, sizeof(StateUnion));

    if (IS_PAL_SYSTEM) {
        SET_FLAG(config.flags, FLAG_IS_PAL);
        VDP_setScreenHeight240();
    } else {
        CLEAR_FLAG(config.flags, FLAG_IS_PAL);
        VDP_setScreenHeight224();
    }

    // 2. Ressourcen laden
    VDP_loadFont(&TS_FONT_CLEAR, CPU);
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    VDP_setTextPalette(PAL3);
    
    initHighscores();
    initStateMachine();
    

    // 3. Hintergrund-System initialisieren
    menu_bg_init(); 
    menu_bg_set_active(true);
    menu_bg_set_mode(BG_MODE_MENU);

    joyState = JOY_readJoypad(JOY_1);
    lastJoyState = joyState; 
    
    SOUND_init(); 
    SOUND_playMusic();

    // 4. Hauptschleife
    while(1) {
        joyState = JOY_readJoypad(JOY_1);

        // State-Wechsel-Logik
        if (currentState != lastState) {
            if (lastState != STATE_NONE) {
                states[lastState].cleanup();
            }

            // RAM-Bereich für den neuen State säubern
            memset(sctx, 0, sizeof(StateUnion));

            lastJoyState = joyState; 
            states[currentState].init();
            states[currentState].init_draw();
            lastState = currentState;
        }

        // State-spezifische Logik & Grafik
        states[currentState].update();
        states[currentState].draw();
        
        // Hintergrund-Animation (Persistent über States hinweg)
        menu_bg_update();

        lastJoyState = joyState;

        // VBlank-Synchronisation
        SYS_doVBlankProcess();
    }

    return 0;
}