#include <genesis.h>
#include "highscore.h"
#include "states.h"

// In src/highscore.c

typedef struct HighscoreContext{
    u16 displayTimer; // NEU: Timer für die Anzeigedauer
} HighscoreContext;

static HighscoreContext* hCtx = NULL;

void highscore_init() {
    hCtx = MEM_alloc(sizeof(HighscoreContext));
    hCtx->displayTimer = 0;

    VDP_clearTextArea(0, 0, 40, 28);
    
    VDP_setTextPalette(PAL1); // Gelb
    VDP_drawText("--- TOP 10 RANKING ---", 9, 4);
    VDP_drawText("RANK  NAME  SCORE", 8, 7);
    
    VDP_setTextPalette(PAL0); // Weiß
    for (u16 i = 0; i < 10; i++) {
        char txtRank[4];
        char txtScore[12];
        sprintf(txtRank, "%2d.", i + 1);
        sprintf(txtScore, "%6ld", highscores[i].score);
        
        u16 y = 9 + i;
        VDP_drawText(txtRank, 8, y);
        VDP_drawText(highscores[i].name, 14, y);
        VDP_drawText(txtScore, 20, y);
    }
}

void highscore_update() {
    if (hCtx == NULL) return;

    // 1. Sofortiger Rücksprung bei IRGENDEINER Taste (Flanke)
    // Wir prüfen, ob im aktuellen Frame eine Taste gedrückt wurde, die vorher nicht da war
    if (joyState & ~lastJoyState) {
        currentState = STATE_TITLE;
        return;
    }

    // 2. Automatischer Rücksprung nach Zeit
    hCtx->displayTimer++;
    // Nach ca. 7 Sekunden zurück zum Titelbild
    if (hCtx->displayTimer >= 420) {
        currentState = STATE_TITLE;
    }
}

void highscore_cleanup() {
    if (hCtx != NULL) {
        MEM_free(hCtx);
        hCtx = NULL;
    }
    VDP_clearTextArea(0, 0, 40, 28);
}