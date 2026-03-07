#include <genesis.h>
#include "gameover.h"
#include "states.h"
#include "game.h" // Um auf den Score zuzugreifen

void gameover_init() {
    menu_bg_set_active(true);
    VDP_clearTextArea(0, 0, 40, 28);

    VDP_setTextPalette(PAL1); // Gelb
    VDP_drawText("--- GAME OVER ---", 11, 8);
    
    // Anzeige des Spielernamens und des Scores
    char scoreTxt[20];
    sprintf(scoreTxt, "SCORE: %ld", config.currentScore); // currentScore muss im GameContext/Global liegen

    VDP_setTextPalette(PAL0); // Weiß
    VDP_drawText("PLAYER:", 12, 12);
    VDP_setTextPalette(PAL2); // Rot
    VDP_drawText(config.playerName, 20, 12);

    VDP_setTextPalette(PAL0);
    VDP_drawText(scoreTxt, 12, 14);

    VDP_setTextPalette(PAL1);
    VDP_drawText("PRESS START FOR RANKING", 8, 22);
}

void gameover_update() {
    // Bei Druck auf START oder A: Highscore aktualisieren und State wechseln
    if ((joyState & (BUTTON_START | BUTTON_A)) && !(lastJoyState & (BUTTON_START | BUTTON_A))) {
        
        // Hier rufen wir die Sortierung auf
        check_and_update_highscore(config.currentScore);
        
        // Direkt zum Highscore-Screen wechseln
        currentState = STATE_HIGHSCORE;
    }
}

void gameover_cleanup() {
    VDP_clearTextArea(0, 0, 40, 28);
}