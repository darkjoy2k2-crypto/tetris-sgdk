#include <genesis.h>
#include "states/states.h"
#include "states/gfxtest.h"
#include "gfx.h"
#include "menu_bg.h"

// PIECES ist in game_logic.c definiert
extern const s8 PIECES[7][4][4][2];

static GfxTestContext* ctx = NULL;
static u16 gfx_tile_start;

#define GFXTEST_COLS      8
#define GFXTEST_ROWS      5
#define GFXTEST_SLOT_SIZE 4
#define GFXTEST_GAP       1
#define GFXTEST_START_X   0
#define GFXTEST_START_Y   2

static void gfxtest_refill_bag(void) {
    for (u8 i = 0; i < 7; i++) ctx->bag[i] = i;
    for (u8 i = 6; i > 0; i--) {
        u8 j = random() % (i + 1);
        u8 temp = ctx->bag[i];
        ctx->bag[i] = ctx->bag[j];
        ctx->bag[j] = temp;
    }
    ctx->bagIndex = 0;
}

static u8 gfxtest_next_piece(void) {
    if (ctx->bagIndex >= 7) gfxtest_refill_bag();
    return ctx->bag[ctx->bagIndex++];
}

static void draw_piece_slot(u16 type, u16 rot, u16 px, u16 py) {
    for (u16 i = 0; i < 4; i++) {
        s16 bx = PIECES[type][rot][i][0];
        s16 by = PIECES[type][rot][i][1];
        VDP_setTileMapXY(BG_A,
            TILE_ATTR_FULL(PAL2, 0, 0, 0, gfx_tile_start + 1 + type),
            (u16)(px + bx), (u16)(py + by));
    }
}

static void gfxtest_reroll_layout(void) {
    VDP_clearPlane(BG_A, TRUE);

    gfxtest_refill_bag();

    for (u16 row = 0; row < GFXTEST_ROWS; row++) {
        for (u16 col = 0; col < GFXTEST_COLS; col++) {
            u16 type = gfxtest_next_piece();
            u16 rot = random() & 3;
            u16 px = GFXTEST_START_X + col * (GFXTEST_SLOT_SIZE + GFXTEST_GAP);
            u16 py = GFXTEST_START_Y + row * (GFXTEST_SLOT_SIZE + GFXTEST_GAP);
            draw_piece_slot(type, rot, px, py);
        }
    }
}

void gfxtest_init() {
    ctx = &sctx->gfxtest;

    menu_bg_set_mode(BG_MODE_MENU);
    gfx_tile_start = TILE_USER_INDEX;
    ctx->bagIndex = 7;
    ctx->needsRedraw = TRUE;
}

void gfxtest_init_draw() {
    ctx = &sctx->gfxtest;
    if (ctx == NULL) return;

    gfx_load_tiles(gfx_tile_start);
    VDP_clearPlane(BG_A, TRUE);
}

void gfxtest_update() {
    u16 changed;

    if (ctx == NULL) return;

    changed = joyState & ~lastJoyState;

    if (changed & BUTTON_START) {
        currentState = STATE_TITLE;
        return;
    }

    if (changed & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT)) {
        ctx->needsRedraw = TRUE;
    }
}

void gfxtest_draw() {
    if (ctx == NULL) return;
    if (!ctx->needsRedraw) return;

    gfxtest_reroll_layout();
    ctx->needsRedraw = FALSE;
}

void gfxtest_cleanup() {
    ctx = NULL;
}
