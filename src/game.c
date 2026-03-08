#include <genesis.h>
#include "game_core.h"
#include "game_logic.h"
#include "game_view.h"
#include "gfx.h"
#include "sound_manager.h"
#include "states.h"
#include "menu_bg.h"
#include "fonts.h"
#include <string.h>

const u16 GRAVITY_SPEEDS[] = { 9999, 60, 30, 15 };
const u16 GARBAGE_INTERVALS[] = { 0, 1200, 600, 300 };

GameContext* ctx = NULL;

void game_init() {
    menu_bg_set_active(false);

    if (ctx != NULL) MEM_free(ctx); 
    ctx = MEM_alloc(sizeof(GameContext));
    memset(ctx, 0, sizeof(GameContext)); 
    
    SOUND_init();
    gfx_init();

    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    VDP_setTextPalette(PAL3);

    ctx->lastScore = 0xFFFFFFFF; 
    ctx->lastLevel = 0xFFFF;
    ctx->lastNextType = -2;
    ctx->lastHoldType = -2;

    view_init_cache(); 

    ctx->score = 0;
    ctx->level = 1;
    ctx->linesTotal = 0;
    ctx->moveTimer = 0;
    ctx->holdType = -1;
    ctx->canHold = true;
    
    ctx->garbageTimer = 0;
    if (config.garbageFreq > 0) {
        u16 base = GARBAGE_INTERVALS[config.garbageFreq];
        ctx->garbageNextThreshold = base + (random() % 120) - 60;
    }

    refillBag();
    ctx->nextType = ctx->bag[ctx->bagIndex];
    ctx->bagIndex++;
    
    VDP_clearTextArea(0, 0, 40, 28);
    VDP_drawText("SCORE:", 1, 1);
    VDP_drawText("LEVEL:", 1, 3);

    if (config.showNext) {
        VDP_drawText("NEXT:", UI_X, NEXT_Y - 1);
    }
    if (config.allowHold) {
        VDP_drawText("HOLD:", UI_X, HOLD_Y - 1);
    }

    spawnPiece();
    drawBoard();
}

void game_update() {
    if (ctx == NULL) return;

    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            spawnPiece();
            ctx->ghostY = ctx->pieceY;
            while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) {
                ctx->ghostY++;
            }
        }
        drawBoard();
        return;
    }

    u16 changed = joyState & ~lastJoyState;
    bool moved = false;

    const u16 dasDelay = 10;
    const u16 dasRepeat = 3;
    u16 currentDir = 0;

    if (joyState & BUTTON_LEFT) currentDir = BUTTON_LEFT;
    else if (joyState & BUTTON_RIGHT) currentDir = BUTTON_RIGHT;

    if (currentDir != 0) {
        if (changed & currentDir) {
            s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
            if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                ctx->pieceX += step;
                moved = true;
                SOUND_play(SND_MOVE);
            }
            ctx->dasTimer = 0;
            ctx->dasDir = currentDir;
        } else if (ctx->dasDir == currentDir) {
            ctx->dasTimer++;
            if (ctx->dasTimer >= dasDelay) {
                if ((ctx->dasTimer - dasDelay) % dasRepeat == 0) {
                    s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
                    if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                        ctx->pieceX += step;
                        moved = true;
                        SOUND_play(SND_MOVE);
                    }
                }
            }
        }
    } else {
        ctx->dasTimer = 0;
        ctx->dasDir = 0;
    }

    if (changed & (BUTTON_A | BUTTON_B)) {
        u16 nr = (changed & BUTTON_A) ? (ctx->rotation + 3) % 4 : (ctx->rotation + 1) % 4;
        if (!checkCollision(ctx->pieceX, ctx->pieceY, nr)) {
            ctx->rotation = nr;
            moved = true;
            SOUND_play(SND_ROTATE);
        }
    }

    if (config.allowHold && (changed & BUTTON_C)) {
        performHold();
        moved = true; 
    }

    if (changed & BUTTON_UP) {
        SOUND_play(SND_HARD_DROP);
        while (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
        }
        lockPiece();
        if (clearLines() == 0) {
            spawnPiece();
            moved = true;
        }
    } else {
        ctx->moveTimer++;
        s16 baseThreshold = (s16)GRAVITY_SPEEDS[config.speedLevel];
        s16 threshold = baseThreshold;
        if (config.speedLevel > 0) {
            threshold = baseThreshold - ((ctx->level - 1) * 4);
        }
        if (threshold < 2) threshold = 2;

        u16 finalThreshold = (joyState & BUTTON_DOWN) ? 2 : (u16)threshold;

        if (ctx->moveTimer >= finalThreshold) {
            if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
                ctx->pieceY++;
                moved = true;
                if (joyState & BUTTON_DOWN) SOUND_play(SND_SOFT_DROP);
            } else {
                lockPiece();
                if (clearLines() == 0) {
                    spawnPiece();
                    moved = true;
                }
            }
            ctx->moveTimer = 0;
        }
    }

    if (moved && config.showShadow) {
        ctx->ghostY = ctx->pieceY;
        while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) {
            ctx->ghostY++;
        }
    }

    if (config.garbageFreq > 0 && ctx->clearTimer == 0) {
        ctx->garbageTimer++;
        if (ctx->garbageTimer >= ctx->garbageNextThreshold) {
            addGarbageLine();
            ctx->garbageTimer = 0;
            u16 base = GARBAGE_INTERVALS[config.garbageFreq];
            ctx->garbageNextThreshold = base + (random() % 120) - 60;
            moved = true;
        }
    }

    drawBoard();
}

void game_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearTextArea(0, 0, 40, 28);
}