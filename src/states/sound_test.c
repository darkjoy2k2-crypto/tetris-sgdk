#include <genesis.h>
#include "states/sound_test.h"
#include "sound_manager.h"
#include "states/states.h"
#include "menu_bg.h"
#include "gfx.h"

static SoundTestContext* ctx = NULL;

static void sound_test_step_value(u16* value, s16 dir, u16 minValue, u16 maxValue) {
    if (maxValue < minValue) {
        *value = minValue;
        return;
    }

    if (dir > 0) {
        *value = (*value >= maxValue) ? minValue : (*value + 1);
    } else if (dir < 0) {
        *value = (*value <= minValue) ? maxValue : (*value - 1);
    }
}

void sound_test_init() {
    ctx = &sctx->soundtest;

    menu_bg_set_mode(BG_MODE_MENU);
    SOUND_stopMusic();

    ctx->currentID = 1;
    ctx->currentMusicID = 1;
    ctx->mode = SOUND_TEST_MODE_SFX;
    ctx->needsDraw = TRUE;
}

void sound_test_init_draw() {
    if (ctx == NULL) return;

    UI_init_fonts_and_palettes();

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_drawText("--- SOUND TEST MODE ---", 9, 4);
    VDP_drawText("LEFT/RIGHT: SELECT ID", 9, 9);
    VDP_drawText("UP/DOWN: SFX OR MUSIC", 8, 10);
    VDP_drawText("A: PLAY | B/C: STOP MUSIC", 6, 22);
    VDP_drawText("START: BACK", 13, 24);
}

void sound_test_update() {
    u16 changed;

    if (ctx == NULL) return;

    changed = joyState & ~lastJoyState;

    if (changed & BUTTON_UP) {
        ctx->mode = SOUND_TEST_MODE_SFX;
        ctx->needsDraw = TRUE;
        SOUND_play(SND_MOVE);
    }
    if (changed & BUTTON_DOWN) {
        ctx->mode = SOUND_TEST_MODE_MUSIC;
        ctx->needsDraw = TRUE;
        SOUND_play(SND_MOVE);
    }

    if (changed & BUTTON_RIGHT) {
        if (ctx->mode == SOUND_TEST_MODE_SFX) {
            sound_test_step_value(&ctx->currentID, 1, 1, 99);
        } else {
            sound_test_step_value(&ctx->currentMusicID, 1, 1, SOUND_getMusicCount());
            SOUND_stopMusic();
        }
        ctx->needsDraw = TRUE;
        SOUND_play(SND_MOVE);
    }
    if (changed & BUTTON_LEFT) {
        if (ctx->mode == SOUND_TEST_MODE_SFX) {
            sound_test_step_value(&ctx->currentID, -1, 1, 99);
        } else {
            sound_test_step_value(&ctx->currentMusicID, -1, 1, SOUND_getMusicCount());
            SOUND_stopMusic();
        }
        ctx->needsDraw = TRUE;
        SOUND_play(SND_MOVE);
    }

    if (changed & BUTTON_A) {
        if (ctx->mode == SOUND_TEST_MODE_SFX) {
            SOUND_play((SoundEvent)ctx->currentID);
        } else {
            SOUND_playMusicById(ctx->currentMusicID);
        }
    }

    if (changed & (BUTTON_B | BUTTON_C)) {
        SOUND_stopMusic();
    }

    if (changed & BUTTON_START) {
        SOUND_stopMusic();
        currentState = STATE_TITLE;
    }
}

void sound_test_draw() {
    char txt[32];

    if (ctx == NULL || !ctx->needsDraw) return;

    VDP_clearTextArea(7, 12, 26, 6);
    VDP_drawText((ctx->mode == SOUND_TEST_MODE_SFX) ? ">" : " ", 7, 12);
    VDP_drawText((ctx->mode == SOUND_TEST_MODE_MUSIC) ? ">" : " ", 7, 14);

    sprintf(txt, "SFX ID: %03d", ctx->currentID);
    VDP_drawText(txt, 9, 12);

    sprintf(txt, "MUSIC ID: %02d/%02d", ctx->currentMusicID, SOUND_getMusicCount());
    VDP_drawText(txt, 9, 14);

    sprintf(txt, "TRACK: %-10s", SOUND_getMusicName(ctx->currentMusicID));
    VDP_drawText(txt, 9, 16);

    ctx->needsDraw = FALSE;
}

void sound_test_cleanup() {
    SOUND_stopMusic();
    VDP_clearTextArea(0, 0, 40, 28);
    ctx = NULL;
}