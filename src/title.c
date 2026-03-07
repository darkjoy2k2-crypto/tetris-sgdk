#include <genesis.h>
#include "title.h"
#include "states.h"

// Lokaler Kontext für den Title-State
typedef struct {
    u16 blinkTimer;
    bool textVisible;
} TitleContext;

static TitleContext* ctx = NULL;

void title_init() {
    // Speicher für diesen State reservieren
    ctx = MEM_alloc(sizeof(TitleContext));
    ctx->blinkTimer = 0;
    ctx->textVisible = true;

    // VDP (Video Display Processor) vorbereiten
    VDP_clearTextArea(0, 0, 40, 28);
    
    // Statischer Text
    VDP_drawText("TETRIS CLONE", 14, 10);
}

void title_update() {
    // Sicherheitscheck: Falls der Kontext nicht existiert, abbrechen
    if (ctx == NULL) return;

    // --- BLINK-LOGIK ---
    // Der Timer zählt die Frames (60 FPS). Alle 30 Frames wechselt die Sichtbarkeit.
    ctx->blinkTimer++;
    if (ctx->blinkTimer >= 30) {
        ctx->blinkTimer = 0;
        ctx->textVisible = !ctx->textVisible;
        
        if (ctx->textVisible) {
            // Text wieder anzeigen
            VDP_drawText("PRESS START TO PLAY", 10, 16);
            VDP_drawText("PRESS C FOR SOUND TEST", 9, 18);
        } else {
            // Textbereich löschen (X: 9, Y: 16, Breite: 22, Höhe: 3)
            VDP_clearTextArea(9, 16, 22, 3);
        }
    }

    // --- INPUT-LOGIK (FLANKENERKENNUNG) ---
    // Wir prüfen: Knopf ist JETZT aktiv (joyState) UND war im LETZTEN Frame INAKTIV (!lastJoyState)

    // START-Knopf: Wechselt zum Auswahlmenü
    if ((joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
        currentState = STATE_SELECT; 
    }

    // C-Knopf: Wechselt zum Soundtest
    if ((joyState & BUTTON_C) && !(lastJoyState & BUTTON_C)) {
        currentState = STATE_SOUNDTEST;
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