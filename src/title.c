#include <genesis.h>
#include "title.h"
#include "states.h"

// Lokaler Kontext für den Title-State
typedef struct TitleContext{
    u16 blinkTimer;
    bool textVisible;
    u16 idleTimer; // NEU: Zähler für Untätigkeit
} TitleContext;

static TitleContext* ctx = NULL;

void title_init() {
    menu_bg_set_active(true);
    ctx = MEM_alloc(sizeof(TitleContext));
    ctx->blinkTimer = 0;
    ctx->textVisible = true;
    ctx->idleTimer = 0; // Initialisieren

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_drawText("TETRIS CLONE", 14, 10);
}

void title_update() {
    if (ctx == NULL) return;

    // 1. Blink-Logik für Text
    ctx->blinkTimer++;
    if (ctx->blinkTimer >= 30) {
        ctx->blinkTimer = 0;
        ctx->textVisible = !ctx->textVisible;
        if (ctx->textVisible) {
            VDP_drawText("PRESS START TO PLAY", 10, 16);
            VDP_drawText("PRESS C FOR SOUND TEST", 9, 18);
        } else {
            VDP_clearTextArea(9, 16, 22, 3);
        }
    }

    // 2. Idle-Logik: Automatischer Wechsel zum Highscore
    // Wenn keine Taste gedrückt wird, zählt der Timer hoch
    if (joyState == 0) {
        ctx->idleTimer++;
        // Nach ca. 7 Sekunden (420 Frames bei 60Hz) wechseln
        if (ctx->idleTimer >= 420) {
            currentState = STATE_HIGHSCORE;
        }
    } else {
        // Bei jeglicher Aktivität Timer zurücksetzen
        ctx->idleTimer = 0;
    }

    // 3. Manuelle Navigation (Flankenerkennung)
    if ((joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
        currentState = STATE_SELECT; 
    }
    
    if ((joyState & BUTTON_C) && !(lastJoyState & BUTTON_C)) {
        currentState = STATE_SOUNDTEST;
    }

    // Manuell zur Highscore-Liste via B
    if ((joyState & BUTTON_B) && !(lastJoyState & BUTTON_B)) {
        currentState = STATE_HIGHSCORE;
    }
}

void title_cleanup() {
    // Textbereiche säubern
    VDP_clearTextArea(0, 0, 40, 28);

    // Speicher explizit freigeben
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
}