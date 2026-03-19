#include <genesis.h>
#include <string.h>

#include "states/gameover.h"
#include "states/states.h"
#include "states/game.h"
#include "fonts.h"
#include "states/highscore.h"
#include "menu_bg.h"
#include "save_manager.h"

void gameover_init() {
    menu_bg_set_mode(BG_MODE_MENU);
}

void gameover_init_draw() {
    VDP_clearTextArea(0, 0, 40, 28);

    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);

    PAL_setPalette(PAL2, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(37, RGB24_TO_VDPCOLOR(0x660000));
    PAL_setColor(38, RGB24_TO_VDPCOLOR(0xFF0000));
    PAL_setColor(39, RGB24_TO_VDPCOLOR(0x660000));

    PAL_setPalette(PAL1, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(21, RGB24_TO_VDPCOLOR(0x666600));
    PAL_setColor(22, RGB24_TO_VDPCOLOR(0xFFFF00));
    PAL_setColor(23, RGB24_TO_VDPCOLOR(0x666600));

    VDP_setTextPalette(PAL2);
    VDP_drawText("--- GAME OVER ---", 11, 8);

    VDP_setTextPalette(PAL3);
    VDP_drawText("PLAYER:", 12, 12);
    VDP_drawText("SCORE:", 12, 14);

    VDP_setTextPalette(PAL1);
    VDP_drawText(config.playerName, 20, 12);

    char scoreTxt[12];
    sprintf(scoreTxt, "%ld", config.currentScore);
    VDP_drawText(scoreTxt, 20, 14);

    VDP_setTextPalette(PAL1);
    VDP_drawText("PRESS START FOR RANKING", 8, 22);
}

void gameover_update() {
    if ((joyState & (BUTTON_START | BUTTON_A)) && !(lastJoyState & (BUTTON_START | BUTTON_A))) {
        // 1. Highscore-Logik (schreibt in config.highscores)
        check_and_update_highscore(config.currentScore);
        
        // 2. SRAM-Sync: Den gesamten Context (inkl. neuer Highscores) sichern
        save_highscores();
        
        currentState = STATE_HIGHSCORE;
    }
}

void gameover_draw() {
}

void gameover_cleanup() {
    VDP_clearTextArea(0, 0, 40, 28);
}