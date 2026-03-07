#include <genesis.h>
#include "game_select.h"
#include "states.h"
#include "sound_manager.h"

typedef struct {
    u16 cursor;         // Aktuelle Zeile
    bool fairRandom;    // 7-Bag vs Chaos
    bool speedUp;       // Gravitation an/aus
    bool showShadow;    // Ghost-Piece
    bool allowHold;     // C-Taste
    bool showNext;      // Vorschau-Fenster
} SelectContext;

static SelectContext* ctx = NULL;

// Hilfsfunktion zum Zeichnen der Optionen
static void draw_option(u16 row, char* label, bool value, bool isSelected, char* trueText, char* falseText) {
    if (isSelected) VDP_drawText(">", 8, 10 + (row * 2));
    else VDP_drawText(" ", 8, 10 + (row * 2));

    VDP_drawText(label, 10, 10 + (row * 2));
    VDP_drawText(value ? trueText : falseText, 26, 10 + (row * 2));
}

void select_init() {
    ctx = MEM_alloc(sizeof(SelectContext));
    ctx->cursor = 0;
    ctx->fairRandom = true;
    ctx->speedUp = true;
    ctx->showShadow = true;
    ctx->allowHold = true;
    ctx->showNext = true;

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_drawText("--- GAME SETTINGS ---", 10, 6);
    VDP_drawText("START TO BEGIN", 13, 24);
}

void select_update() {
    if (ctx == NULL) return;

    u16 joy = JOY_readJoypad(JOY_1);
    static u16 lastJoy = 0;
    u16 changed = joy & ~lastJoy;
    lastJoy = joy;

    // Navigation Up/Down
    if (changed & BUTTON_DOWN) {
        ctx->cursor = (ctx->cursor + 1) % 5;
        SOUND_play(SND_MOVE);
    }
    if (changed & BUTTON_UP) {
        ctx->cursor = (ctx->cursor == 0) ? 4 : ctx->cursor - 1;
        SOUND_play(SND_MOVE);
    }

    // Werte ändern mit Links/Rechts oder A
    if (changed & (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_A)) {
        SOUND_play(SND_ROTATE);
        switch(ctx->cursor) {
            case 0: ctx->fairRandom = !ctx->fairRandom; break;
            case 1: ctx->speedUp    = !ctx->speedUp;    break;
            case 2: ctx->showShadow = !ctx->showShadow; break;
            case 3: ctx->allowHold  = !ctx->allowHold;  break;
            case 4: ctx->showNext   = !ctx->showNext;   break;
        }
    }

    // Anzeige zeichnen
    draw_option(0, "Zufall",     ctx->fairRandom, (ctx->cursor == 0), "Fair ", "Chaos");
    draw_option(1, "Speed Up",   ctx->speedUp,    (ctx->cursor == 1), "An   ", "Aus ");
    draw_option(2, "Schatten",   ctx->showShadow, (ctx->cursor == 2), "An   ", "Aus ");
    draw_option(3, "Hold",       ctx->allowHold,  (ctx->cursor == 3), "An   ", "Aus ");
    draw_option(4, "Next Tile",  ctx->showNext,   (ctx->cursor == 4), "An   ", "Aus ");

    // Spiel starten
    if (changed & BUTTON_START) {
        // Hier würden wir die Werte in den globalen Spiel-Kontext übertragen
        currentState = STATE_GAME;
    }
}

void select_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearTextArea(0, 0, 40, 28);
}