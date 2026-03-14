#include <genesis.h>
#include "states/states.h"
#include "states/title.h"
#include "states/game_select.h"
#include "states/game.h"
#include "sound_manager.h"
#include "states/sound_test.h"
#include "states/gameover.h"
#include "states/highscore.h"
#include "states/options.h"
#include "menu_bg.h" // WICHTIG: Das neue Hintergrund-Modul
#include "fonts.h" // Dein Header mit dem Ressourcen-Verweis

// Hier wird der Speicher für die globalen Variablen reserviert
GameState currentState = STATE_TITLE;
GameState lastState = STATE_NONE;
u16 joyState = 0;
u16 lastJoyState = 0;

GlobalConfig config = {
    "ABC",
    0,
    0,
    1,
    1,
    1,
    FLAG_SHADOW | FLAG_HOLD | FLAG_NEXT | FLAG_MUSIC | FLAG_SOUND | FLAG_BG// Standard an
};

StateHandler states[8]; 

void initStateMachine() {
    states[STATE_TITLE]     = (StateHandler){      title_init,      title_init_draw,      title_update,      title_draw,      title_cleanup };
    states[STATE_SELECT]    = (StateHandler){     select_init,     select_init_draw,     select_update,     select_draw,     select_cleanup };
    states[STATE_GAME]      = (StateHandler){       game_init,       game_init_draw,       game_update,       game_draw,       game_cleanup };
    states[STATE_SOUNDTEST] = (StateHandler){ sound_test_init, sound_test_init_draw, sound_test_update, sound_test_draw, sound_test_cleanup };
    states[STATE_GAMEOVER]  = (StateHandler){   gameover_init,   gameover_init_draw,   gameover_update,   gameover_draw,   gameover_cleanup };
    states[STATE_HIGHSCORE] = (StateHandler){  highscore_init,  highscore_init_draw,  highscore_update,  highscore_draw,  highscore_cleanup };
    states[STATE_OPTIONS] =   (StateHandler){    options_init,    options_init_draw,    options_update,    options_draw,    options_cleanup };
}

HighscoreEntry highscores[10]; 

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
    
if (IS_PAL_SYSTEM) {
    SET_FLAG(config.flags, FLAG_IS_PAL);
    VDP_setScreenHeight240();
} else {
    CLEAR_FLAG(config.flags, FLAG_IS_PAL);
    VDP_setScreenHeight224();
}


    VDP_loadFont(&TS_FONT_CLEAR, CPU);
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    VDP_setTextPalette(PAL3);
    
    initHighscores();
    initStateMachine();
    
    // Hilfreich für NTSC/PAL Tests: Zeigt die CPU-Last an
    SYS_showFrameLoad(TRUE);

    menu_bg_init(); 
    menu_bg_set_active(true);
    menu_bg_set_mode(BG_MODE_MENU);

    joyState = JOY_readJoypad(JOY_1);
    lastJoyState = joyState; 
    
    SOUND_init(); 
    SOUND_playMusic();

    while(1) {
        joyState = JOY_readJoypad(JOY_1);

        if (currentState != lastState) {
            if (lastState != STATE_NONE) {
                states[lastState].cleanup();
            }
            lastJoyState = joyState; 
            states[currentState].init();
            states[currentState].init_draw();
            lastState = currentState;
        }

        states[currentState].update();
        states[currentState].draw();
        
        menu_bg_update();
        lastJoyState = joyState;

        // Wartet auf VBlank (50Hz bei PAL, 60Hz bei NTSC)
        SYS_doVBlankProcess();
    }

    return 0;
}