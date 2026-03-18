#include <genesis.h>
#include "states/sound_test.h"
#include "sound_manager.h"
#include "states/states.h"
#include "menu_bg.h"

static SoundTestContext* ctx = NULL;

void sound_test_init() {
    // Brücke zur globalen Union schlagen
    ctx = &sctx->soundtest;

    // Hintergrund-Modul konfigurieren
    menu_bg_set_mode(BG_MODE_MENU);
    menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));

    // Initialisierung der Werte (Speicher wurde in main.c genullt)
    ctx->currentID = 1;
    ctx->needsDraw = true;
}

void sound_test_init_draw() {
    if (ctx == NULL) return;

    // Statische UI-Elemente
    VDP_clearTextArea(0, 0, 40, 28);
    VDP_drawText("--- SOUND TEST MODE ---", 9, 4);
    VDP_drawText("LEFT/RIGHT: SELECT", 11, 10);
    VDP_drawText("A: PLAY | START: BACK", 9, 22);
}

void sound_test_update() {
    if (ctx == NULL) return;

    // Lokale Flankenerkennung (nutzt joyState/lastJoyState vom System)
    u16 changed = joyState & ~lastJoyState;

    // Navigation (1-99)
    if (changed & BUTTON_RIGHT) {
        ctx->currentID = (ctx->currentID >= 99) ? 1 : ctx->currentID + 1;
        ctx->needsDraw = true;
    }
    if (changed & BUTTON_LEFT) {
        ctx->currentID = (ctx->currentID <= 1) ? 99 : ctx->currentID - 1;
        ctx->needsDraw = true;
    }

    // Sound abspielen
    if (changed & BUTTON_A) {
        SOUND_play((SoundEvent)ctx->currentID);
    }

    // Zurück zum Titel
    if (changed & BUTTON_START) {
        currentState = STATE_TITLE;
    }
}

void sound_test_draw() {
    if (ctx == NULL || !ctx->needsDraw) return;

    char txt[16];
    // %03d sorgt für die führenden Nullen (001, 002...)
    sprintf(txt, "SOUND ID: %03d", ctx->currentID);
    VDP_drawText(txt, 13, 12);

    // Flag zurücksetzen, damit wir nicht jeden Frame zeichnen
    ctx->needsDraw = false;
}

void sound_test_cleanup() {
    // Nur Pointer lösen, kein MEM_free
    VDP_clearTextArea(0, 0, 40, 28);
    ctx = NULL;
}