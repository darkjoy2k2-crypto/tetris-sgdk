#include <genesis.h>
#include "game_view.h"
#include "game_core.h"
#include "game_logic.h"
#include "states.h"
#include "gfx.h"

static u16 tileCache[BOARD_WIDTH][BOARD_HEIGHT];

void view_init_cache() {
    for (u16 y = 0; y < BOARD_HEIGHT; y++)
        for (u16 x = 0; x < BOARD_WIDTH; x++)
            tileCache[x][y] = 0xFFFF;
}

void drawPreview(s16 type, u16 x, u16 y) {
    // Nur den 4x4 Bereich säubern
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_EMPTY_INDEX), x, y, 4, 4);
    if (type < 0) return;

    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[type][0][i][0];
        s16 py = PIECES[type][0][i][1];
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_BLOCK_BASE + type), x + px, y + py);
    }
}

void drawBoard() {
    if (ctx == NULL) return;

    // --- 1. HUD UPDATES (Nur bei Änderung!) ---
    if (ctx->score != ctx->lastScore) {
        char buf[12];
        // uintToStr ist schneller als sprintf für reine Zahlen!
        uintToStr(ctx->score, buf, 7); 
        VDP_drawText(buf, 8, 1); // Zeichnet den Wert hinter "SCORE:"
        ctx->lastScore = ctx->score;
    }

    if (ctx->level != ctx->lastLevel) {
        char buf[5];
        uintToStr(ctx->level, buf, 2);
        VDP_drawText(buf, 8, 3);
        ctx->lastLevel = ctx->level;
    }

    if (config.showNext && ctx->nextType != ctx->lastNextType) {
        if (ctx->lastNextType == -2) VDP_drawText("NEXT", UI_X, NEXT_Y - 1);
        drawPreview(ctx->nextType, UI_X, NEXT_Y);
        ctx->lastNextType = ctx->nextType;
    }

    if (config.allowHold && ctx->holdType != ctx->lastHoldType) {
        if (ctx->lastHoldType == -2) VDP_drawText("HOLD", UI_X, HOLD_Y - 1);
        drawPreview(ctx->holdType, UI_X, HOLD_Y);
        ctx->lastHoldType = ctx->holdType;
    }

    // --- 2. SPIELFELD RENDERING ---
    s16 ghostY = ctx->pieceY;
    if (config.showShadow && ctx->clearTimer == 0) {
        while (!checkCollision(ctx->pieceX, ghostY + 1, ctx->rotation)) ghostY++;
    }

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        // Blink-Animation (umgeht Cache)
        if (ctx->pendingLines[y] && ctx->clearTimer > 0) {
            bool blinkOn = (ctx->clearTimer % 4 < 2);
            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                u16 tile = blinkOn ? (TILE_BLOCK_BASE + (ctx->board[x][y] - 1)) : TILE_EMPTY_INDEX;
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
                tileCache[x][y] = 0xFFFF; 
            }
            continue;
        }

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 tile = TILE_EMPTY_INDEX;
            bool isDynamic = false;

            if (ctx->clearTimer == 0) {
                // Stein & Schatten
                for (u16 i = 0; i < 4; i++) {
                    if (x == ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0] && 
                        y == ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1]) {
                        tile = TILE_BLOCK_BASE + ctx->type; isDynamic = true; break;
                    }
                    if (config.showShadow && x == ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0] && 
                        y == ghostY + PIECES[ctx->type][ctx->rotation][i][1]) {
                        tile = TILE_GHOST_INDEX; isDynamic = true; break;
                    }
                }
            }

            if (!isDynamic && ctx->board[x][y] != 0) {
                tile = TILE_BLOCK_BASE + (ctx->board[x][y] - 1);
            }

            // DER PERFORMANCE-CHECK (Nur bei Änderung zum VDP schicken)
            if (tile != tileCache[x][y]) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
                tileCache[x][y] = tile;
            }
        }
    }
}