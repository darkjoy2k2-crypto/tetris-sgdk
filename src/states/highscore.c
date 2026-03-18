#include <genesis.h>
#include "states/highscore.h"
#include "states/states.h"
#include "menu_bg.h"
#include "fonts.h"
#include "gfx.h"

static HighscoreContext* ctx = NULL;

void highscore_init() {
    // Brücke schlagen
    ctx = &sctx->highscore;
    
    // Initialisierung (Speicher wurde in main.c genullt)
    ctx->displayTimer = 0;
    
    // Hintergrund-Handling
    menu_bg_set_mode(BG_MODE_MENU);
    menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));
}

void highscore_init_draw() {
    if (ctx == NULL) return;
    UI_init_fonts_and_palettes();
    
    VDP_clearTextArea(0, 0, 40, 28);

    VDP_setTextPalette(PAL2);
    VDP_drawText("--- TOP 10 RANKING ---", 9, 4);
    VDP_drawText("RANK   NAME  SCORE", 11, 8);
    
    char txtRank[4];
    char txtScore[12];
    for (u16 i = 0; i < 10; i++) {
        sprintf(txtRank, "%2d.", i + 1);
        sprintf(txtScore, "%6ld", highscores[i].score);
        u16 y = 10 + i;

        if (highscores[i].isNew) {
            VDP_setTextPalette(PAL1);
        } else {
            VDP_setTextPalette(PAL3);
        }

        VDP_drawText(txtRank, 11, y);
        VDP_drawText(highscores[i].name, 17, y);
        VDP_drawText(txtScore, 23, y);
    }
}

void highscore_update() {
    if (ctx == NULL) return;

    if (joyState & ~lastJoyState) {
        currentState = STATE_TITLE;
        return;
    }

    ctx->displayTimer++;
    if (ctx->displayTimer >= 420) {
        currentState = STATE_TITLE;
    }
}

void highscore_draw() {
    // Statische Anzeige, keine Logik erforderlich
}

void highscore_cleanup() {
    // isNew zurücksetzen, bevor der State gewechselt wird
    for (u16 i = 0; i < 10; i++) {
        highscores[i].isNew = false;
    }

    VDP_clearTextArea(0, 0, 40, 28);
    ctx = NULL;
}