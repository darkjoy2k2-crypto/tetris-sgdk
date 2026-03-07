#include <genesis.h>
#include "game_view.h"
#include "game_core.h"
#include "game_logic.h"
#include "states.h"
#include "gfx.h"

// Der Tile-Cache speichert, was wir zuletzt an den VDP geschickt haben
static u16 tileCache[BOARD_WIDTH][BOARD_HEIGHT];

/**
 * Initialisiert den Cache mit einem ungültigen Wert, 
 * damit beim ersten drawBoard() alles einmal gezeichnet wird.
 */
void view_init_cache() {
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            tileCache[x][y] = 0xFFFF; 
        }
    }
    // Auch HUD-Cache im Context zurücksetzen
    if (ctx) {
        ctx->lastScore = 0xFFFFFFFF;
        ctx->lastLevel = 0xFFFF;
        ctx->lastNextType = -2;
        ctx->lastHoldType = -2;
    }
}

void drawPreview(s16 type, u16 x, u16 y) {
    // Bereich leeren
    for(u16 py=0; py<4; py++) {
        for(u16 px=0; px<4; px++) {
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_EMPTY_INDEX), x + px, y + py);
        }
    }
    if (type == -1) return;

    // Stein zeichnen
    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[type][0][i][0];
        s16 py = PIECES[type][0][i][1];
        u16 tile = TILE_BLOCK_BASE + type;
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), x + px, y + py);
    }
}

void drawBoard() {
    if (ctx == NULL) return;

    // --- 1. HUD UPDATES (Nur bei Änderung!) ---
    if (ctx->score != ctx->lastScore) {
        char buf[16];
        sprintf(buf, "SCORE: %07lu", ctx->score); // u32 = %lu
        VDP_drawText(buf, 1, 1);
        ctx->lastScore = ctx->score;
    }

    if (ctx->level != ctx->lastLevel) {
        char buf[12];
        sprintf(buf, "LEVEL: %d", ctx->level);
        VDP_drawText(buf, 1, 3);
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

    // --- 2. BOARD & PIECE RENDERING ---
    s16 ghostY = ctx->pieceY;
    if (config.showShadow && ctx->clearTimer == 0) {
        while (!checkCollision(ctx->pieceX, ghostY + 1, ctx->rotation)) {
            ghostY++;
        }
    }

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        // Sonderfall: Blink-Animation (umgeht den Cache für diese Zeilen)
        if (ctx->pendingLines[y] && ctx->clearTimer > 0) {
            bool blinkOn = (ctx->clearTimer % 4 < 2);
            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                u16 tile = blinkOn ? (TILE_BLOCK_BASE + (ctx->board[x][y] - 1)) : TILE_EMPTY_INDEX;
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
                tileCache[x][y] = 0xFFFF; // Cache invalidieren für nach der Animation
            }
            continue; 
        }

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 tile = TILE_EMPTY_INDEX;
            bool isPiece = false;

            if (ctx->clearTimer == 0) {
                // Aktiver Stein?
                for (u16 i = 0; i < 4; i++) {
                    if (x == ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0] && 
                        y == ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1]) {
                        tile = TILE_BLOCK_BASE + ctx->type;
                        isPiece = true; break;
                    }
                }
                // Ghost?
                if (!isPiece && config.showShadow) {
                    for (u16 i = 0; i < 4; i++) {
                        if (x == ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0] && 
                            y == ghostY + PIECES[ctx->type][ctx->rotation][i][1]) {
                            tile = TILE_GHOST_INDEX;
                            isPiece = true; break;
                        }
                    }
                }
            }

            // Festes Board?
            if (!isPiece && ctx->board[x][y] != 0) {
                tile = TILE_BLOCK_BASE + (ctx->board[x][y] - 1);
            }

            // --- PERFORMANCE CHECK ---
            // Nur an VDP senden, wenn das Tile anders ist als im letzten Frame
            if (tile != tileCache[x][y]) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
                tileCache[x][y] = tile;
            }
        }
    }

    // Kommentare (HUD)
    if (ctx->commentTimer > 0) {
        VDP_drawText(ctx->lastComment, RENDER_X, RENDER_Y + BOARD_HEIGHT + 1);
        ctx->commentTimer--;
        if (ctx->commentTimer == 0) VDP_clearTextArea(RENDER_X, RENDER_Y + BOARD_HEIGHT + 1, 20, 1);
    }
}