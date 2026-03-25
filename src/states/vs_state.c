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
#include "sprite.h"

#define VS_LEFT_X   5
#define VS_RIGHT_X 25
#define VS_BOARD_Y  4
#define VS_EVENT_Y  25
#define VS_EVENT_W  14
#define VS_WINNER_NONE  0
#define VS_WINNER_LEFT  1
#define VS_WINNER_RIGHT 2
#define VS_WINNER_DRAW  3

static VsContext* vctx = NULL;
static u16 vsTileStart = TILE_USER_INDEX;

// Per-player tile caches: only write to VDP when tile actually changes.
static u16 leftCache[200];
static u16 rightCache[200];
static u16 debugOverlayTimer = 0;

static void bind_player(GameContext* player, u16 joyNow, u16 joyPrev, GameContext** savedCtx, u16* savedJoy, u16* savedLastJoy);
static void unbind_player(GameContext* savedCtx, u16 savedJoy, u16 savedLastJoy);
static void vs_finish_line_clear(VsContext* vs, GameContext* player, bool isLeft);
static void vs_update_player_animations(VsContext* vs, GameContext* player, bool isLeft, bool* needsRedraw);
static bool vs_is_tspin_lock(const GameContext* player, bool lastMoveWasRotate);
static bool vs_is_perfect_clear(const GameContext* player);
static u16 vs_base_attack(u16 lines, bool tSpin);
static void vs_set_rotate_flag(bool isLeft, bool value);
static bool vs_get_rotate_flag(bool isLeft);
static void vs_set_event_text(bool isLeft, const char* text);
static void vs_update_event_timers(void);
static void vs_draw_event_text(void);
static void vs_handle_match_end(void);
static void vs_finalize_attack(VsContext* vs, bool isLeft, u16 attack, u16 canceled, const char* eventName, bool b2b, bool perfectClear);

static s16 vs_board_x_for(bool isLeft) {
    return isLeft ? VS_LEFT_X : VS_RIGHT_X;
}

static void vs_step_game_over_animation(GameContext* player, s16 boardOriginX, s16* animRow, bool* needsRedraw) {
    if (*animRow < 0 || *animRow >= BOARD_HEIGHT) return;

    {
        u16 y = (u16)(*animRow);
        bool rowChanged = FALSE;
        u16 explodeDiv = (u16)((random() % 3) + 2);

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 idx = (u16)((y * BOARD_WIDTH) + x);
            if (player->board[idx] != 0) {
                rowChanged = TRUE;
                player->board[idx] = TILE_ID_GARBAGE;
                if ((random() % explodeDiv) == 0) {
                    sprites_trigger_explosion_at_board_cell_at_origin(x, y, 2, boardOriginX, VS_BOARD_Y);
                }
            }
        }

        if (rowChanged) {
            player->boardFlags |= GF_NEEDS_DRAW;
            *needsRedraw = TRUE;
        }
    }

    (*animRow)++;
    if (*animRow >= BOARD_HEIGHT) {
        *animRow = -1;
    }
}

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

static void vs_set_rotate_flag(bool isLeft, bool value) {
    if (isLeft) vctx->leftLastRotate = value;
    else vctx->rightLastRotate = value;
}

static bool vs_get_rotate_flag(bool isLeft) {
    return isLeft ? vctx->leftLastRotate : vctx->rightLastRotate;
}

static bool vs_is_tspin_lock(const GameContext* player, bool lastMoveWasRotate) {
    s16 cx;
    s16 cy;
    u16 corners = 0;
    const s16 checks[4][2] = {
        {-1, -1},
        { 1, -1},
        {-1,  1},
        { 1,  1}
    };

    if (player == NULL) return FALSE;
    if (player->type != 2 || !lastMoveWasRotate) return FALSE;

    cx = (s16)(player->pieceX + 1);
    cy = (s16)(player->pieceY + 1);

    for (u16 i = 0; i < 4; i++) {
        s16 x = (s16)(cx + checks[i][0]);
        s16 y = (s16)(cy + checks[i][1]);

        if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) {
            corners++;
        } else if (player->board[(y * BOARD_WIDTH) + x] != 0) {
            corners++;
        }
    }

    return (corners >= 3);
}

static bool vs_is_perfect_clear(const GameContext* player) {
    if (player == NULL) return FALSE;

    for (u16 i = 0; i < 200; i++) {
        if (player->board[i] != 0) return FALSE;
    }

    return TRUE;
}

static u16 vs_base_attack(u16 lines, bool tSpin) {
    if (tSpin) {
        switch (lines) {
            case 1: return 2;
            case 2: return 4;
            case 3: return 6;
            default: return 0;
        }
    }

    switch (lines) {
        case 2: return 1;
        case 3: return 2;
        case 4: return 4;
        default: return 0;
    }
}

static void vs_set_event_text(bool isLeft, const char* text) {
    char* dst = isLeft ? vctx->leftEventText : vctx->rightEventText;
    u16* timer = isLeft ? &vctx->leftEventTimer : &vctx->rightEventTimer;

    if (text == NULL) {
        dst[0] = '\0';
        *timer = 0;
        return;
    }

    strncpy(dst, text, 23);
    dst[23] = '\0';
    *timer = GET_TICKS(150);
}

static void vs_update_event_timers(void) {
    if (vctx->leftEventTimer > 0) {
        vctx->leftEventTimer--;
        if (vctx->leftEventTimer == 0) vctx->leftEventText[0] = '\0';
    }

    if (vctx->rightEventTimer > 0) {
        vctx->rightEventTimer--;
        if (vctx->rightEventTimer == 0) vctx->rightEventText[0] = '\0';
    }
}

static void vs_draw_event_text(void) {
    char line[VS_EVENT_W + 1];

    memset(line, ' ', VS_EVENT_W);
    line[VS_EVENT_W] = '\0';
    if (vctx->leftEventText[0] != '\0') {
        strncpy(line, vctx->leftEventText, VS_EVENT_W);
    }
    VDP_drawText(line, VS_LEFT_X, VS_EVENT_Y);

    memset(line, ' ', VS_EVENT_W);
    line[VS_EVENT_W] = '\0';
    if (vctx->rightEventText[0] != '\0') {
        strncpy(line, vctx->rightEventText, VS_EVENT_W);
    }
    VDP_drawText(line, VS_RIGHT_X, VS_EVENT_Y);
}

static void vs_finalize_attack(VsContext* vs, bool isLeft, u16 attack, u16 canceled, const char* eventName, bool b2b, bool perfectClear) {
    u16* incoming;
    u16* outgoing;
    u16 sent;
    char text[24];

    if (isLeft) {
        incoming = &vs->rightGarbagePending;
        outgoing = &vs->leftGarbagePending;
    } else {
        incoming = &vs->leftGarbagePending;
        outgoing = &vs->rightGarbagePending;
    }

    if (canceled > *incoming) canceled = *incoming;
    *incoming = (u16)(*incoming - canceled);

    sent = (attack > canceled) ? (u16)(attack - canceled) : 0;
    if ((u32)(*outgoing) + sent > 0xFFFF) *outgoing = 0xFFFF;
    else *outgoing = (u16)(*outgoing + sent);

    if (eventName != NULL && eventName[0] != '\0') {
        if (perfectClear) {
            sprintf(text, "PC %s +%u", eventName, sent);
        } else if (b2b && canceled > 0) {
            sprintf(text, "B2B %s C%u", eventName, canceled);
        } else if (b2b) {
            sprintf(text, "B2B %s +%u", eventName, sent);
        } else if (canceled > 0) {
            sprintf(text, "%s C%u +%u", eventName, canceled, sent);
        } else {
            sprintf(text, "%s +%u", eventName, sent);
        }

        vs_set_event_text(isLeft, text);
    }
}

static void vs_handle_match_end(void) {
    if (vctx->matchOver) return;

    if (vctx->leftDead && vctx->rightDead) {
        vctx->matchOver = TRUE;
        vctx->winnerSide = VS_WINNER_DRAW;
        vs_set_event_text(TRUE, "DRAW");
        vs_set_event_text(FALSE, "DRAW");
        return;
    }

    if (vctx->rightDead && !vctx->leftDead) {
        vctx->matchOver = TRUE;
        vctx->winnerSide = VS_WINNER_LEFT;
        vctx->left.activeBadEffect = EFFECT_RAINBOW;
        vctx->left.sortingRow = 0;
        vctx->left.badEffectTimer = 0;
        vctx->left.boardFlags |= GF_NEEDS_DRAW;
        vctx->leftNeedsRedraw = TRUE;
        vs_set_event_text(TRUE, "WINNER");
        return;
    }

    if (vctx->leftDead && !vctx->rightDead) {
        vctx->matchOver = TRUE;
        vctx->winnerSide = VS_WINNER_RIGHT;
        vctx->right.activeBadEffect = EFFECT_RAINBOW;
        vctx->right.sortingRow = 0;
        vctx->right.badEffectTimer = 0;
        vctx->right.boardFlags |= GF_NEEDS_DRAW;
        vctx->rightNeedsRedraw = TRUE;
        vs_set_event_text(FALSE, "WINNER");
    }
}

static void vs_finish_line_clear(VsContext* vs, GameContext* player, bool isLeft) {
    u16 linesFound = 0;
    bool tSpin = FALSE;
    bool difficult = FALSE;
    bool b2bBonus = FALSE;
    bool perfectClear = FALSE;
    u16 comboBonus = 0;
    u16 attack = 0;
    u16 counter = 0;
    const char* eventName = "";

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        if ((player->clearingLineMask & (1UL << y)) != 0) {
            u16 rowOffset = (u16)((y << 3) + (y << 1));
            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                player->board[rowOffset + x] = player->clearingLineBackup[rowOffset + x];
            }
        }
    }

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        if ((player->clearingLineMask & (1UL << y)) != 0) {
            u16 rowOffset = (u16)((y << 3) + (y << 1));
            linesFound++;
            player->boardFlags |= (1UL << (y + GF_PENDING_SHIFT));
            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                player->board[rowOffset + x] = 0;
            }
        }
    }

    if (linesFound > 0) {
        tSpin = vs_is_tspin_lock(player, vs_get_rotate_flag(isLeft));

        player->comboCount++;
        player->linesTotal = (u16)(player->linesTotal + linesFound);
        player->score += (u32)(linesFound * 100);
        player->level = (u16)(1 + (player->linesTotal / 10));

        attack = vs_base_attack(linesFound, tSpin);
        difficult = ((linesFound == 4) || (tSpin && linesFound > 0));

        if (difficult) {
            if ((player->flags & GF_B2B_ACTIVE) != 0) {
                attack++;
                b2bBonus = TRUE;
            }
            player->flags |= GF_B2B_ACTIVE;
        } else {
            player->flags &= ~GF_B2B_ACTIVE;
        }

        if (player->comboCount > 1) {
            comboBonus = (u16)(player->comboCount - 1);
            attack = (u16)(attack + comboBonus);
        }

        perfectClear = vs_is_perfect_clear(player);
        if (perfectClear) {
            attack = (u16)(attack + 10);
        }

        counter = isLeft ? vctx->rightGarbagePending : vctx->leftGarbagePending;

        if (tSpin) {
            if (linesFound == 1) eventName = "T-SPIN S";
            else if (linesFound == 2) eventName = "T-SPIN D";
            else if (linesFound == 3) eventName = "T-SPIN T";
            else eventName = "T-SPIN";
        } else if (linesFound == 4) {
            eventName = "TETRIS";
        } else if (linesFound == 3) {
            eventName = "TRIPLE";
        } else if (linesFound == 2) {
            eventName = "DOUBLE";
        } else {
            eventName = "SINGLE";
        }

        vs_finalize_attack(vs, isLeft, attack, counter, eventName, b2bBonus, perfectClear);
    } else {
        player->comboCount = 0;
    }

    vs_set_rotate_flag(isLeft, FALSE);
    player->clearingLineMask = 0;
    player->boardFlags |= GF_NEEDS_DRAW;
}

static void vs_update_player_animations(VsContext* vs, GameContext* player, bool isLeft, bool* needsRedraw) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;
    bool animated = FALSE;

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);

    if (player->clearTimer > 0) {
        update_blinking_animation();
        player->clearTimer--;
        if (player->clearTimer == 0) {
            vs_finish_line_clear(vs, player, isLeft);
        }
        player->boardFlags |= GF_NEEDS_DRAW;
        animated = TRUE;
    }

    if ((player->boardFlags & GF_PENDING_MASK) != 0) {
        handle_board_collapse();
        player->boardFlags |= GF_NEEDS_DRAW;
        animated = TRUE;
    }

    if (player->activeBadEffect == EFFECT_RAINBOW || player->activeBadEffect == EFFECT_SHADOW_BOARD || player->sortingRow >= 0) {
        update_board_animations();
        animated = TRUE;
    }

    unbind_player(savedCtx, savedJoy, savedLastJoy);

    if (animated || ((player->boardFlags & GF_NEEDS_DRAW) != 0)) {
        *needsRedraw = TRUE;
    }
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

bool vs_spawn_piece_for_player(GameContext* player) {
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
    player->itemSlot = 255;
    player->itemType = ITEM_ID_NONE;
    player->moveTimer = 0;

    calculate_ghost_y();
    player->boardFlags |= GF_NEEDS_DRAW;

    {
        bool blocked = checkCollision(player->pieceX, player->pieceY, player->rotation);
        unbind_player(savedCtx, savedJoy, savedLastJoy);

        if (vctx != NULL) {
            if (player == &vctx->left) vctx->leftLastRotate = FALSE;
            else if (player == &vctx->right) vctx->rightLastRotate = FALSE;
        }

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
    player->activeBadEffect = EFFECT_NONE;
    player->sortingRow = -1;

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);
    refillBag();
    player->nextType = player->bag[player->bagIndex++];
    unbind_player(savedCtx, savedJoy, savedLastJoy);

    vs_spawn_piece_for_player(player);
}

// Draws one player's board using a tile cache  VDP only written when tile changes.
// Returns early if boardFlags does not have GF_NEEDS_DRAW.
static void vs_draw_player_board(GameContext* player, u16 ox, u16 oy, bool isDead, u16* cache, bool* needsRedraw) {
    if (!(*needsRedraw)) return;

    view_draw_board_for_context(player, ox, oy, vsTileStart, (u16)(vsTileStart + 8), (u16)(vsTileStart + 8), cache, !isDead, !isDead && GET_FLAG(config.flags, FLAG_SHADOW));

    player->boardFlags &= ~GF_NEEDS_DRAW;
    *needsRedraw = FALSE;
}
bool vs_lock_piece_for_player(VsContext* vs, GameContext* player, bool isLeft, bool wasDropLock, bool lastMoveWasRotate) {
    bool lockedAbove = FALSE;
    s16 boardX = vs_board_x_for(isLeft);
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;

    sprites_trigger_dust_at_board_origin(boardX, VS_BOARD_Y, player->pieceX, player->ghostY, wasDropLock);

    for (u16 i = 0; i < 4; i++) {
        s16 gx = player->pieceX + PIECES[player->type][player->rotation][i][0];
        s16 gy = player->pieceY + PIECES[player->type][player->rotation][i][1];

        if (gy < 0) lockedAbove = TRUE;

        if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
            player->board[gx + ((gy << 3) + (gy << 1))] = (u8)(player->type + 1);
        }
    }

    SOUND_play(SND_PIECE_LOCK);

    if (lockedAbove) {
        return FALSE;
    }

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);
    vs_set_rotate_flag(isLeft, lastMoveWasRotate);
    clearLinesAtOrigin(boardX, VS_BOARD_Y);
    unbind_player(savedCtx, savedJoy, savedLastJoy);

    player->boardFlags |= GF_NEEDS_DRAW;
    return TRUE;
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

static void vs_update_player(VsContext* vs, GameContext* player, u16 joyNow, u16 joyPrev, bool* deadFlag, bool* needsRedraw, bool isLeft) {
    u16 changed;
    u16 currentDir;
    bool dirty = FALSE;

    if (*deadFlag) return;
    if (vctx->matchOver) return;

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
                vs_set_rotate_flag(isLeft, FALSE);
                SOUND_play(SND_MOVE);
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
                    vs_set_rotate_flag(isLeft, FALSE);
                    SOUND_play(SND_MOVE);
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
            vs_set_rotate_flag(isLeft, TRUE);
            SOUND_play(SND_ROTATE);
        }
    }
    if (changed & BUTTON_B) {
        if (vs_try_rotate(player, (u16)((player->rotation + 1) & 3))) {
            dirty = TRUE;
            vs_set_rotate_flag(isLeft, TRUE);
            SOUND_play(SND_ROTATE);
        }
    }

    // Up = hard drop
    if (changed & BUTTON_UP) {
        SOUND_play(SND_HARD_DROP);
        while (vs_try_step_down(player)) { }
        dirty = TRUE;
        if (!vs_lock_piece_for_player(vs, player, isLeft, TRUE, vs_get_rotate_flag(isLeft)) || !vs_spawn_piece_for_player(player)) {
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
                if (!vs_lock_piece_for_player(vs, player, isLeft, (joyNow & BUTTON_DOWN) != 0, vs_get_rotate_flag(isLeft)) || !vs_spawn_piece_for_player(player)) {
                    *deadFlag = TRUE;
                }
                dirty = TRUE;
            } else {
                dirty = TRUE;
                if (joyNow & BUTTON_DOWN) {
                    player->score++;
                    SOUND_play(SND_SOFT_DROP);
                }
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
    vctx->leftGameOverAnimRow = -1;
    vctx->rightGameOverAnimRow = -1;
    vctx->winnerSide = VS_WINNER_NONE;
    vs_brain_reset(vctx);

    menu_bg_set_mode(BG_MODE_NONE);

    vs_reset_player(&vctx->left);
    vs_reset_player(&vctx->right);
}

void vs_state_init_draw() {
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    sprites_init();

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

    {
        bool prevLeftDead = vctx->leftDead;
        bool prevRightDead = vctx->rightDead;

        vctx->joy1 = JOY_readJoypad(JOY_1);
        vctx->joy2 = JOY_readJoypad(JOY_2);

        if (vctx->matchOver) {
            if ((vctx->joy1 & BUTTON_START) && !(vctx->joy1Last & BUTTON_START)) {
                currentState = STATE_TITLE;
            }

            if (vctx->winnerSide == VS_WINNER_LEFT) {
                vs_update_player_animations(vctx, &vctx->left, TRUE, &vctx->leftNeedsRedraw);
            } else if (vctx->winnerSide == VS_WINNER_RIGHT) {
                vs_update_player_animations(vctx, &vctx->right, FALSE, &vctx->rightNeedsRedraw);
            }

            if (vctx->leftDead) {
                vs_step_game_over_animation(&vctx->left, VS_LEFT_X, &vctx->leftGameOverAnimRow, &vctx->leftNeedsRedraw);
            }
            if (vctx->rightDead) {
                vs_step_game_over_animation(&vctx->right, VS_RIGHT_X, &vctx->rightGameOverAnimRow, &vctx->rightNeedsRedraw);
            }

            vs_update_event_timers();
            sprites_update();

            vctx->joy1Last = vctx->joy1;
            vctx->joy2Last = vctx->joy2;
            return;
        }

        if (!vctx->leftDead) {
            vs_update_player_animations(vctx, &vctx->left, TRUE, &vctx->leftNeedsRedraw);
        }

        if (!vctx->rightDead) {
            vs_update_player_animations(vctx, &vctx->right, FALSE, &vctx->rightNeedsRedraw);
        }

        if (!vctx->leftDead && vctx->left.clearTimer == 0) {
            vs_update_player(vctx, &vctx->left, vctx->joy1, vctx->joy1Last, &vctx->leftDead, &vctx->leftNeedsRedraw, TRUE);
        }

        if (!vctx->rightDead && vctx->right.clearTimer == 0) {
            if (vctx->rightAiEnabled) {
                vs_brain_update_player(vctx, &vctx->right, &vctx->rightDead, &vctx->rightNeedsRedraw);
            } else {
                vs_update_player(vctx, &vctx->right, vctx->joy2, vctx->joy2Last, &vctx->rightDead, &vctx->rightNeedsRedraw, FALSE);
            }
        }

        if (!vctx->leftDead && vctx->rightGarbagePending > 0 && vctx->left.clearTimer == 0 && (vctx->left.boardFlags & GF_PENDING_MASK) == 0) {
            if (!addGarbageLineForContext(&vctx->left)) {
                vctx->leftDead = TRUE;
            }
            vctx->rightGarbagePending--;
            vctx->leftNeedsRedraw = TRUE;
        }

        if (!vctx->rightDead && vctx->leftGarbagePending > 0 && vctx->right.clearTimer == 0 && (vctx->right.boardFlags & GF_PENDING_MASK) == 0) {
            if (!addGarbageLineForContext(&vctx->right)) {
                vctx->rightDead = TRUE;
            }
            vctx->leftGarbagePending--;
            vctx->rightNeedsRedraw = TRUE;
        }

        if (!prevLeftDead && vctx->leftDead) {
            SOUND_play(SND_GAME_OVER);
            vctx->leftGameOverAnimRow = 0;
            vs_handle_match_end();
        }

        if (!prevRightDead && vctx->rightDead) {
            SOUND_play(SND_GAME_OVER);
            vctx->rightGameOverAnimRow = 0;
            vs_handle_match_end();
        }

        if (vctx->leftDead) {
            vs_step_game_over_animation(&vctx->left, VS_LEFT_X, &vctx->leftGameOverAnimRow, &vctx->leftNeedsRedraw);
        }
        if (vctx->rightDead) {
            vs_step_game_over_animation(&vctx->right, VS_RIGHT_X, &vctx->rightGameOverAnimRow, &vctx->rightNeedsRedraw);
        }

        sprites_update();
        vs_update_event_timers();

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
}

void vs_state_draw() {
    if (vctx == NULL) return;

    vs_draw_player_board(&vctx->left,  VS_LEFT_X,  VS_BOARD_Y, vctx->leftDead,  leftCache, &vctx->leftNeedsRedraw);
    vs_draw_player_board(&vctx->right, VS_RIGHT_X, VS_BOARD_Y, vctx->rightDead, rightCache, &vctx->rightNeedsRedraw);
    vs_draw_event_text();
    vs_draw_debug_overlay();

    if (vctx->matchOver) {
        if (vctx->winnerSide == VS_WINNER_DRAW) {
            VDP_drawText("  DRAW  ", 16, 13);
        } else if (vctx->winnerSide == VS_WINNER_RIGHT) {
            VDP_drawText(vctx->rightAiEnabled ? "CPU WINS" : "P2 WINS!", 16, 13);
        } else if (vctx->winnerSide == VS_WINNER_LEFT) {
            VDP_drawText("P1 WINS!", 16, 13);
        }
        VDP_drawText("START: EXIT", 14, 15);
    }

}

void vs_state_cleanup() {
    vctx = NULL;
    sprites_cleanup();
    VDP_clearPlane(BG_A, TRUE);
}