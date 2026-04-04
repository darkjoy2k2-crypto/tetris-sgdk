#include <genesis.h>
#include <string.h>

#include "states/highscore.h"
#include "states/states.h"
#include "menu_bg.h"
#include "fonts.h"
#include "gfx.h"
#include "text_manager.h"

static HighscoreContext* highscoreCtx = NULL;

static void highscore_finish_state(void) {
    bool needsSave = FALSE;

    for (u16 i = 0; i < 10; i++) {
        if (config.highscores[i].isNew != 0) {
            needsSave = TRUE;
            break;
        }
    }

    config.currentScore = 0;

    if (needsSave) {
        config.sramop = SRAM_SAVE;
        config.preferredState = STATE_TITLE;
        currentState = STATE_SAVE;
    } else {
        config.sramop = SRAM_NONE;
        config.preferredState = STATE_NONE;
        currentState = STATE_TITLE;
    }
}

void highscore_init() {
    highscoreCtx = &sctx->highscore;
    highscoreCtx->displayTimer = 0;
    highscoreCtx->needsRefresh = TRUE;

    menu_bg_set_mode(BG_MODE_MENU);
}

void highscore_init_draw() {
    if (highscoreCtx == NULL) return;

    UI_init_fonts_and_palettes();
    VDP_clearTextArea(0, 0, 40, 28);

    VDP_setTextPalette(PAL1);
    VDP_drawText("--- TOP 10 RANKING ---", 9, 3);
    VDP_setTextPalette(PAL3);
    VDP_drawText("RANK   NAME  SCORE", 11, 7);

    {
        char txtRank[4];
        char txtScore[12];

        for (u16 i = 0; i < 10; i++) {
            sprintf(txtRank, "%2d.", i + 1);
            sprintf(txtScore, "%6ld", config.highscores[i].score);

            if (config.highscores[i].isNew != 0) VDP_setTextPalette(PAL1);
            else VDP_setTextPalette(PAL3);

            VDP_drawText(txtRank, 11, (u16)(9 + i));
            VDP_drawText(config.highscores[i].name, 17, (u16)(9 + i));
            VDP_drawText(txtScore, 23, (u16)(9 + i));
        }
    }

    text_manager_init_highscore();
    text_manager_glyphs_visible(TRUE);
}

void highscore_update() {
    bool input;

    if (highscoreCtx == NULL) return;

    input = (joyState & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C)) &&
            !(lastJoyState & (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_C));

    if (input || highscoreCtx->displayTimer >= 420) {
        text_manager_request_exit();
    }

    text_manager_set_enabled(TRUE);
    text_manager_update();

    if (text_manager_is_finished()) {
        highscore_finish_state();
        return;
    }

    highscoreCtx->displayTimer++;
}

void highscore_draw() {
}

void highscore_cleanup() {
    for (u16 i = 0; i < 10; i++) {
        config.highscores[i].isNew = 0;
    }

    text_manager_cleanup();
    VDP_clearTextArea(0, 0, 40, 28);
    highscoreCtx = NULL;
}