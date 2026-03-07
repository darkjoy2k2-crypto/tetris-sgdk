#include <genesis.h>
#include "game_view.h"
#include "game_core.h"
#include "game_logic.h"
#include "gfx.h"

// Hilfsfunktion: Zeichnet eine 4x4 Vorschau für NEXT oder HOLD
void drawPreview(s16 type, u16 x, u16 y) {
    // 1. Den 4x4 Bereich zuerst leeren
    for(u16 py=0; py<4; py++) {
        for(u16 px=0; px<4; px++) {
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_EMPTY_INDEX), x + px, y + py);
        }
    }

    // Wenn kein Stein vorhanden ist (z.B. Hold am Anfang), abbrechen
    if (type == -1) return;

    // 2. Den Stein in seiner Standard-Rotation (0) zeichnen
    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[type][0][i][0];
        s16 py = PIECES[type][0][i][1];
        
        // Wir nutzen die neuen farbigen Tiles mit Lichtkanten
        u16 tile = TILE_BLOCK_BASE + type;
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), x + px, y + py);
    }
}

void drawBoard() {
    if (ctx == NULL) return;

    // 1. Ghost-Y Berechnung (nur wenn keine Zeilen-Animation läuft)
    s16 ghostY = ctx->pieceY;
    if (ctx->clearTimer == 0) {
        while (!checkCollision(ctx->pieceX, ghostY + 1, ctx->rotation)) {
            ghostY++;
        }
    }

    // 2. Das Spielfeld-Grid zeichnen
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        
        // --- SONDERFALL: BLINK-ANIMATION ---
        if (ctx->pendingLines[y] && ctx->clearTimer > 0) {
            // Wir prüfen, ob wir im "An" oder "Aus" Teil des Blinkens sind
            // (ctx->clearTimer % 8 < 4) ergibt alle 4 Frames einen Wechsel
            bool blinkOn = (ctx->clearTimer % 4 < 2);

            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                u16 tile;
                if (blinkOn) {
                    // "An"-Phase: Nutze die Originalfarbe des Steins aus dem Board
                    // Da board[x][y] den Wert (Typ + 1) speichert, ziehen wir 1 ab
                    tile = TILE_BLOCK_BASE + (ctx->board[x][y] - 1);
                } else {
                    // "Aus"-Phase: Zeige das leere Hintergrund-Tile
                    tile = TILE_EMPTY_INDEX;
                }
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
            }
            continue; // Springe zum nächsten 'y', da diese Zeile fertig behandelt ist
        }

        // --- REGULÄRES ZEICHNEN (Wenn keine Animation für diese Zeile läuft) ---
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 tile = TILE_EMPTY_INDEX;
            bool active = false;

            // Nur aktiven Stein und Schatten zeichnen, wenn keine generelle Lösch-Pause aktiv ist
            if (ctx->clearTimer == 0) {
                // A) Aktiver Stein
                for (u16 i = 0; i < 4; i++) {
                    if (x == ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0] && 
                        y == ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1]) {
                        tile = TILE_BLOCK_BASE + ctx->type;
                        active = true; break;
                    }
                }
                // B) Ghost-Piece (Schatten)
                if (!active) {
                    for (u16 i = 0; i < 4; i++) {
                        if (x == ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0] && 
                            y == ghostY + PIECES[ctx->type][ctx->rotation][i][1]) {
                            tile = TILE_GHOST_INDEX;
                            active = true; break;
                        }
                    }
                }
            }

            // C) Festes Board (Bereits platzierte Steine)
            if (!active && ctx->board[x][y] != 0) {
                tile = TILE_BLOCK_BASE + (ctx->board[x][y] - 1);
            }

            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
        }
    }

    // --- UI ELEMENTE (SCORE, LEVEL, KOMMENTARE) ---
    char buf[32];
    sprintf(buf, "SCORE: %07ld", ctx->score);
    VDP_drawText(buf, 1, 1);
    
    sprintf(buf, "LEVEL: %d", ctx->level);
    VDP_drawText(buf, 1, 3);

    // Zeitgesteuerter Kommentar (Tetris!, Combo etc.)
    if (ctx->commentTimer > 0 && ctx->commentTimer <= 60) {
        VDP_drawText(ctx->lastComment, RENDER_X, RENDER_Y + BOARD_HEIGHT + 1);
        ctx->commentTimer--;
        if (ctx->commentTimer == 0) {
            VDP_clearTextArea(RENDER_X, RENDER_Y + BOARD_HEIGHT + 1, 20, 1);
        }
    }

    // --- VORSCHAU-FENSTER (NEXT & HOLD) ---
    VDP_drawText("NEXT", UI_X, NEXT_Y - 1);
    drawPreview(ctx->nextType, UI_X, NEXT_Y);
    
    VDP_drawText("HOLD", UI_X, HOLD_Y - 1);
    drawPreview(ctx->holdType, UI_X, HOLD_Y);
}