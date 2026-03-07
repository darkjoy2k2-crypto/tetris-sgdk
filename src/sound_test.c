#include <genesis.h>
#include "sound_test.h"
#include "sound_manager.h"
#include "states.h"

typedef struct {
    u16 currentID;
} SoundTestCtx;

static SoundTestCtx* ctx = NULL;

// 1. Die Hilfsfunktion nach oben schieben
static void sound_test_draw_number() {
    if (ctx == NULL) return;
    char txt[16];
    // %03d sorgt für die führenden Nullen (001, 002...)
    sprintf(txt, "SOUND ID: %03d", ctx->currentID);
    VDP_drawText(txt, 13, 12);
}

// 2. Jetzt kennt init die Funktion draw_number bereits
void sound_test_init() {
    ctx = MEM_alloc(sizeof(SoundTestCtx));
    ctx->currentID = 1;

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_drawText("--- SOUND TEST MODE ---", 9, 4);
    VDP_drawText("LEFT/RIGHT: SELECT", 11, 10);
    VDP_drawText("A: PLAY | START: BACK", 9, 22);

    sound_test_draw_number();
}

void sound_test_update() {
    if (ctx == NULL) return;

    u16 joy = JOY_readJoypad(JOY_1);
    static u16 lastJoy = 0;
    u16 changed = joy & ~lastJoy;
    lastJoy = joy;

    bool needsRefresh = false;

    // Navigation (1-99)
    if (changed & BUTTON_RIGHT) {
        ctx->currentID = (ctx->currentID >= 99) ? 1 : ctx->currentID + 1;
        needsRefresh = true;
    }
    if (changed & BUTTON_LEFT) {
        ctx->currentID = (ctx->currentID <= 1) ? 99 : ctx->currentID - 1;
        needsRefresh = true;
    }

    if (needsRefresh) {
        sound_test_draw_number();
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

void sound_test_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearTextArea(0, 0, 40, 28);
}