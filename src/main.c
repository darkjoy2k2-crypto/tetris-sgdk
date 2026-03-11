#include <genesis.h>
#include "states/states.h"
#include "states/title.h"
#include "states/game_select.h"
#include "states/game.h"
#include "sound_manager.h"
#include "states/sound_test.h"
#include "states/gameover.h"
#include "states/highscore.h"
#include "menu_bg.h" // WICHTIG: Das neue Hintergrund-Modul
#include "fonts.h" // Dein Header mit dem Ressourcen-Verweis

// Hier wird der Speicher für die globalen Variablen reserviert
GameState currentState = STATE_TITLE;
GameState lastState = STATE_NONE;
u16 joyState = 0;
u16 lastJoyState = 0;

// Hier setzen wir die Standardwerte für das Spiel
GlobalConfig config = {
    "JOE",          // playerName
    0,              // currentScore
    0,              // randMode: 0 = Fair, 1 = Chaos
    1,              // speedLevel: 1 = Slow
    1,              // garbageFreq: 1 = Slow
    1,              // itemMode: On
    true,           // showShadow: On
    true,           // allowHold: On
    true            // showNext: On
};

StateHandler states[7]; 

void initStateMachine() {
    states[STATE_TITLE]     = (StateHandler){      title_init,      title_init_draw,      title_update,      title_draw,      title_cleanup };
    states[STATE_SELECT]    = (StateHandler){     select_init,     select_init_draw,     select_update,     select_draw,     select_cleanup };
    states[STATE_GAME]      = (StateHandler){       game_init,       game_init_draw,       game_update,       game_draw,       game_cleanup };
    states[STATE_SOUNDTEST] = (StateHandler){ sound_test_init, sound_test_init_draw, sound_test_update, sound_test_draw, sound_test_cleanup };
    states[STATE_GAMEOVER]  = (StateHandler){   gameover_init,   gameover_init_draw,   gameover_update,   gameover_draw,   gameover_cleanup };
    states[STATE_HIGHSCORE] = (StateHandler){  highscore_init,  highscore_init_draw,  highscore_update,  highscore_draw,  highscore_cleanup };
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
    VDP_loadFont(&TS_FONT_CLEAR, CPU);    // 2. Spiel-Logik Inits
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    VDP_setTextPalette(PAL3);
    initHighscores();
    initStateMachine();
    SYS_showFrameLoad(TRUE);
    // 3. Grafik-Hintergrund vorbereiten
    menu_bg_init(); 
    // Da wir mit STATE_TITLE starten, schalten wir ihn hier direkt aktiv
    menu_bg_set_active(true);
    menu_bg_set_mode(BG_MODE_MENU);
    // Kaltstart-Fix: Input synchronisieren
    joyState = JOY_readJoypad(JOY_1);
    lastJoyState = joyState; 
    SOUND_init(); 
    
    // 2. Musik starten
    SOUND_playMusic();

    while(1) {
        // Input lesen
        joyState = JOY_readJoypad(JOY_1);

        // --- STATE MANAGEMENT ---
        if (currentState != lastState) {
            // Alten State wegräumen
            if (lastState != STATE_NONE) {
                states[lastState].cleanup();
            }
            
            // Input-Buffer beim Wechsel leeren (verhindert Geister-Eingaben)
            lastJoyState = joyState; 
            
            // Neuen State starten
            states[currentState].init();
            states[currentState].init_draw();
            lastState = currentState;
        }

        // --- UPDATE LOGIK ---
        
        // Aktuellen Spiel-Zustand (Titel, Menü, Game) updaten
        states[currentState].update();
        states[currentState].draw();
        
        
        // Hintergrund-Animation (läuft autark, wenn menu_bg_set_active(true) ist)
        menu_bg_update();

        // Letzten Input für den nächsten Frame speichern
        lastJoyState = joyState;

        // Auf VBlank warten (60 FPS Taktung)
        SYS_doVBlankProcess();
    }

    // Für den Fall eines Resets oder Loop-Exits
    menu_bg_set_active(false);
    
    return 0;
}