#include <genesis.h>
#include "states/highscore.h"
#include "states/states.h"
#include "fonts.h"

typedef struct HighscoreContext {
    u16 displayTimer;
} HighscoreContext;

static HighscoreContext* hCtx = NULL;

void highscore_init() {
    // Speicher-Management (Sicherheitshalber aufräumen falls nötig)
    if (hCtx != NULL) {
        MEM_free(hCtx);
    }
    
    hCtx = MEM_alloc(sizeof(HighscoreContext));
    hCtx->displayTimer = 0;
    
    // Logik-Reset: Die isNew-Flags werden erst im Cleanup zurückgesetzt, 
    // damit die init_draw sie noch zum Markieren nutzen kann.
}

void highscore_init_draw() {
    if (hCtx == NULL) return;

    // 1. Screen vorbereiten
    VDP_clearTextArea(0, 0, 40, 28);
    
    // 2. Paletten-Setup (Farben definieren)
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);

    // Palette für das UI (Rot-Töne)
    PAL_setPalette(PAL2, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0x440000)); 
    PAL_setColor(37, RGB24_TO_VDPCOLOR(0x880000)); 
    PAL_setColor(38, RGB24_TO_VDPCOLOR(0xFF0000)); 
    PAL_setColor(39, RGB24_TO_VDPCOLOR(0xFF8888));

    // Palette für neue Einträge (Gelb/Gold)
    PAL_setPalette(PAL1, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x444400));
    PAL_setColor(21, RGB24_TO_VDPCOLOR(0x888800));
    PAL_setColor(22, RGB24_TO_VDPCOLOR(0xFFFF00));
    PAL_setColor(23, RGB24_TO_VDPCOLOR(0x888800));

    // 3. Header zeichnen
    VDP_setTextPalette(PAL2);
    VDP_drawText("--- TOP 10 RANKING ---", 9, 4);
    VDP_drawText("RANK   NAME  SCORE", 11, 8);
    
    // 4. Liste rendern
    char txtRank[4];
    char txtScore[12];
    for (u16 i = 0; i < 10; i++) {
        sprintf(txtRank, "%2d.", i + 1);
        sprintf(txtScore, "%6ld", highscores[i].score);
        u16 y = 10 + i;

        // Markierung für neue Rekorde via Palette
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
    if (hCtx == NULL) return;

    // Flankenerkennung: Bei irgendeinem Tastendruck zurück zum Titel
    if (joyState & ~lastJoyState) {
        currentState = STATE_TITLE;
        return;
    }

    // Automatischer Timeout
    hCtx->displayTimer++;
    if (hCtx->displayTimer >= 420) {
        currentState = STATE_TITLE;
    }
}

void highscore_draw() {
    // Hier ist aktuell nichts zu tun, da die Highscore-Liste statisch ist.
    // Falls du später animierte "New Record"-Blinker willst, kämen sie hier rein.
}

void highscore_cleanup() {
    // Speicher freigeben
    if (hCtx != NULL) {
        MEM_free(hCtx);
        hCtx = NULL;
    }

    // Flags zurücksetzen, nachdem alles gezeichnet wurde
    for (u16 i = 0; i < 10; i++) {
        highscores[i].isNew = false;
    }

    VDP_clearTextArea(0, 0, 40, 28);
}