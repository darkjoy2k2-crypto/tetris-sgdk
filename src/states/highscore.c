#include <genesis.h>
#include <string.h>

#include "states/highscore.h"
#include "states/states.h"
#include "menu_bg.h"
#include "fonts.h"
#include "gfx.h"

static HighscoreContext* ctx = NULL;

void highscore_init() {
    ctx = &sctx->highscore;
    ctx->displayTimer = 0;
    
    menu_bg_set_mode(BG_MODE_MENU);
    menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));
}

void highscore_init_draw() {
    if (ctx == NULL) return;
    
    VDP_clearTextArea(0, 0, 40, 28);

    VDP_setTextPalette(PAL2);
    VDP_drawText("--- TOP 10 RANKING ---", 9, 4);
    VDP_drawText("RANK   NAME  SCORE", 11, 8);
    
    char txtRank[4];
    char txtScore[12];
    for (u16 i = 0; i < 10; i++) {
        sprintf(txtRank, "%2d.", i + 1);
        sprintf(txtScore, "%6ld", config.highscores[i].score);
        u16 y = 10 + i;

        if (config.highscores[i].isNew != 0) {
            VDP_setTextPalette(PAL1);
        } else {
            VDP_setTextPalette(PAL3);
        }

        VDP_drawText(txtRank, 11, y);
        VDP_drawText(config.highscores[i].name, 17, y);
        VDP_drawText(txtScore, 23, y);
    }
}

void highscore_update() {
    if (ctx == NULL) return;

    bool input = (joyState & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C)) && 
                 !(lastJoyState & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C));

    if (input || ctx->displayTimer >= 420) {
        // Prüfen ob gespeichert werden muss
        bool needsSave = FALSE;
        for (u16 i = 0; i < 10; i++) {
            if (config.highscores[i].isNew != 0) {
                needsSave = TRUE;
                break;
            }
        }

        // RAM bereinigen bevor wir gehen
        config.currentScore = 0;

        if (needsSave) {
            currentState = STATE_SAVE;
        } else {
            currentState = STATE_TITLE;
        }
    }

    ctx->displayTimer++;
}

void highscore_draw() {
}

void highscore_cleanup() {
    // Alle isNew Markierungen löschen
    for (u16 i = 0; i < 10; i++) {
        config.highscores[i].isNew = 0;
    }

    VDP_clearTextArea(0, 0, 40, 28);
    ctx = NULL;
}