#include <genesis.h>
#include "game_view.h"
#include "game_core.h"
#include "game_logic.h"
#include "states.h"
#include "gfx.h"
#include "fonts.h"

static u16 tileCache[BOARD_WIDTH][BOARD_HEIGHT];

void view_init_cache() {
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            tileCache[x][y] = 0xFFFF; 
        }
    }
}

void drawPreview(s16 type, u16 x, u16 y) {
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_EMPTY_INDEX), x, y, 4, 4);
    if (type < 0) return;

    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[type][0][i][0];
        s16 py = PIECES[type][0][i][1];
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_BLOCK_BASE + type), x + px, y + py);
    }
}

void drawBoard() {
    if (ctx == NULL) return;

    VDP_setTextPalette(PAL3);

    if (ctx->score != ctx->lastScore) {
        char buf[12];
        uintToStr(ctx->score, buf, 7); 
        VDP_drawText(buf, 8, 1);
        ctx->lastScore = ctx->score;
    }

    if (ctx->level != ctx->lastLevel) {
        char buf[5];
        uintToStr(ctx->level, buf, 2);
        VDP_drawText(buf, 8, 3);
        ctx->lastLevel = ctx->level;
    }

    if (config.showNext && ctx->nextType != ctx->lastNextType) {
        drawPreview(ctx->nextType, UI_X, NEXT_Y);
        ctx->lastNextType = ctx->nextType;
    }

    if (config.allowHold && ctx->holdType != ctx->lastHoldType) {
        drawPreview(ctx->holdType, UI_X, HOLD_Y);
        ctx->lastHoldType = ctx->holdType;
    }

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
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
            if (ctx->board[x][y] != 0) {
                tile = TILE_BLOCK_BASE + (ctx->board[x][y] - 1);
            }

            if (tile != tileCache[x][y]) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
                tileCache[x][y] = tile;
            }
        }
    }

    if (config.showShadow && ctx->clearTimer == 0) {
        for (u16 i = 0; i < 4; i++) {
            s16 gx = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
            s16 gy = ctx->ghostY + PIECES[ctx->type][ctx->rotation][i][1];
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_GHOST_INDEX), RENDER_X + gx, RENDER_Y + gy);
            tileCache[gx][gy] = 0xFFFF; 
        }
    }

    if (ctx->clearTimer == 0) {
        for (u16 i = 0; i < 4; i++) {
            s16 px = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
            s16 py = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_BLOCK_BASE + ctx->type), RENDER_X + px, RENDER_Y + py);
            tileCache[px][py] = 0xFFFF;
        }
    }

    if (ctx->commentTimer > 0) {
        VDP_drawText(ctx->lastComment, RENDER_X, RENDER_Y + BOARD_HEIGHT + 1);
        ctx->commentTimer--;
        if (ctx->commentTimer == 0) {
            VDP_clearTextArea(RENDER_X, RENDER_Y + BOARD_HEIGHT + 1, 20, 1);
        }
    }
}