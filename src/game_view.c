#include <genesis.h>
#include "game_view.h"
#include "game_core.h"
#include "game_logic.h"
#include "states.h"
#include "gfx.h"
#include "fonts.h"
#include "bg.h"
#include <string.h>

static u16 tileCache[BOARD_WIDTH][BOARD_HEIGHT];
u16 BG_TILE_START;
u16 GAME_TILE_START;
static u16 SKULL_TILE_IDX;
static u16 HEART_TILE_IDX;

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

void view_animate_grayscale() {
    for (s16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (ctx->board[x][y] != 0) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START + 8), RENDER_X + x, RENDER_Y + y);
            }
        }
        SYS_doVBlankProcess();
        SYS_doVBlankProcess();
    }
}

void view_fade_in_frame() {
    u16 target_pal[16];
    memcpy(target_pal, game_bg.palette->data, 16 * 2);
    
    target_pal[5]  = RGB24_TO_VDPCOLOR(0x222222);
    target_pal[6]  = RGB24_TO_VDPCOLOR(0xFFFFFF); // Skull
    target_pal[7]  = RGB24_TO_VDPCOLOR(0x000044);
    target_pal[8]  = RGB24_TO_VDPCOLOR(0x0000FF);
    target_pal[9]  = RGB24_TO_VDPCOLOR(0xFFFF00);
    target_pal[10] = RGB24_TO_VDPCOLOR(0xFF00FF);
    target_pal[11] = RGB24_TO_VDPCOLOR(0x00FF00);
    target_pal[12] = RGB24_TO_VDPCOLOR(0xFF0000); // Heart
    target_pal[13] = RGB24_TO_VDPCOLOR(0x5555FF);
    target_pal[14] = RGB24_TO_VDPCOLOR(0xFFA500);
    target_pal[15] = RGB24_TO_VDPCOLOR(0x444444);

    PAL_fadeInPalette(PAL2, target_pal, 30, FALSE);
}

void view_fade_out_frame() {
    PAL_fadeOutPalette(PAL2, 30, FALSE);
}

void view_init_cache() {
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            tileCache[x][y] = 0xFFFF; 
        }
    }
}

void drawPreview(s16 type, u16 x, u16 y) {
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START), x, y, 4, 2);
    if (type < 0) return;
    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[type][0][i][0];
        s16 py = PIECES[type][0][i][1];
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START + 1 + type), x + px, y + py);
    }
}

void drawBoard() {
    if (ctx == NULL) return;
    VDP_setTextPalette(PAL3);

    // Score & Level Anzeige
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

    // Nächste-Zeilen-Logik
    u16 linesNext = 10 - (ctx->linesTotal % 10);
    if (linesNext != ctx->lastLinesNext) {
        char buf[5];
        uintToStr(linesNext, buf, 2);
        VDP_drawText(buf, UI_X + 3, 18);
        ctx->lastLinesNext = linesNext;
    }

    // Next Piece (Verschwindet bei Bad Effect)
    bool hideNext = (ctx->activeBadEffect == EFFECT_HIDE_NEXT);
    if (config.showNext && ctx->nextType != ctx->lastNextType) {
        if (hideNext) drawPreview(-1, UI_X, NEXT_Y);
        else drawPreview(ctx->nextType, UI_X, NEXT_Y);
        ctx->lastNextType = ctx->nextType;
    }

    // Hold Piece
    if (config.allowHold && ctx->holdType != ctx->lastHoldType) {
        drawPreview(ctx->holdType, UI_X, HOLD_Y);
        ctx->lastHoldType = ctx->holdType;
    }

    // Das statische Spielfeld zeichnen
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 tile = GAME_TILE_START;
            u8 cell = ctx->board[x][y];

            if (cell != 0) {
                if (cell == ITEM_ID_SKULL) tile = SKULL_TILE_IDX;
                else if (cell == ITEM_ID_HEART) tile = HEART_TILE_IDX;
                else tile = GAME_TILE_START + 1 + (cell - 1);
            }

            if (tile != tileCache[x][y]) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, tile), RENDER_X + x, RENDER_Y + y);
                tileCache[x][y] = tile;
            }
        }
    }

    // Schatten-Piece
    if (config.showShadow && ctx->clearTimer == 0) {
        for (u16 i = 0; i < 4; i++) {
            s16 gx = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
            s16 gy = ctx->ghostY + PIECES[ctx->type][ctx->rotation][i][1];
            if (gy >= 0) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, GAME_TILE_START + 8), RENDER_X + gx, RENDER_Y + gy);
                tileCache[gx][gy] = 0xFFFF;
            }
        }
    }

    // Aktives Piece inkl. Item-Slot (dreht sich mit!)
// Das hier hast du schon und das ist 100% richtig!
if (ctx->clearTimer == 0) {
    for (u16 i = 0; i < 4; i++) {
        s16 px = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
        s16 py = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];
        
        if (py >= 0) {
            u16 tile;
            // Diese Abfrage hier ist die "Magie"
            if (i == ctx->itemSlot) {
                tile = (ctx->itemType == ITEM_ID_SKULL) ? SKULL_TILE_IDX : HEART_TILE_IDX;
            } else {
                tile = GAME_TILE_START + 1 + ctx->type;
            }
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, tile), RENDER_X + px, RENDER_Y + py);
            tileCache[px][py] = 0xFFFF;
        }
    }
}

    // Combo Anzeige
    if (ctx->comboCount != ctx->lastComboCount) {
        VDP_drawTextBG(VDP_BG_A, (ctx->comboCount <= 1) ? "    " : "COMBO", UI_X - 1, 20);
        ctx->lastComboCount = ctx->comboCount;
    }

    // --- DEBUFF ANZEIGE ---
    // Wir prüfen, ob sich der Effekt oder der Timer geändert hat, um VRAM-Last zu sparen
// --- DEBUFF ANZEIGE ---
    if (ctx->activeBadEffect != ctx->lastActiveBadEffect || ctx->badEffectTimer != ctx->lastBadEffectTimer) {
        
        if (ctx->activeBadEffect == EFFECT_NONE) {
            // Zeile säubern, wenn kein Effekt aktiv ist (verwende genug Leerzeichen)
            VDP_drawTextBG(VDP_BG_A, "      ", UI_X - 1, 24);
        } else {
            char statusMsg[20];
            char timerBuf[8];

            // 1. Text festlegen
            switch(ctx->activeBadEffect) {
                case EFFECT_FULLSPEED:  strncpy(statusMsg, " SPEED", 12); break;
                case EFFECT_SAME_TILES: strncpy(statusMsg, "SAME T", 12); break;
                case EFFECT_REVERSED:   strncpy(statusMsg, " SILLY", 12); break;
                case EFFECT_NO_ROTATE:  strncpy(statusMsg, " NOROT", 12); break;
                case EFFECT_HOLD_LOCK:  strncpy(statusMsg, "NOHOLD", 12); break;
                case EFFECT_HIDE_NEXT:  strncpy(statusMsg, "NONEXT", 12); break;
                default:                strncpy(statusMsg, "DEBUFF", 12); break;
            }

            // 2. Timer-Format wählen (Stücke oder Sekunden)
            if (ctx->activeBadEffect <= 2) {
                // Stück-Anzeige (EFFECT_FULLSPEED und EFFECT_SAME_TILES)
                sprintf(timerBuf, "%d P", ctx->badEffectTimer);
            } else {
                // Zeit-Anzeige (Frames in Sekunden umrechnen)
                u16 sec = (ctx->badEffectTimer + 59) / 60;
                sprintf(timerBuf, "%d S", sec);
            }

            // 3. Zeichnen (Klammern sind jetzt korrekt!)
            VDP_drawTextBG(VDP_BG_A, statusMsg, UI_X - 1, 24);
            VDP_drawTextBG(VDP_BG_A, timerBuf, UI_X + 10, 24);
        }

        ctx->lastActiveBadEffect = ctx->activeBadEffect;
        ctx->lastBadEffectTimer = ctx->badEffectTimer;
    }
}