#include <genesis.h>
#include "states/gameover.h"
#include "states/states.h"
#include "states/game.h"
#include "fonts.h"
#include "states/highscore.h"
#include "menu_bg.h"

void gameover_init() {
    // 1. Logik-Setup
    menu_bg_set_mode(BG_MODE_MENU);
    
    // Da wir keine dynamischen Variablen im GameOver-Kontext brauchen, 
    // verzichten wir hier auf ein MEM_alloc.
}

void gameover_init_draw() {
    // 2. Visuelles Setup (VDP & Paletten)
    VDP_clearTextArea(0, 0, 40, 28);

    // Paletten-Definitionen
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);

    // Palette für "GAME OVER" (Rot)
    PAL_setPalette(PAL2, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(37, RGB24_TO_VDPCOLOR(0x660000));
    PAL_setColor(38, RGB24_TO_VDPCOLOR(0xFF0000));
    PAL_setColor(39, RGB24_TO_VDPCOLOR(0x660000));

    // Palette für Highlights/Spielername (Gelb)
    PAL_setPalette(PAL1, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(21, RGB24_TO_VDPCOLOR(0x666600));
    PAL_setColor(22, RGB24_TO_VDPCOLOR(0xFFFF00));
    PAL_setColor(23, RGB24_TO_VDPCOLOR(0x666600));

    // 3. Texte zeichnen
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
    // 4. Input-Logik & State-Wechsel
    if ((joyState & (BUTTON_START | BUTTON_A)) && !(lastJoyState & (BUTTON_START | BUTTON_A))) {
        // Highscore-Logik triggern, bevor wir den State verlassen
        check_and_update_highscore(config.currentScore);
        currentState = STATE_HIGHSCORE;
    }
}

void gameover_draw() {
    // Aktuell statischer Screen, daher keine Frame-Updates nötig.
}

void gameover_cleanup() {
    // Visuelle Reinigung für den nächsten State
    VDP_clearTextArea(0, 0, 40, 28);
}