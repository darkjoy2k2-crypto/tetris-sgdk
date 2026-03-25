#include <genesis.h>
#include <string.h>

#include "states/vs_state.h"
#include "states/vs/brain.h"
#include "states/states.h"
#include "states/game/game_core.h"
#include "states/game/game_logic.h"
#include "states/game/game_view.h"
#include "gfx.h"
#include "fonts.h"
#include "menu_bg.h"
#include "sound_manager.h"

#define VS_LEFT_X   2
#define VS_RIGHT_X 22
#define VS_BOARD_Y  4

static VsContext* vctx = NULL;
static u16 vsTileStart = TILE_USER_INDEX;

// Per-player tile caches: only write to VDP when tile actually changes.
static u16 leftCache[200];
static u16 rightCache[200];
static u16 debugOverlayTimer = 0;

static void vs_draw_debug_overlay(void) {
    char line1[32];
    char line2[32];

    if (debugOverlayTimer > 0) {
        debugOverlayTimer--;
        return;
    }

    debugOverlayTimer = GET_TICKS(60);

    sprintf(line1, "CPU %3u%% B%u   ", SYS_getCPULoad(), vctx->rightAiThinkBudget);
    sprintf(line2, "AI %u P%2u G%2u ", vctx->rightAiState, vctx->rightAiPulseTimer, vctx->right.moveTimer);

    VDP_drawText(line1, 1, 26);
    VDP_drawText(line2, 1, 27);
}

static void bind_player(GameContext* player, u16 joyNow, u16 joyPrev, GameContext** savedCtx, u16* savedJoy, u16* savedLastJoy) {
    *savedCtx = ctx;
    *savedJoy = joyState;
    *savedLastJoy = lastJoyState;

    ctx = player;
    joyState = joyNow;
    lastJoyState = joyPrev;
}

static void unbind_player(GameContext* savedCtx, u16 savedJoy, u16 savedLastJoy) {
    ctx = savedCtx;
    joyState = savedJoy;
    lastJoyState = savedLastJoy;
}

static void set_vs_palette(void) {
    PAL_setColor(32 + 1, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(32 + 5, RGB24_TO_VDPCOLOR(0x222222));
    PAL_setColor(32 + 6, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(32 + 7, RGB24_TO_VDPCOLOR(0x000044));
    PAL_setColor(32 + 8, RGB24_TO_VDPCOLOR(0x0000FF));
    PAL_setColor(32 + 9, RGB24_TO_VDPCOLOR(0xFFFF00));
    PAL_setColor(32 + 10, RGB24_TO_VDPCOLOR(0xFF00FF));
    PAL_setColor(32 + 11, RGB24_TO_VDPCOLOR(0x00FF00));
    PAL_setColor(32 + 12, RGB24_TO_VDPCOLOR(0xFF0000));
    PAL_setColor(32 + 13, RGB24_TO_VDPCOLOR(0x5555FF));
    PAL_setColor(32 + 14, RGB24_TO_VDPCOLOR(0xFFA500));
    PAL_setColor(32 + 15, RGB24_TO_VDPCOLOR(0x444444));
}

static bool vs_spawn_piece(GameContext* player) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);

    player->type = player->nextType;

    if (player->bagIndex >= 7) {
        refillBag();
    }
    player->nextType = player->bag[player->bagIndex++];

    player->rotation = 0;
    player->pieceX = 3;
    player->pieceY = (player->type == 0) ? -1 : 0;
    player->itemSlot = ITEM_ID_NONE;
    player->itemType = ITEM_ID_NONE;
    player->moveTimer = 0;

    calculate_ghost_y();
    player->boardFlags |= GF_NEEDS_DRAW;

    {
        bool blocked = checkCollision(player->pieceX, player->pieceY, player->rotation);
        unbind_player(savedCtx, savedJoy, savedLastJoy);
        return !blocked;
    }
}

static void vs_reset_player(GameContext* player) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;

    memset(player, 0, sizeof(GameContext));

    player->holdType = -1;
    player->lastHoldType = -2;
    player->lastNextType = -2;
    player->lastScore = 0xFFFFFFFF;
    player->lastLevel = 0xFFFF;
    player->lastLinesNext = 0xFFFF;
    player->lastComboCount = 0xFFFF;
    player->lastActiveBadEffect = 99;
    player->lastBadEffectTimer = -1;
    player->level = 1;
    player->startLevel = 1;
    player->flags = GF_CAN_HOLD;
    player->boardFlags = GF_NEEDS_DRAW;
    player->dasNextThreshold = config.thresholdLRInitial;

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);
    refillBag();
    player->nextType = player->bag[player->bagIndex++];
    unbind_player(savedCtx, savedJoy, savedLastJoy);

    vs_spawn_piece(player);
}

static u16 tile_for_cell(u8 cell) {
    if (cell == 0) return vsTileStart;
    if (cell >= 1 && cell <= 7) return (u16)(vsTileStart + cell);
    if (cell == ITEM_ID_SKULL) return (u16)(vsTileStart + 8);
    if (cell == ITEM_ID_HEART) return (u16)(vsTileStart + 8);
    return vsTileStart;
}

// Draws one player's board using a tile cache  VDP only written when tile changes.
// Returns early if boardFlags does not have GF_NEEDS_DRAW.
static void vs_draw_player_board(GameContext* player, u16 ox, u16 oy, bool isDead, u16* cache, bool* needsRedraw) {
    if (!(*needsRedraw)) return;

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 rowOffset = (u16)((y << 3) + (y << 1));
            u8 cell = player->board[rowOffset + x];
            u16 tile = tile_for_cell(cell);
            u16 attr;

            if (!isDead) {
                // Check active piece
                bool isPiece = FALSE;
                for (u16 i = 0; i < 4; i++) {
                    s16 px = player->pieceX + PIECES[player->type][player->rotation][i][0];
                    s16 py = player->pieceY + PIECES[player->type][player->rotation][i][1];
                    if (px == (s16)x && py == (s16)y) { isPiece = TRUE; break; }
                }
                if (isPiece) {
                    attr = TILE_ATTR_FULL(PAL2, 1, 0, 0, (u16)(vsTileStart + 1 + player->type));
                } else if (GET_FLAG(config.flags, FLAG_SHADOW)) {
                    bool isGhost = FALSE;
                    for (u16 i = 0; i < 4; i++) {
                        s16 sx = player->pieceX + PIECES[player->type][player->rotation][i][0];
                        s16 sy = player->ghostY + PIECES[player->type][player->rotation][i][1];
                        if (sx == (s16)x && sy == (s16)y) { isGhost = TRUE; break; }
                    }
                    attr = isGhost
                        ? TILE_ATTR_FULL(PAL2, 0, 0, 0, (u16)(vsTileStart + 8))
                        : TILE_ATTR_FULL(PAL2, 0, 0, 0, tile);
                } else {
                    attr = TILE_ATTR_FULL(PAL2, 0, 0, 0, tile);
                }
            } else {
                attr = TILE_ATTR_FULL(PAL2, 0, 0, 0, tile);
            }

            u16 cacheIdx = (u16)(y * BOARD_WIDTH + x);
            if (cache[cacheIdx] != attr) {
                cache[cacheIdx] = attr;
                VDP_setTileMapXY(BG_A, attr, ox + x, oy + y);
            }
        }
    }

    player->boardFlags &= ~GF_NEEDS_DRAW;
    *needsRedraw = FALSE;
}

static u16 vs_clear_lines_simple(GameContext* player) {
    u16 cleared = 0;

    for (s16 y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool full = TRUE;
        u16 rowOffset = (u16)((y << 3) + (y << 1));

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (player->board[rowOffset + x] == 0) {
                full = FALSE;
                break;
            }
        }

        if (full) {
            cleared++;

            for (s16 yy = y; yy > 0; yy--) {
                u16 dst = (u16)((yy << 3) + (yy << 1));
                u16 src = (u16)(((yy - 1) << 3) + ((yy - 1) << 1));
                memcpy(&player->board[dst], &player->board[src], BOARD_WIDTH);
            }
            memset(&player->board[0], 0, BOARD_WIDTH);
            y++;
        }
    }

    if (cleared > 0) {
        player->linesTotal = (u16)(player->linesTotal + cleared);
        player->score += (u32)(cleared * 100);
        player->level = (u16)(1 + (player->linesTotal / 10));
    }

    return cleared;
}

static bool vs_lock_and_respawn(GameContext* player) {
    for (u16 i = 0; i < 4; i++) {
        s16 gx = player->pieceX + PIECES[player->type][player->rotation][i][0];
        s16 gy = player->pieceY + PIECES[player->type][player->rotation][i][1];

        if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
            player->board[gx + ((gy << 3) + (gy << 1))] = (u8)(player->type + 1);
        }
    }

    vs_clear_lines_simple(player);
    return vs_spawn_piece(player);
}

static bool vs_try_rotate(GameContext* player, u16 newRotation) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;
    s16 kicks[5] = {0, 1, -1, 2, -2};

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);

    for (u16 i = 0; i < 5; i++) {
        if (!checkCollision(player->pieceX + kicks[i], player->pieceY, newRotation)) {
            player->pieceX += kicks[i];
            player->rotation = newRotation;
            calculate_ghost_y();
            unbind_player(savedCtx, savedJoy, savedLastJoy);
            return TRUE;
        }
    }

    unbind_player(savedCtx, savedJoy, savedLastJoy);
    return FALSE;
}

static bool vs_try_step_down(GameContext* player) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);

    if (!checkCollision(player->pieceX, player->pieceY + 1, player->rotation)) {
        player->pieceY++;
        calculate_ghost_y();
        unbind_player(savedCtx, savedJoy, savedLastJoy);
        return TRUE;
    }

    unbind_player(savedCtx, savedJoy, savedLastJoy);
    return FALSE;
}

static void vs_update_player(GameContext* player, u16 joyNow, u16 joyPrev, bool* deadFlag, bool* needsRedraw) {
    u16 changed;
    u16 currentDir;
    bool dirty = FALSE;

    if (*deadFlag) return;

    changed = joyNow & ~joyPrev;

    // Left / Right movement with DAS
    currentDir = (joyNow & BUTTON_LEFT) ? BUTTON_LEFT : ((joyNow & BUTTON_RIGHT) ? BUTTON_RIGHT : 0);
    if (currentDir != 0) {
        if (changed & currentDir) {
            s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
            GameContext* savedCtx;
            u16 savedJoy;
            u16 savedLastJoy;

            bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);
            if (!checkCollision(player->pieceX + step, player->pieceY, player->rotation)) {
                player->pieceX += step;
                calculate_ghost_y();
                dirty = TRUE;
            }
            unbind_player(savedCtx, savedJoy, savedLastJoy);

            player->dasTimer = 0;
            player->dasDir = currentDir;
            player->dasNextThreshold = config.thresholdLRInitial;
        } else if (player->dasDir == currentDir) {
            player->dasTimer++;
            if (player->dasTimer >= player->dasNextThreshold) {
                s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
                GameContext* savedCtx;
                u16 savedJoy;
                u16 savedLastJoy;

                bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);
                if (!checkCollision(player->pieceX + step, player->pieceY, player->rotation)) {
                    player->pieceX += step;
                    calculate_ghost_y();
                    dirty = TRUE;
                }
                unbind_player(savedCtx, savedJoy, savedLastJoy);

                player->dasTimer = 0;
                player->dasNextThreshold = config.thresholdLRRepeat;
            }
        }
    } else {
        player->dasTimer = 0;
        player->dasDir = 0;
        player->dasNextThreshold = config.thresholdLRInitial;
    }

    // A = CCW, B = CW  (matches normal game controls)
    if (changed & BUTTON_A) {
        if (vs_try_rotate(player, (u16)((player->rotation + 3) & 3))) {
            dirty = TRUE;
        }
    }
    if (changed & BUTTON_B) {
        if (vs_try_rotate(player, (u16)((player->rotation + 1) & 3))) {
            dirty = TRUE;
        }
    }

    // Up = hard drop
    if (changed & BUTTON_UP) {
        while (vs_try_step_down(player)) { }
        dirty = TRUE;
        if (!vs_lock_and_respawn(player)) {
            *deadFlag = TRUE;
        }
    }

    // Gravity + soft drop
    {
        s16 threshold = (s16)GET_TICKS(48 - (player->level > 1 ? (player->level - 1) * 2 : 0));
        if (threshold < 2) threshold = 2;

        if (joyNow & BUTTON_DOWN) {
            threshold = (s16)config.thresholdSD;
            if (threshold < 1) threshold = 1;
        }

        player->moveTimer++;
        if (player->moveTimer >= (u16)threshold) {
            if (!vs_try_step_down(player)) {
                if (!vs_lock_and_respawn(player)) {
                    *deadFlag = TRUE;
                }
                dirty = TRUE;
            } else {
                dirty = TRUE;
            }
            player->moveTimer = 0;
        }
    }

    if (dirty) {
        player->boardFlags |= GF_NEEDS_DRAW;
        *needsRedraw = TRUE;
    }
}

void vs_state_init() {
    vctx = &sctx->vs;

    memset(vctx, 0, sizeof(VsContext));
    vctx->rightAiEnabled = TRUE;
    vs_brain_reset(vctx);

    menu_bg_set_mode(BG_MODE_NONE);

    vs_reset_player(&vctx->left);
    vs_reset_player(&vctx->right);
}

void vs_state_init_draw() {
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    vsTileStart = TILE_USER_INDEX;
    gfx_load_tiles(vsTileStart);
    set_vs_palette();

    VDP_loadFont(&TS_FONT_CLEAR, CPU);
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    VDP_setTextPalette(PAL3);

    VDP_drawText("VERSUS", 17, 1);
    VDP_drawText("P1", VS_LEFT_X + 4, 2);
    VDP_drawText(vctx->rightAiEnabled ? "CPU" : "P2", VS_RIGHT_X + 4, 2);

    // Invalidate caches so first draw writes all tiles.
    memset(leftCache, 0xFF, sizeof(leftCache));
    memset(rightCache, 0xFF, sizeof(rightCache));
    debugOverlayTimer = 0;

    vctx->left.boardFlags  |= GF_NEEDS_DRAW;
    vctx->right.boardFlags |= GF_NEEDS_DRAW;
    vctx->leftNeedsRedraw = TRUE;
    vctx->rightNeedsRedraw = TRUE;
}

void vs_state_update() {
    if (vctx == NULL) return;

    vctx->joy1 = JOY_readJoypad(JOY_1);
    vctx->joy2 = JOY_readJoypad(JOY_2);

    // After game over: wait ~3 s then return to title.
    if (vctx->leftDead || vctx->rightDead) {
        vctx->gameOverTimer++;
        if (vctx->gameOverTimer > 180) {
            currentState = STATE_TITLE;
        }
        vctx->joy1Last = vctx->joy1;
        vctx->joy2Last = vctx->joy2;
        return;
    }

    vs_update_player(&vctx->left,  vctx->joy1, vctx->joy1Last, &vctx->leftDead, &vctx->leftNeedsRedraw);
    if (vctx->rightAiEnabled) {
        vs_brain_update_player(vctx, &vctx->right, &vctx->rightDead, &vctx->rightNeedsRedraw);
    } else {
        vs_update_player(&vctx->right, vctx->joy2, vctx->joy2Last, &vctx->rightDead, &vctx->rightNeedsRedraw);
    }

    // Force final redraw of both boards when someone just died.
    if (vctx->leftDead || vctx->rightDead) {
        vctx->left.boardFlags  |= GF_NEEDS_DRAW;
        vctx->right.boardFlags |= GF_NEEDS_DRAW;
        vctx->leftNeedsRedraw = TRUE;
        vctx->rightNeedsRedraw = TRUE;
    }

    vctx->joy1Last = vctx->joy1;
    vctx->joy2Last = vctx->joy2;
}

void vs_state_draw() {
    if (vctx == NULL) return;

    vs_draw_player_board(&vctx->left,  VS_LEFT_X,  VS_BOARD_Y, vctx->leftDead,  leftCache, &vctx->leftNeedsRedraw);
    vs_draw_player_board(&vctx->right, VS_RIGHT_X, VS_BOARD_Y, vctx->rightDead, rightCache, &vctx->rightNeedsRedraw);
    vs_draw_debug_overlay();

    // Draw result text exactly once (gameOverTimer==1 is the first frame after death).
    if (vctx->gameOverTimer == 1) {
        if (vctx->leftDead && vctx->rightDead) {
            VDP_drawText("  DRAW  ", 16, 13);
        } else if (vctx->leftDead) {
            VDP_drawText("P2 WINS!", 16, 13);
        } else {
            VDP_drawText("P1 WINS!", 16, 13);
        }
    }

}

void vs_state_cleanup() {
    vctx = NULL;
    VDP_clearPlane(BG_A, TRUE);
}