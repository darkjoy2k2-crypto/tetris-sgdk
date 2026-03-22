#include <genesis.h>
#include <string.h>
#include <kdebug.h> // Erforderlich für KLog/KDebug-Ausgaben in BlastEm

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
#include "states/save_manager.h"

// --- GLOBALE VARIABLEN ---
GameState currentState = STATE_TITLE;
GameState lastState = STATE_NONE;
u16 joyState = 0;
u16 lastJoyState = 0;

StateUnion *sctx = NULL; 

GlobalConfig config = {
    .serializable = {
        .currentScore = 0,
        .playerName = "ABC",
        .randMode = 0,
        .speedLevel = 1,
        .garbageFreq = 1,
        .itemMode = 1,
        .flags = FLAG_SHADOW | FLAG_HOLD | FLAG_NEXT | FLAG_MUSIC | FLAG_SOUND | FLAG_BG,
        .thresholdLRInitial = 6,
        .thresholdLRRepeat = 2,
        .thresholdSD = 3,
        .highscores = {
            {10000, "PET", 0}, {9000, "SGK", 0}, {8000, "CPU", 0},
            {7000, "VDP", 0},  {6000, "ACE", 0}, {5000, "SKY", 0},
            {4000, "DAN", 0},  {3000, "EVA", 0}, {2000, "MAX", 0},
            {1000, "JOE", 0}
        }
    },
    .preferredState = STATE_NONE,
    .sramop = SRAM_NONE
};

StateHandler states[9]; 

void initStateMachine() {
    states[STATE_TITLE]     = (StateHandler){ title_init,      title_init_draw,      title_update,      title_draw,      title_cleanup };
    states[STATE_SELECT]    = (StateHandler){ select_init,     select_init_draw,     select_update,     select_draw,     select_cleanup };
    states[STATE_GAME]      = (StateHandler){ game_init,       game_init_draw,       game_update,       game_draw,       game_cleanup };
    states[STATE_SOUNDTEST] = (StateHandler){ sound_test_init, sound_test_init_draw, sound_test_update, sound_test_draw, sound_test_cleanup };
    states[STATE_GAMEOVER]  = (StateHandler){ gameover_init,   gameover_init_draw,   gameover_update,   gameover_draw,   gameover_cleanup };
    states[STATE_HIGHSCORE] = (StateHandler){ highscore_init,  highscore_init_draw,  highscore_update,  highscore_draw,  highscore_cleanup };
    states[STATE_OPTIONS]   = (StateHandler){ options_init,    options_init_draw,    options_update,    options_draw,    options_cleanup };
    states[STATE_SAVE]      = (StateHandler){ saving_init,     saving_init_draw,     saving_update,     saving_draw,     saving_cleanup };
}

void initHighscores() {
    char* names[] = {"PET", "SGK", "CPU", "VDP", "ACE", "SKY", "DAN", "EVA", "MAX", "JOE"};
    for (u16 i = 0; i < 10; i++) {
        strncpy(config.highscores[i].name, names[i], 3);
        config.highscores[i].name[3] = '\0';
        config.highscores[i].score = (u32)((10 - i) * 1000);
        config.highscores[i].isNew = 0;
    }
}

int main(bool hardReset) {
    // 0. KDebug Test-Ausgabe (Erscheint sofort im BlastEm Terminal)
    KLog("!n");    KLog("--- LOG RESET: NEW SESSION STARTED ---");    
    KLog("--- BOOT SEQUENCE START ---");
    KLog_U1("Hard Reset: ", hardReset);

    // 1. Hardware-Basis-Inits
    JOY_init();
    
    // Speicher für die Union reservieren
    sctx = MEM_alloc(sizeof(StateUnion));
    if (sctx == NULL) {
        VDP_drawText("FATAL: MEMORY FULL", 10, 10);
        KLog("CRITICAL: MEM_alloc failed for sctx!");
        while(1); 
    }
    memset(sctx, 0, sizeof(StateUnion));

    // Region Check
    if (IS_PAL_SYSTEM) {
        VDP_setScreenHeight240();
        KLog("Region: PAL System detected.");
    } else {
        VDP_setScreenHeight224();
        KLog("Region: NTSC System detected.");
    }

    // 2. Ressourcen laden
    VDP_loadFont(&TS_FONT_CLEAR, CPU);
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    VDP_setTextPalette(PAL3);
    
    // Inits
    initHighscores();
    initStateMachine();
    
    // 3. Hintergrund-System initialisieren
    menu_bg_init(); 
    menu_bg_set_active(TRUE);
    menu_bg_set_mode(BG_MODE_MENU);

    joyState = JOY_readJoypad(JOY_1);
    lastJoyState = joyState; 
    
    SOUND_init(); 
    SOUND_playMusic();

    // Start-Zustand: SRAM Initialisierung über SaveManager
    KLog("MAIN: Switching to STATE_SAVE (SRAM_INIT)");
    currentState = STATE_SAVE;
    config.sramop = SRAM_INIT;
    
    // 4. Hauptschleife
    while(1) {
        joyState = JOY_readJoypad(JOY_1);

        // State-Wechsel-Logik
        if (currentState != lastState) {
            KLog_U1("STATE_CHANGE: Old State = ", lastState);
            KLog_U1("STATE_CHANGE: New State = ", currentState);

            if (lastState != STATE_NONE) {
                states[lastState].cleanup();
            }

            // RAM-Bereich für den neuen State säubern (Union-Gefahr verhindern)
            memset(sctx, 0, sizeof(StateUnion));

            lastJoyState = joyState; 
            states[currentState].init();
            states[currentState].init_draw();
            lastState = currentState;
        }

        // State-spezifische Logik & Grafik
        states[currentState].update();
        states[currentState].draw();
        
        // Hintergrund-Animation
        menu_bg_update();

        lastJoyState = joyState;

        // VBlank-Synchronisation
        SYS_doVBlankProcess();
    }

    return 0;
}