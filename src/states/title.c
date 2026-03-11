#include <genesis.h>
#include "states/title.h"
#include "states/states.h"
#include "menu_bg.h"

// Lokaler Kontext für den Title-State
typedef struct {
    u16 blinkTimer;
    bool textVisible;
    bool lastTextVisible; // Hilfsvariable, um VDP-Überlastung zu vermeiden
    u16 idleTimer;
} TitleContext;

static TitleContext* ctx = NULL;

void title_init() {
    // 1. Logik & Speicher
    menu_bg_set_active(true);
    ctx = MEM_alloc(sizeof(TitleContext));
    
    ctx->blinkTimer = 0;
    ctx->textVisible = true;
    ctx->lastTextVisible = false; // Erzwingt das erste Zeichnen in title_draw
    ctx->idleTimer = 0;
}

void title_init_draw() {
    if (ctx == NULL) return;

    // Alles, was einmalig beim Start gemalt wird
    VDP_clearTextArea(0, 0, 40, 28);
    VDP_drawText("TETRIS CLONE", 14, 10);
}

void title_update() {
    if (ctx == NULL) return;

    // 1. Blink-Logik (Nur State-Änderung)
    ctx->blinkTimer++;
    if (ctx->blinkTimer >= 30) {
        ctx->blinkTimer = 0;
        ctx->textVisible = !ctx->textVisible;
    }

    // 2. Idle-Logik: Automatischer Wechsel
    if (joyState == 0) {
        ctx->idleTimer++;
        if (ctx->idleTimer >= 420) {
            currentState = STATE_HIGHSCORE;
        }
    } else {
        ctx->idleTimer = 0;
    }

    // 3. Navigation (Input-Logik)
    if ((joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
        currentState = STATE_SELECT; 
    }
    
    if ((joyState & BUTTON_C) && !(lastJoyState & BUTTON_C)) {
        currentState = STATE_SOUNDTEST;
    }

    if ((joyState & BUTTON_B) && !(lastJoyState & BUTTON_B)) {
        currentState = STATE_HIGHSCORE;
    }
}

void title_draw() {
    if (ctx == NULL) return;

    // Wir malen NUR, wenn sich der Zustand von textVisible geändert hat
    if (ctx->textVisible != ctx->lastTextVisible) {
        if (ctx->textVisible) {
            VDP_drawText("PRESS START TO PLAY", 10, 16);
            VDP_drawText("PRESS C FOR SOUND TEST", 9, 18);
        } else {
            // Löscht den Bereich, in dem der Text stand
            VDP_clearTextArea(9, 16, 22, 3);
        }
        // Zustand merken, damit wir im nächsten Frame nicht erneut malen
        ctx->lastTextVisible = ctx->textVisible;
    }
}

void title_cleanup() {
    // Visuelle Reinigung
    VDP_clearTextArea(0, 0, 40, 28);

    // Speicherfreigabe
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
}