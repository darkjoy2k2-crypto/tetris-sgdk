#include <genesis.h>
#include "states/game/game_view.h"
#include "states/game/game_core.h"
#include "states/game/game_logic.h"
#include "states/states.h"
#include "gfx.h"
#include "fonts.h"
#include "bg.h"
#include "sprite.h"
#include <string.h>

// --- Statische Variablen & Cache ---
static u16 tileCache[BOARD_WIDTH][BOARD_HEIGHT];
u16 BG_TILE_START;
u16 GAME_TILE_START;
static u16 SKULL_TILE_IDX;
static u16 HEART_TILE_IDX;

// --- Initialisierung ---

void view_draw_debug_bag(GameContext* ctx) {
    if (ctx == NULL) return;

    // 1. Spalte: Die komplette 7-Bag (X=0, Y=0 bis 27)
    for (u16 b = 0; b < 7; b++) {
        u8 bType = ctx->bag[b];
        u16 anchorY = b << 2; // b * 4 via Bitshift (Abstand 4 Tiles)

        // Altes 4x4 Feld löschen
        VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START), 0, anchorY, 4, 4);

        // Bag-Piece zeichnen
        for (u16 i = 0; i < 4; i++) {
            s16 px = PIECES[bType][0][i][0];
            s16 py = PIECES[bType][0][i][1];
            
            // Markierung: Aktueller Bag-Index bekommt Highlight-Tile (GAME_TILE_START + 8)
            u16 tile = (b == ctx->bagIndex) ? (GAME_TILE_START + 8) : (GAME_TILE_START + 1 + bType);
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, tile), px, anchorY + py);
        }
    }

    // 2. Spalte: Aktueller Typ & Next Typ (X=5, Y=0 und Y=5)
    u16 col2X = 5;

    // Aktueller Piece (Type) an Y=0
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START), col2X, 0, 4, 4);
    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[ctx->type][0][i][0];
        s16 py = PIECES[ctx->type][0][i][1];
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START + 1 + ctx->type), col2X + px, py);
    }

    // Nächstes Piece (NextType) an Y=5 (Sicherheitsabstand zu Y=0)
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START), col2X, 5, 4, 4);
    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[ctx->nextType][0][i][0];
        s16 py = PIECES[ctx->nextType][0][i][1];
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START + 1 + ctx->nextType), col2X + px, 5 + py);
    }
}

void load_background() {
    u16 ind = TILE_USER_INDEX;
    VDP_drawImageEx(BG_A, &game_bg, TILE_ATTR_FULL(PAL2, 0, 0, 0, ind), 0, 0, FALSE, CPU);
    BG_TILE_START = ind;
    ind += game_bg.tileset->numTile;

    GAME_TILE_START = ind;
    gfx_load_tiles(GAME_TILE_START);
    ind += 9; 

    SKULL_TILE_IDX = ind++;
    VDP_loadTileData(tile_skull, SKULL_TILE_IDX, 1, CPU);
    HEART_TILE_IDX = ind++;
    VDP_loadTileData(tile_heart, HEART_TILE_IDX, 1, CPU);

    PAL_setPalette(PAL2, palette_black, CPU);
}

void view_init_cache() {
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            tileCache[x][y] = 0xFFFF; 
        }
    }
}

// --- Hilfsfunktionen ---

void drawPreview(s16 type, u16 x, u16 y) {
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START), x, y, 4, 2);
    if (type < 0) return;
    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[type][0][i][0];
        s16 py = PIECES[type][0][i][1];
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START + 1 + type), x + px, y + py);
    }
}

// --- UI Rendering ---


void view_draw_debug_memory()
{
    char str[16];
    VDP_setTextPalette(PAL3);

    // 1. Größe der Union (Fixer Footprint im RAM)
    intToStr(sizeof(StateUnion), str, 1);
    VDP_drawTextBG(VDP_BG_A, "UNION SIZE:", 0, 0);
    VDP_drawTextBG(VDP_BG_A, str, 12, 0);

    // 2. Aktuelle Adresse des GameContext Pointers (Hexadezimal)
    // Zeigt an, wo genau im RAM die "Brücke" gerade steht
    intToHex((u32)ctx, str, 8);
    VDP_drawTextBG(VDP_BG_A, "CTX ADDR :", 0, 1);
    VDP_drawTextBG(VDP_BG_A, str, 12, 1);

    // 3. Freier Heap (SGDK Speicherverwaltung)
    // Wenn dieser Wert sinkt, gibt es irgendwo ein MEM_alloc ohne MEM_free
    uintToStr(MEM_getFree(), str, 1);
    VDP_drawTextBG(VDP_BG_A, "FREE HEAP:", 0, 2);
    VDP_drawTextBG(VDP_BG_A, str, 12, 2);

}

void view_update_ui(GameContext* ctx) {
    if (ctx == NULL) return;
    VDP_setTextPalette(PAL3);

    if (ctx->score != ctx->lastScore) {
        char buf[12];
        uintToStr(ctx->score, buf, 6); 
        VDP_drawText(buf, UI_X - 1, 14); 
        ctx->lastScore = ctx->score;
    }

    if (ctx->level != ctx->lastLevel) {
        char buf[5];
        uintToStr(ctx->level, buf, 2);
        VDP_drawText(buf, UI_X + 3, 16);
        ctx->lastLevel = ctx->level;
    }

    u16 linesNext = 10 - (ctx->linesTotal % 10);
    if (linesNext != ctx->lastLinesNext) {
        char buf[5];
        uintToStr(linesNext, buf, 2);
        VDP_drawText(buf, UI_X + 3, 18);
        ctx->lastLinesNext = linesNext;
    }

    bool hideNext = (ctx->activeBadEffect == EFFECT_HIDE_NEXT);
if (GET_FLAG(config.flags, FLAG_NEXT)) {
    if (hideNext) drawPreview(-1, UI_X, NEXT_Y);
    else drawPreview(ctx->nextType, UI_X, NEXT_Y);

    ctx->lastNextType = ctx->nextType;
}

if (GET_FLAG(config.flags, FLAG_HOLD) && ctx->holdType != ctx->lastHoldType) {
            drawPreview(ctx->holdType, UI_X, HOLD_Y);
        ctx->lastHoldType = ctx->holdType;
    }

    if (ctx->comboCount != ctx->lastComboCount) {
        VDP_drawTextBG(VDP_BG_A, (ctx->comboCount <= 1) ? "    " : "COMBO", UI_X - 1, 20);
        ctx->lastComboCount = ctx->comboCount;
    }

    if (ctx->activeBadEffect != ctx->lastActiveBadEffect || ctx->badEffectTimer != ctx->lastBadEffectTimer) {
        if (ctx->activeBadEffect == EFFECT_NONE) {
            VDP_drawTextBG(VDP_BG_A, "      ", UI_X - 1, 24);
        } else {
            char statusMsg[20];
            char timerBuf[8];
            switch(ctx->activeBadEffect) {
                case EFFECT_FULLSPEED:  strncpy(statusMsg, " SPEED", 12); break;
                case EFFECT_SAME_TILES: strncpy(statusMsg, "SAME T", 12); break;
                case EFFECT_REVERSED:   strncpy(statusMsg, " SILLY", 12); break;
                case EFFECT_NO_ROTATE:  strncpy(statusMsg, " NOROT", 12); break;
                case EFFECT_HOLD_LOCK:  strncpy(statusMsg, "NOHOLD", 12); break;
                case EFFECT_HIDE_NEXT:  strncpy(statusMsg, "NONEXT", 12); break;
                case EFFECT_I_RAIN:     strncpy(statusMsg, "I-RAIN", 12); break;
                case EFFECT_FREEZE:     strncpy(statusMsg, "FREEZE", 12); break;
                default:                strncpy(statusMsg, "ACTIVE", 12); break;
            }
            if (ctx->activeBadEffect <= 2 || ctx->activeBadEffect == EFFECT_I_RAIN) {
                sprintf(timerBuf, "%d P", ctx->badEffectTimer);
            } else {
u16 sec = (ctx->badEffectTimer + (GET_TICKS(60) - 1)) / GET_TICKS(60);  
                sprintf(timerBuf, "%d S", sec);
            }
            VDP_drawTextBG(VDP_BG_A, statusMsg, UI_X - 1, 24);
            VDP_drawTextBG(VDP_BG_A, timerBuf, UI_X + 10, 24);
        }
        ctx->lastActiveBadEffect = ctx->activeBadEffect;
        ctx->lastBadEffectTimer = ctx->badEffectTimer;
    }
        view_draw_debug_memory();

}

// --- Haupt-Board Rendering ---

void drawBoard() {
    if (ctx == NULL) return;

    // 1. Board & Gitter zeichnen (Hintergrund und festliegende Steine)
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        // Blink-Check: Zeilen flackern während des Löschens
        bool isClearingRow = (ctx->clearTimer > 0 && GET_LINE_PENDING(y)); 
        bool showFlash = isClearingRow && ((ctx->clearTimer >> (GET_FLAG(config.flags, FLAG_IS_PAL) ? 1 : 2)) & 1);

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 tile;
            u8 cell = ctx->board[x][y];
            u8 priority = 1; // Standard für Blöcke: High Priority (1)

            if (showFlash) {
                tile = GAME_TILE_START + 8; // Weißes Flash-Tile
            } else if (cell != 0) {
                // Item- oder Block-Kacheln
                if (cell == ITEM_ID_SKULL) tile = SKULL_TILE_IDX;
                else if (cell == ITEM_ID_HEART) tile = HEART_TILE_IDX;
                else tile = GAME_TILE_START + 1 + (cell - 1);
            } else {
                // Leere Zelle (Spielfeld-Gitter)
                tile = GAME_TILE_START;
                priority = 0; // Gitter: Low Priority (0), damit Sprites davor liegen können
            }

            // Redraw nur bei Änderung (Tile Cache)
            if (tile != tileCache[x][y]) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, priority, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
                tileCache[x][y] = tile;
            }
        }
    }

    // 2. Schatten Rendering (Ghost Piece)
    if (GET_FLAG(config.flags, FLAG_SHADOW) && ctx->clearTimer == 0) {
        for (u16 i = 0; i < 4; i++) {
            s16 gx = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
            s16 gy = ctx->ghostY + PIECES[ctx->type][ctx->rotation][i][1];
            if (gy >= 0) {
                // Ghost Piece bleibt im Hintergrund (Priority 0)
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START + 8), RENDER_X + gx, RENDER_Y + gy);
                tileCache[gx][gy] = 0xFFFF; // Cache invalidieren für nächsten Frame
            }
        }
    }

    // 3. Aktiver Tetromino (Fallendes Teil)
    if (ctx->clearTimer == 0) {
        for (u16 i = 0; i < 4; i++) {
            s16 px = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
            s16 py = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];
            if (py >= 0) {
                // Ermittlung, ob dieser Block ein Item (Schädel/Herz) trägt
                u16 tile = (i == ctx->itemSlot) ? 
                           ((ctx->itemType == ITEM_ID_SKULL) ? SKULL_TILE_IDX : HEART_TILE_IDX) : 
                           (GAME_TILE_START + 1 + ctx->type);
                
                // AKTIVER TETROMINO: HIGH PRIORITY (1)
                // Ermöglicht es dem Sprite-System, den Schädel hinter dem Stein zu zeichnen
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 1, 0, 0, tile), RENDER_X + px, RENDER_Y + py);
                tileCache[px][py] = 0xFFFF;
            }
        }
    }

// RICHTIG: (Spalte + Versatz) * 8
Vect2D_s16 pPos = { 
    .x = (RENDER_X + ctx->pieceX) << 3, 
    .y = (RENDER_Y + ctx->pieceY) << 3 
};

Vect2D_s16 sPos = { 
    .x = (RENDER_X + ctx->pieceX) << 3, 
    .y = (RENDER_Y + ctx->ghostY) << 3
};

sprites_sync_game(pPos, sPos, ctx->activeBadEffect);
}

void view_animate_grayscale() {
    for (s16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (ctx->board[x][y] != 0) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START + 8), RENDER_X + x, RENDER_Y + y);
            }
        }
        SYS_doVBlankProcess();
    }
}

void view_fade_in_frame() {
    u16 target_pal[16];
    memcpy(target_pal, game_bg.palette->data, 16 * 2);
    target_pal[5]  = RGB24_TO_VDPCOLOR(0x222222);
    target_pal[6]  = RGB24_TO_VDPCOLOR(0xFFFFFF); 
    target_pal[7]  = RGB24_TO_VDPCOLOR(0x000044);
    target_pal[8]  = RGB24_TO_VDPCOLOR(0x0000FF);
    target_pal[9]  = RGB24_TO_VDPCOLOR(0xFFFF00);
    target_pal[10] = RGB24_TO_VDPCOLOR(0xFF00FF);
    target_pal[11] = RGB24_TO_VDPCOLOR(0x00FF00);
    target_pal[12] = RGB24_TO_VDPCOLOR(0xFF0000); 
    target_pal[13] = RGB24_TO_VDPCOLOR(0x5555FF);
    target_pal[14] = RGB24_TO_VDPCOLOR(0xFFA500);
    target_pal[15] = RGB24_TO_VDPCOLOR(0x444444);
PAL_fadeInPalette(PAL2, target_pal, GET_TICKS(30), FALSE);
}

void view_fade_out_frame() {
PAL_fadeOutPalette(PAL2, GET_TICKS(30), FALSE);
}