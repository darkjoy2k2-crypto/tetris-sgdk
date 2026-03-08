#include <genesis.h>
#include "highscore.h"
#include "states.h"
#include "fonts.h"

typedef struct HighscoreContext {
    u16 displayTimer;
} HighscoreContext;

static HighscoreContext* hCtx = NULL;

void highscore_init() {
    if (hCtx != NULL) {
        MEM_free(hCtx);
        hCtx = NULL;
    }
    
    hCtx = MEM_alloc(sizeof(HighscoreContext));
    hCtx->displayTimer = 0;

    VDP_clearTextArea(0, 0, 40, 28);
    
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);

    PAL_setPalette(PAL2, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0x440000)); 
    PAL_setColor(37, RGB24_TO_VDPCOLOR(0x880000)); 
    PAL_setColor(38, RGB24_TO_VDPCOLOR(0xFF0000)); 
    PAL_setColor(39, RGB24_TO_VDPCOLOR(0xFF8888));

    PAL_setPalette(PAL1, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x444400));
    PAL_setColor(21, RGB24_TO_VDPCOLOR(0x888800));
    PAL_setColor(22, RGB24_TO_VDPCOLOR(0xFFFF00));
    PAL_setColor(23, RGB24_TO_VDPCOLOR(0x888800));

    VDP_setTextPalette(PAL2);
    VDP_drawText("--- TOP 10 RANKING ---", 9, 4);
    VDP_drawText("RANK   NAME  SCORE", 11, 8);
    
    char txtRank[4];
    char txtScore[12];
    for (u16 i = 0; i < 10; i++) {
        sprintf(txtRank, "%2d.", i + 1);
        sprintf(txtScore, "%6ld", highscores[i].score);
        u16 y = 10 + i;

        if (highscores[i].isNew) VDP_setTextPalette(PAL1);
        else VDP_setTextPalette(PAL3);

        VDP_drawText(txtRank, 11, y);
        VDP_drawText(highscores[i].name, 17, y);
        VDP_drawText(txtScore, 23, y);
    }
}

void highscore_update() {
    if (hCtx == NULL) return;

    if (joyState & ~lastJoyState) {
        currentState = STATE_TITLE;
        return;
    }

    hCtx->displayTimer++;
    if (hCtx->displayTimer >= 420) {
        currentState = STATE_TITLE;
    }
}

void highscore_cleanup() {
    if (hCtx != NULL) {
        MEM_free(hCtx);
        hCtx = NULL;
    }
    for (u16 i = 0; i < 10; i++) {
        highscores[i].isNew = false;
    }
    VDP_clearTextArea(0, 0, 40, 28);
}