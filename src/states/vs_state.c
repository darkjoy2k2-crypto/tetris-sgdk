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
#include "sounds.h"
#include "sprite.h"
#include "text_manager.h"

#define VS_LEFT_X   5
#define VS_RIGHT_X 25
#define VS_BOARD_Y  4
#define VS_EVENT_Y   25
#define VS_EVENT_W   14
#define VS_PROMPT_X  2
#define VS_PROMPT_Y  26
#define VS_WINNER_NONE   0
#define VS_WINNER_LEFT   1
#define VS_WINNER_RIGHT  2
#define VS_WINNER_DRAW   3
#define VS_EXIT_NONE     0
#define VS_EXIT_RESTART  1
#define VS_EXIT_TITLE    2

typedef enum VsItemMode {
    VS_ITEM_MODE_NONE = 0,
    VS_ITEM_MODE_BOTH = 1,
    VS_ITEM_MODE_HEART_ONLY = 2,
    VS_ITEM_MODE_SKULL_ONLY = 3
} VsItemMode;

// Per-side VS item config.
#define VS_LEFT_ITEM_MODE  VS_ITEM_MODE_NONE
#define VS_RIGHT_ITEM_MODE VS_ITEM_MODE_BOTH

static VsContext* vctx = NULL;
static u16 vsTileStart = TILE_USER_INDEX;
static u16 vsSkullTileIdx = TILE_USER_INDEX;
static u16 vsHeartTileIdx = TILE_USER_INDEX;

// Per-player tile caches: only write to VDP when tile actually changes.
static u16 leftCache[200];
static u16 rightCache[200];
static u8 leftSortBuffer[200];
static u8 rightSortBuffer[200];
static u16 debugOverlayTimer = 0;
static char leftEventDrawCache[VS_EVENT_W + 1] = "";
static char rightEventDrawCache[VS_EVENT_W + 1] = "";


static void bind_player(GameContext* player, u16 joyNow, u16 joyPrev, GameContext** savedCtx, u16* savedJoy, u16* savedLastJoy);
static void unbind_player(GameContext* savedCtx, u16 savedJoy, u16 savedLastJoy);
static void vs_finish_line_clear(VsContext* vs, GameContext* player, bool isLeft);
static void vs_update_player_animations(VsContext* vs, GameContext* player, bool isLeft, bool* needsRedraw);
static bool vs_is_tspin_lock(const GameContext* player, bool lastMoveWasRotate);
static bool vs_is_perfect_clear(const GameContext* player);
static u16 vs_base_attack(u16 lines, bool tSpin);
static void vs_set_rotate_flag(bool isLeft, bool value);
static bool vs_get_rotate_flag(bool isLeft);
static u16 vs_get_item_mode(bool isLeft);
static u8* vs_get_sort_buffer(bool isLeft);
static void vs_prepare_sort_buffer(GameContext* player, u8* sortBuffer);
static void vs_trigger_multiclear(GameContext* player, bool isLeft);
static void vs_trigger_good_effect(VsContext* vs, GameContext* player, bool isLeft);
static void vs_trigger_bad_effect(VsContext* vs, GameContext* player, bool isLeft);
static bool vs_effect_slot_busy(const GameContext* player);
static void vs_tick_player_effects(GameContext* player, bool* needsRedraw);
static bool vs_update_board_effect_animation(VsContext* vs, GameContext* player, bool isLeft);
static void vs_handle_item_spawn(GameContext* player, u16 itemMode);
static u16 vs_get_gravity_threshold(GameContext* player, u16 joyNow, bool allowSoftDrop);
static void vs_set_event_text(bool isLeft, const char* text);
static void vs_draw_event_text(void);
static void vs_handle_match_end(void);
static void vs_finalize_attack(VsContext* vs, bool isLeft, u16 attack, u16 canceled, const char* eventName, bool b2b, bool perfectClear, u16 comboCount);
static void vs_sync_effect_sprites(void);

static s16 vs_board_x_for(bool isLeft) {
    return isLeft ? VS_LEFT_X : VS_RIGHT_X;
}

static u16 vs_get_item_mode(bool isLeft) {
    return isLeft ? vctx->leftItemMode : vctx->rightItemMode;
}

static u8* vs_get_sort_buffer(bool isLeft) {
    return isLeft ? leftSortBuffer : rightSortBuffer;
}

static void vs_prepare_sort_buffer(GameContext* player, u8* sortBuffer) {
    memcpy(sortBuffer, player->board, 200);

    for (s16 targetY = 19; targetY >= 0; targetY--) {
        u16 maxBlocksIndex = 0;
        u16 maxBlocksCount = 0;

        for (u16 currentY = 0; currentY <= (u16)targetY; currentY++) {
            u16 currentCount = 0;
            u16 rowOffset = (u16)((currentY << 3) + (currentY << 1));

            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                if (sortBuffer[rowOffset + x] != 0) currentCount++;
            }

            if (currentCount >= maxBlocksCount) {
                maxBlocksCount = currentCount;
                maxBlocksIndex = currentY;
            }
        }

        {
            u16 targetOffset = (u16)((targetY << 3) + (targetY << 1));
            u16 maxOffset = (u16)((maxBlocksIndex << 3) + (maxBlocksIndex << 1));

            if (targetOffset != maxOffset) {
                for (u16 x = 0; x < BOARD_WIDTH; x++) {
                    u8 temp = sortBuffer[targetOffset + x];
                    sortBuffer[targetOffset + x] = sortBuffer[maxOffset + x];
                    sortBuffer[maxOffset + x] = temp;
                }
            }
        }
    }
}

static void vs_trigger_multiclear(GameContext* player, bool isLeft) {
    u16 nonFullLines[BOARD_HEIGHT];
    u16 nonFullCount = 0;

    if (player == NULL || player->sortingRow != -1) return;

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        bool full = TRUE;
        u16 rowOffset = (u16)((y << 3) + (y << 1));

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (player->board[rowOffset + x] == 0) {
                full = FALSE;
                break;
            }
        }

        if (!full) nonFullLines[nonFullCount++] = y;
    }

    if (nonFullCount == 0) {
        if (player->activeBadEffect == EFFECT_MULTIPLIER) {
            player->activeBadEffect = EFFECT_NONE;
            player->badEffectTimer = 0;
            player->lastActiveBadEffect = 99;
        }
        return;
    }

    {
        u16 linesToClear = (u16)((random() % 4) + 1);
        if (linesToClear > nonFullCount) linesToClear = nonFullCount;

        player->clearingLineMask = 0;
        for (u16 i = 0; i < linesToClear; i++) {
            u16 randomIdx = (u16)(random() % nonFullCount);
            u16 y = nonFullLines[randomIdx];
            u16 rowOffset = (u16)((y << 3) + (y << 1));

            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                player->clearingLineBackup[rowOffset + x] = player->board[rowOffset + x];
            }

            player->clearingLineMask |= (1UL << y);
            nonFullLines[randomIdx] = nonFullLines[nonFullCount - 1];
            nonFullCount--;
        }
    }

    player->clearTimer = GET_TICKS(20);
    player->boardFlags |= GF_NEEDS_DRAW;
    SOUND_play(52);
    sprites_trigger_line_clear_explosions_at_origin(player->clearingLineMask, vs_board_x_for(isLeft), VS_BOARD_Y);
}

static bool vs_effect_slot_busy(const GameContext* player) {
    if (player == NULL) return TRUE;
    if (player->activeBadEffect != EFFECT_NONE) return TRUE;
    if (player->sortingRow >= 0) return TRUE;
    return FALSE;
}

static void vs_trigger_good_effect(VsContext* vs, GameContext* player, bool isLeft) {
    u16 roll;

    if (vs_effect_slot_busy(player)) return;

    roll = (u16)(random() % 5);
    switch (roll) {
        case 0:
            player->activeBadEffect = EFFECT_I_RAIN;
            player->badEffectTimer = DUR_I_RAIN_SPAWNS;
            vs_set_event_text(isLeft, "IRAIN");
            break;
        case 1:
            vs_prepare_sort_buffer(player, vs_get_sort_buffer(isLeft));
            player->sortingRow = 0;
            vs_set_event_text(isLeft, "SORT");
            SOUND_play(SND_TETRIS);
            break;
        case 2:
            player->activeBadEffect = EFFECT_RAINBOW;
            player->badEffectTimer = 0;
            player->sortingRow = 0;
            vs_set_event_text(isLeft, "RGB");
            break;
        case 3:
            player->activeBadEffect = EFFECT_FREEZE;
            player->badEffectTimer = DUR_FREEZE_TICKS;
            vs_set_event_text(isLeft, "ICE");
            break;
        default:
            player->activeBadEffect = EFFECT_MULTIPLIER;
            player->badEffectTimer = 1;
            vs_set_event_text(isLeft, "CLR");
            vs_trigger_multiclear(player, isLeft);
            break;
    }

    for (u16 i = 0; i < 200; i++) {
        if (player->board[i] == ITEM_ID_HEART) player->board[i] = (u8)(1 + (random() % 7));
    }

    player->boardFlags |= GF_NEEDS_DRAW;
    SOUND_play(SND_GOOD_ITEM);
    (void)vs;
}

static void vs_trigger_bad_effect(VsContext* vs, GameContext* player, bool isLeft) {
    u16 roll;
    bool playGenericBadSound = TRUE;

    if (vs_effect_slot_busy(player)) return;

    roll = (u16)(random() % 7);
    switch (roll) {
        case 0:
            player->activeBadEffect = EFFECT_NO_ROTATE;
            player->badEffectTimer = DUR_NO_ROTATE_TICKS;
            player->flags |= GF_ROT_LOCKED;
            vs_set_event_text(isLeft, "NOROT");
            break;
        case 1:
            player->activeBadEffect = EFFECT_REVERSED;
            player->badEffectTimer = DUR_REVERSED_TICKS;
            vs_set_event_text(isLeft, "SILLY");
            break;
        case 2:
            player->activeBadEffect = EFFECT_FULLSPEED;
            player->badEffectTimer = GET_TICKS(120) + DUR_FULLSPEED_SPAWNS;
            playGenericBadSound = FALSE;
            vs_set_event_text(isLeft, "FAST");
            break;
        case 3:
            player->activeBadEffect = EFFECT_SAME_TILES;
            player->badEffectTimer = DUR_SAME_TILES_SPAWNS;
            player->forcedPieceType = (s16)(random() % 7);
            vs_set_event_text(isLeft, "SAME");
            break;
        case 4:
            player->activeBadEffect = EFFECT_HOLD_LOCK;
            player->badEffectTimer = DUR_HOLD_LOCK_TICKS;
            vs_set_event_text(isLeft, "NOHLD");
            break;
        case 5:
            player->activeBadEffect = EFFECT_HIDE_NEXT;
            player->badEffectTimer = DUR_HIDE_NEXT_TICKS;
            vs_set_event_text(isLeft, "NONXT");
            break;
        default:
            player->activeBadEffect = EFFECT_SHADOW_BOARD;
            player->badEffectTimer = DUR_SHADOW_TICKS;
            player->sortingRow = 0;
            vs_set_event_text(isLeft, "FADE");
            break;
    }

    if (playGenericBadSound) SOUND_play(SND_BAD_ITEM);
    player->lastActiveBadEffect = 99;
    player->boardFlags |= GF_NEEDS_DRAW;
    (void)vs;
}

static void vs_tick_player_effects(GameContext* player, bool* needsRedraw) {
    if (player->badEffectTimer <= 0) return;

    if (player->activeBadEffect == EFFECT_FULLSPEED) {
        if (player->badEffectTimer > DUR_FULLSPEED_SPAWNS) {
            u16 warningTicks = (u16)(player->badEffectTimer - DUR_FULLSPEED_SPAWNS);
            if (warningTicks == GET_TICKS(120) || warningTicks == GET_TICKS(60)) {
                SOUND_play(SND_ALERT);
            }
            player->badEffectTimer--;
        }
        return;
    }

    if (player->activeBadEffect == EFFECT_RAINBOW ||
        player->activeBadEffect == EFFECT_SAME_TILES ||
        player->activeBadEffect == EFFECT_I_RAIN ||
        player->activeBadEffect == EFFECT_MULTIPLIER) {
        return;
    }

    player->badEffectTimer--;
    if (player->badEffectTimer > 0) return;

    switch (player->activeBadEffect) {
        case EFFECT_NO_ROTATE:
            player->flags &= ~GF_ROT_LOCKED;
            player->activeBadEffect = EFFECT_NONE;
            break;
        case EFFECT_SHADOW_BOARD:
            player->activeBadEffect = EFFECT_RAINBOW;
            player->sortingRow = 0;
            break;
        default:
            player->activeBadEffect = EFFECT_NONE;
            break;
    }

    if (player->activeBadEffect == EFFECT_NONE) {
        player->lastActiveBadEffect = 99;
    }

    player->boardFlags |= GF_NEEDS_DRAW;
    *needsRedraw = TRUE;
    SOUND_play(SND_GOOD_ITEM);
}

static bool vs_update_board_effect_animation(VsContext* vs, GameContext* player, bool isLeft) {
    if (player->activeBadEffect == EFFECT_RAINBOW) {
        if (player->sortingRow >= 0 && player->sortingRow < BOARD_HEIGHT) {
            u16 rowOffset = (u16)(((u16)player->sortingRow << 3) + ((u16)player->sortingRow << 1));
            u8 rowColor = (u8)((random() % 7) + 1);

            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                if (player->board[rowOffset + x] != 0) player->board[rowOffset + x] = rowColor;
            }

            player->boardFlags |= GF_NEEDS_DRAW;
            player->sortingRow++;
            if (player->sortingRow >= BOARD_HEIGHT) {
                player->sortingRow = -1;
                player->activeBadEffect = EFFECT_NONE;
                player->badEffectTimer = 0;
            }
        }
        return TRUE;
    }

    if (player->activeBadEffect == EFFECT_SHADOW_BOARD) {
        if (player->sortingRow >= 0 && player->sortingRow < BOARD_HEIGHT) {
            u16 rowOffset = (u16)(((u16)player->sortingRow << 3) + ((u16)player->sortingRow << 1));

            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                if (player->board[rowOffset + x] != 0) player->board[rowOffset + x] = TILE_ID_GARBAGE;
            }

            player->boardFlags |= GF_NEEDS_DRAW;
            player->sortingRow++;
            if (player->sortingRow >= BOARD_HEIGHT) player->sortingRow = -1;
        }
        return TRUE;
    }

    if (player->sortingRow >= 0 && player->sortingRow < BOARD_HEIGHT) {
        u8 tempRow[BOARD_WIDTH];
        u8* sortBuffer = vs_get_sort_buffer(isLeft);
        u16 rowOffset = (u16)(((u16)player->sortingRow << 3) + ((u16)player->sortingRow << 1));
        u16 filled = 0;
        GameContext* savedCtx;
        u16 savedJoy;
        u16 savedLastJoy;

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            player->board[rowOffset + x] = sortBuffer[rowOffset + x];
        }

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u8 tile = player->board[rowOffset + x];
            if (tile != 0) tempRow[filled++] = tile;
        }
        for (u16 x = 0; x < filled; x++) player->board[rowOffset + x] = tempRow[x];
        if (filled < BOARD_WIDTH) memset(&player->board[rowOffset + filled], 0, BOARD_WIDTH - filled);

        player->boardFlags |= GF_NEEDS_DRAW;
        player->sortingRow++;
        if (player->sortingRow >= BOARD_HEIGHT) {
            player->sortingRow = -1;
            bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);
            clearLinesAtOrigin(vs_board_x_for(isLeft), VS_BOARD_Y);
            unbind_player(savedCtx, savedJoy, savedLastJoy);
        }
        (void)vs;
        return TRUE;
    }

    (void)vs;
    return FALSE;
}

static void vs_handle_item_spawn(GameContext* player, u16 itemMode) {
    if (itemMode == 0) {
        player->itemSlot = 255;
        player->itemType = ITEM_ID_NONE;
        return;
    }

    if (player->itemSpawnCounter <= 0) {
        player->itemSlot = (u16)(random() % 4);
        if (itemMode == 1) player->itemType = (random() % 100 < ITEM_RATIO_HEART) ? ITEM_ID_HEART : ITEM_ID_SKULL;
        else player->itemType = (itemMode == 2) ? ITEM_ID_HEART : ITEM_ID_SKULL;
        player->itemSpawnCounter = (u16)((random() % 2) + 1);
    } else {
        player->itemSlot = 255;
        player->itemType = ITEM_ID_NONE;
        player->itemSpawnCounter--;
    }
}

static u16 vs_get_gravity_threshold(GameContext* player, u16 joyNow, bool allowSoftDrop) {
    u16 vBtnSoftDrop = (player->activeBadEffect == EFFECT_REVERSED) ? BUTTON_LEFT : BUTTON_DOWN;
    s16 threshold = (s16)GET_TICKS(48 - (player->level > 1 ? (player->level - 1) * 2 : 0));

    if (threshold < 2) threshold = 2;

    if (player->activeBadEffect == EFFECT_FULLSPEED) {
        if (player->badEffectTimer > 0 && player->badEffectTimer <= DUR_FULLSPEED_SPAWNS) {
            threshold = 4;
        }
    }

    if (allowSoftDrop && (joyNow & vBtnSoftDrop)) {
        threshold = (s16)config.thresholdSD;
    } else if (player->activeBadEffect == EFFECT_FREEZE) {
        threshold = 9999;
    }

    if (threshold < 1) threshold = 1;
    return (u16)threshold;
}

static void vs_sync_effect_sprites(void) {
    bool leftHasEffect = (!vctx->leftDead) && (
        vctx->left.activeBadEffect == EFFECT_NO_ROTATE ||
        vctx->left.activeBadEffect == EFFECT_REVERSED ||
        vctx->left.activeBadEffect == EFFECT_FULLSPEED);
    bool rightHasEffect = (!vctx->rightDead) && (
        vctx->right.activeBadEffect == EFFECT_NO_ROTATE ||
        vctx->right.activeBadEffect == EFFECT_REVERSED ||
        vctx->right.activeBadEffect == EFFECT_FULLSPEED);

    if (rightHasEffect) {
        sprites_sync_vs_effect(&vctx->right, VS_RIGHT_X, VS_BOARD_Y);
    } else if (leftHasEffect) {
        sprites_sync_vs_effect(&vctx->left, VS_LEFT_X, VS_BOARD_Y);
    } else {
        sprites_sync_vs_effect(NULL, 0, 0);
    }
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

static bool vs_check_spawn_collision_safe(const GameContext* player) {
    for (u16 i = 0; i < 4; i++) {
        s16 gx = player->pieceX + PIECES[player->type][player->rotation][i][0];
        s16 gy = player->pieceY + PIECES[player->type][player->rotation][i][1];

        if (gx < 0 || gx >= BOARD_WIDTH || gy >= BOARD_HEIGHT) return TRUE;

        if (gy >= 0 && player->board[(gy * BOARD_WIDTH) + gx] != 0) {
            if ((player->clearTimer > 0) && (player->clearingLineMask & (1UL << gy))) {
                continue;
            }
            return TRUE;
        }
    }

    return FALSE;
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

    if (text == NULL) {
        dst[0] = '\0';
        text_manager_clear_vs_status(isLeft);
        return;
    }

    strncpy(dst, text, 23);
    dst[23] = '\0';
    text_manager_set_vs_status(isLeft, dst);
}

static void vs_draw_event_text(void) {
    if (strcmp(leftEventDrawCache, "              ") != 0) {
        VDP_drawText("              ", VS_LEFT_X, VS_EVENT_Y);
        strcpy(leftEventDrawCache, "              ");
    }

    if (strcmp(rightEventDrawCache, "              ") != 0) {
        VDP_drawText("              ", VS_RIGHT_X, VS_EVENT_Y);
        strcpy(rightEventDrawCache, "              ");
    }
}

static void vs_finalize_attack(VsContext* vs, bool isLeft, u16 attack, u16 canceled, const char* eventName, bool b2b, bool perfectClear, u16 comboCount) {
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

    if ((eventName != NULL && eventName[0] != '\0') || (comboCount > 1)) {
        if (perfectClear) {
            strcpy(text, "PCLR");
        } else if (comboCount > 1) {
            if (comboCount > 99) comboCount = 99;
            sprintf(text, "X%u", comboCount);
        } else if (b2b) {
            strcpy(text, "B2B");
        } else {
            strncpy(text, eventName, 23);
            text[23] = '\0';
        }

        vs_set_event_text(isLeft, text);
    }
}

static void vs_handle_match_end(void) {
    if (vctx->matchOver) return;

    if (vctx->leftDead && vctx->rightDead) {
        vctx->matchOver = TRUE;
        vctx->winnerSide = VS_WINNER_DRAW;
        vctx->matchExitAction = VS_EXIT_NONE;
        vctx->matchPromptBlinkTimer = GET_TICKS(20);
        vctx->matchPromptVisible = TRUE;
        text_manager_clear_vs_status(TRUE);
        text_manager_clear_vs_status(FALSE);
        text_manager_init_vs_winner("DRAW");
        text_manager_glyphs_visible(TRUE);
        return;
    }

    if (vctx->rightDead && !vctx->leftDead) {
        vctx->matchOver = TRUE;
        vctx->winnerSide = VS_WINNER_LEFT;
        vctx->matchExitAction = VS_EXIT_NONE;
        vctx->matchPromptBlinkTimer = GET_TICKS(20);
        vctx->matchPromptVisible = TRUE;
        vctx->left.activeBadEffect = EFFECT_RAINBOW;
        vctx->left.sortingRow = 0;
        vctx->left.badEffectTimer = 0;
        vctx->left.boardFlags |= GF_NEEDS_DRAW;
        vctx->leftNeedsRedraw = TRUE;
        text_manager_clear_vs_status(TRUE);
        text_manager_clear_vs_status(FALSE);
        text_manager_init_vs_winner("1P WINS");
        text_manager_glyphs_visible(TRUE);
        return;
    }

    if (vctx->leftDead && !vctx->rightDead) {
        vctx->matchOver = TRUE;
        vctx->winnerSide = VS_WINNER_RIGHT;
        vctx->matchExitAction = VS_EXIT_NONE;
        vctx->matchPromptBlinkTimer = GET_TICKS(20);
        vctx->matchPromptVisible = TRUE;
        vctx->right.activeBadEffect = EFFECT_RAINBOW;
        vctx->right.sortingRow = 0;
        vctx->right.badEffectTimer = 0;
        vctx->right.boardFlags |= GF_NEEDS_DRAW;
        vctx->rightNeedsRedraw = TRUE;
        text_manager_clear_vs_status(TRUE);
        text_manager_clear_vs_status(FALSE);
        text_manager_init_vs_winner(vctx->rightAiEnabled ? "CPU WINS" : "2P WINS");
        text_manager_glyphs_visible(TRUE);
    }
}

static void vs_finish_line_clear(VsContext* vs, GameContext* player, bool isLeft) {
    u16 linesFound = 0;
    u16 totalHearts = 0;
    u16 totalSkulls = 0;
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
                u8 tile = player->clearingLineBackup[rowOffset + x];
                player->board[rowOffset + x] = tile;
                if (tile == ITEM_ID_HEART) totalHearts++;
                else if (tile == ITEM_ID_SKULL) totalSkulls++;
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
            if (linesFound == 1) eventName = "TS1";
            else if (linesFound == 2) eventName = "TS2";
            else if (linesFound == 3) eventName = "TS3";
            else eventName = "TSPN";
        } else if (linesFound == 4) {
            eventName = "TETR";
        } else if (linesFound == 3) {
            eventName = "TRI";
        } else if (linesFound == 2) {
            eventName = "DBL";
        } else {
            eventName = "";
        }

        vs_finalize_attack(vs, isLeft, attack, counter, eventName, b2bBonus, perfectClear, player->comboCount);

        if (totalHearts > totalSkulls) {
            vs_trigger_good_effect(vs, player, isLeft);
        } else if (totalSkulls > totalHearts) {
            GameContext* opponent = isLeft ? &vs->right : &vs->left;
            vs_trigger_bad_effect(vs, opponent, !isLeft);
        }
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

    unbind_player(savedCtx, savedJoy, savedLastJoy);

    if (vs_update_board_effect_animation(vs, player, isLeft)) {
        animated = TRUE;
    }

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
    bool isLeft;
    u16 itemMode;

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);

    if (player->activeBadEffect == EFFECT_FULLSPEED) {
        if (player->badEffectTimer > 0 && player->badEffectTimer <= DUR_FULLSPEED_SPAWNS) {
            player->badEffectTimer--;
            if (player->badEffectTimer <= 0) {
                player->activeBadEffect = EFFECT_NONE;
                player->badEffectTimer = 0;
                player->lastActiveBadEffect = 99;
                SOUND_play(SND_GOOD_ITEM);
            }
        }
    } else if (player->activeBadEffect == EFFECT_SAME_TILES || player->activeBadEffect == EFFECT_I_RAIN) {
        if (player->badEffectTimer > 0) {
            player->badEffectTimer--;
            if (player->badEffectTimer <= 0) {
                player->activeBadEffect = EFFECT_NONE;
                player->badEffectTimer = 0;
                player->lastActiveBadEffect = 99;
                SOUND_play(SND_GOOD_ITEM);
            }
        }
    }

    player->type = player->nextType;

    if (player->bagIndex >= 7) {
        refillBag();
    }
    player->nextType = player->bag[player->bagIndex++];

    if (player->activeBadEffect == EFFECT_SAME_TILES) player->type = (u16)player->forcedPieceType;
    if (player->activeBadEffect == EFFECT_I_RAIN) player->type = 0;

    player->rotation = 0;
    player->pieceX = 3;
    player->pieceY = (player->type == 0) ? -1 : 0;
    player->moveTimer = 0;

    isLeft = (vctx != NULL && player == &vctx->left);
    itemMode = vs_get_item_mode(isLeft);
    vs_handle_item_spawn(player, itemMode);

    calculate_ghost_y();
    player->boardFlags |= GF_NEEDS_DRAW;

    {
        bool blocked = vs_check_spawn_collision_safe(player);
        unbind_player(savedCtx, savedJoy, savedLastJoy);

        if (vctx != NULL) {
            if (player == &vctx->left) vctx->leftLastRotate = FALSE;
            else if (player == &vctx->right) vctx->rightLastRotate = FALSE;
        }

        return !blocked;
    }
}

static void vs_reset_player(GameContext* player, bool spawnInitialPiece) {
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
    player->forcedPieceType = -1;
    player->level = 1;
    player->startLevel = 1;
    player->flags = 0;
    player->boardFlags = GF_NEEDS_DRAW;
    player->dasNextThreshold = config.thresholdLRInitial;
    player->activeBadEffect = EFFECT_NONE;
    player->sortingRow = -1;
    player->itemSlot = 255;
    player->itemType = ITEM_ID_NONE;

    bind_player(player, 0, 0, &savedCtx, &savedJoy, &savedLastJoy);
    refillBag();
    player->nextType = player->bag[player->bagIndex++];
    unbind_player(savedCtx, savedJoy, savedLastJoy);

    if (spawnInitialPiece) {
        vs_spawn_piece_for_player(player);
    }
}

// Draws one player's board using a tile cache  VDP only written when tile changes.
// Returns early if boardFlags does not have GF_NEEDS_DRAW.
static void vs_draw_player_board(GameContext* player, u16 ox, u16 oy, bool isDead, u16* cache, bool* needsRedraw) {
    if (!(*needsRedraw)) return;

    view_draw_board_for_context(player, ox, oy, vsTileStart, vsSkullTileIdx, vsHeartTileIdx, cache, !isDead && !vctx->introActive, !isDead && !vctx->introActive && GET_FLAG(config.flags, FLAG_SHADOW));

    player->boardFlags &= ~GF_NEEDS_DRAW;
    *needsRedraw = FALSE;
}
bool vs_lock_piece_for_player(VsContext* vs, GameContext* player, bool isLeft, u8 lockPulseType, bool lastMoveWasRotate) {
    bool lockedAbove = FALSE;
    bool wasDropLock = (lockPulseType != 1);
    s16 boardX = vs_board_x_for(isLeft);
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;

    sprites_trigger_dust_at_board_origin(boardX, VS_BOARD_Y, player->pieceX, player->ghostY, wasDropLock);
    menu_bg_riistar_pulse(lockPulseType);

    for (u16 i = 0; i < 4; i++) {
        s16 gx = player->pieceX + PIECES[player->type][player->rotation][i][0];
        s16 gy = player->pieceY + PIECES[player->type][player->rotation][i][1];

        if (gy < 0) lockedAbove = TRUE;

        if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
            u8 cell = ((u16)i == player->itemSlot)
                ? (u8)player->itemType
                : (u8)(player->type + 1);
            player->board[gx + ((gy << 3) + (gy << 1))] = cell;
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

    vs_set_event_text(isLeft, "NO-STAT");

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
    u16 vBtnLeft = BUTTON_LEFT;
    u16 vBtnRight = BUTTON_RIGHT;
    u16 vBtnSoftDrop = BUTTON_DOWN;
    u16 vBtnHardDrop = BUTTON_UP;
    u16 vBtnRotCCW = BUTTON_A;
    u16 vBtnRotCW = BUTTON_B;
    bool dirty = FALSE;

    if (*deadFlag) return;
    if (vctx->matchOver) return;

    if (player->activeBadEffect == EFFECT_REVERSED) {
        vBtnLeft = BUTTON_B;
        vBtnRight = BUTTON_A;
        vBtnRotCW = BUTTON_UP;
        vBtnRotCCW = BUTTON_DOWN;
        vBtnHardDrop = BUTTON_RIGHT;
        vBtnSoftDrop = BUTTON_LEFT;
    }

    changed = joyNow & ~joyPrev;

    currentDir = (joyNow & vBtnLeft) ? vBtnLeft : ((joyNow & vBtnRight) ? vBtnRight : 0);
    if (currentDir != 0) {
        if (changed & currentDir) {
            s16 step = (currentDir == vBtnLeft) ? -1 : 1;
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
                s16 step = (currentDir == vBtnLeft) ? -1 : 1;
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

    if (changed & (vBtnRotCCW | vBtnRotCW)) {
        if (player->activeBadEffect == EFFECT_NO_ROTATE || (player->flags & GF_ROT_LOCKED)) {
            if (vs_try_step_down(player)) dirty = TRUE;
            SOUND_play(SND_BAD_ITEM);
        } else {
            u16 newRotation = (changed & vBtnRotCW) ? (u16)((player->rotation + 1) & 3) : (u16)((player->rotation + 3) & 3);
            if (vs_try_rotate(player, newRotation)) {
                dirty = TRUE;
                vs_set_rotate_flag(isLeft, TRUE);
                SOUND_play(SND_ROTATE);
            }
        }
    }

    if (changed & vBtnHardDrop) {
        SOUND_play(SND_HARD_DROP);
        while (vs_try_step_down(player)) { }
        dirty = TRUE;
        if (!vs_lock_piece_for_player(vs, player, isLeft, 3, vs_get_rotate_flag(isLeft)) || !vs_spawn_piece_for_player(player)) {
            *deadFlag = TRUE;
        }
    }

    player->moveTimer++;
    if (player->moveTimer >= vs_get_gravity_threshold(player, joyNow, TRUE)) {
        if (!vs_try_step_down(player)) {
            if (!vs_lock_piece_for_player(vs, player, isLeft, (joyNow & vBtnSoftDrop) ? 2 : 1, vs_get_rotate_flag(isLeft)) || !vs_spawn_piece_for_player(player)) {
                *deadFlag = TRUE;
            }
            dirty = TRUE;
        } else {
            dirty = TRUE;
            if (joyNow & vBtnSoftDrop) {
                player->score++;
                SOUND_play(SND_SOFT_DROP);
            }
        }
        player->moveTimer = 0;
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
    vctx->leftItemMode = VS_LEFT_ITEM_MODE;
    vctx->rightItemMode = VS_RIGHT_ITEM_MODE;
    vctx->leftGameOverAnimRow = -1;
    vctx->rightGameOverAnimRow = -1;
    vctx->winnerSide = VS_WINNER_NONE;
    vctx->matchExitAction = VS_EXIT_NONE;
    vctx->matchPromptBlinkTimer = GET_TICKS(20);
    vctx->matchPromptVisible = TRUE;
    vctx->introActive = TRUE;
    text_manager_clear_vs_status(TRUE);
    text_manager_clear_vs_status(FALSE);
    vs_brain_reset(vctx);

    menu_bg_set_mode_instant(BG_MODE_CLUB);

    vs_reset_player(&vctx->left, FALSE);
    vs_reset_player(&vctx->right, FALSE);
}

void vs_state_init_draw() {
    VDP_clearPlane(BG_A, TRUE);
    // BG_B ist exklusiv für menu_bg — nicht löschen! menu_bg_set_mode_instant verwaltet es.

    sprites_init();

    vsTileStart = TILE_USER_INDEX;
    gfx_load_tiles(vsTileStart);
    vsSkullTileIdx = (u16)(vsTileStart + 9);
    VDP_loadTileData(tile_skull, vsSkullTileIdx, 1, CPU);
    vsHeartTileIdx = (u16)(vsTileStart + 10);
    VDP_loadTileData(tile_heart, vsHeartTileIdx, 1, CPU);
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
    leftEventDrawCache[0] = '\0';
    rightEventDrawCache[0] = '\0';
    vctx->left.boardFlags  |= GF_NEEDS_DRAW;
    vctx->right.boardFlags |= GF_NEEDS_DRAW;
    vctx->leftNeedsRedraw = TRUE;
    vctx->rightNeedsRedraw = TRUE;

    text_manager_init_vs_countdown();
    text_manager_glyphs_visible(TRUE);
}

void vs_state_update() {
    if (vctx == NULL) return;

    bool prevLeftDead = vctx->leftDead;
    bool prevRightDead = vctx->rightDead;

    vctx->joy1 = JOY_readJoypad(JOY_1);
    vctx->joy2 = JOY_readJoypad(JOY_2);

    if (vctx->introActive) {
        text_manager_set_enabled(TRUE);
        text_manager_update();

        if (text_manager_is_finished()) {
            text_manager_glyphs_visible(FALSE);
            text_manager_cleanup();
            sprites_init();
            sprites_text_set_enabled(TRUE);
            text_manager_clear_vs_status(TRUE);
            text_manager_clear_vs_status(FALSE);
            set_vs_palette();

            if (!vs_spawn_piece_for_player(&vctx->left)) vctx->leftDead = TRUE;
            if (!vs_spawn_piece_for_player(&vctx->right)) vctx->rightDead = TRUE;

            vctx->introActive = FALSE;
            vctx->left.boardFlags  |= GF_NEEDS_DRAW;
            vctx->right.boardFlags |= GF_NEEDS_DRAW;
            vctx->leftNeedsRedraw = TRUE;
            vctx->rightNeedsRedraw = TRUE;

            if (vctx->leftDead || vctx->rightDead) {
                if (vctx->leftDead) vctx->leftGameOverAnimRow = 0;
                if (vctx->rightDead) vctx->rightGameOverAnimRow = 0;
                vs_handle_match_end();
            }
        }

        vctx->joy1Last = vctx->joy1;
        vctx->joy2Last = vctx->joy2;
        return;
    }

    // --- MATCH OVER LOGIK ---
    if (vctx->matchOver) {
        if (vctx->matchExitAction == VS_EXIT_NONE) {
            if (vctx->matchPromptBlinkTimer > 0) {
                vctx->matchPromptBlinkTimer--;
            } else {
                vctx->matchPromptBlinkTimer = GET_TICKS(20);
                vctx->matchPromptVisible = !vctx->matchPromptVisible;
            }

            if ((vctx->joy1 & BUTTON_START) && !(vctx->joy1Last & BUTTON_START)) {
                vctx->matchExitAction = VS_EXIT_RESTART;
                vctx->matchPromptVisible = FALSE;
                text_manager_request_exit();
            } else if ((vctx->joy1 & BUTTON_C) && !(vctx->joy1Last & BUTTON_C)) {
                vctx->matchExitAction = VS_EXIT_TITLE;
                vctx->matchPromptVisible = FALSE;
                text_manager_request_exit();
            }
        }

        if (vctx->winnerSide == VS_WINNER_LEFT) {
            vs_update_player_animations(vctx, &vctx->left, TRUE, &vctx->leftNeedsRedraw);
        } else if (vctx->winnerSide == VS_WINNER_RIGHT) {
            vs_update_player_animations(vctx, &vctx->right, FALSE, &vctx->rightNeedsRedraw);
        }

        if (vctx->leftDead) vs_step_game_over_animation(&vctx->left, VS_LEFT_X, &vctx->leftGameOverAnimRow, &vctx->leftNeedsRedraw);
        if (vctx->rightDead) vs_step_game_over_animation(&vctx->right, VS_RIGHT_X, &vctx->rightGameOverAnimRow, &vctx->rightNeedsRedraw);

        vs_sync_effect_sprites();
        text_manager_set_enabled(TRUE);
        text_manager_update();

        if (vctx->matchExitAction != VS_EXIT_NONE && text_manager_is_finished()) {
            u16 action = vctx->matchExitAction;

            text_manager_glyphs_visible(FALSE);
            text_manager_cleanup();

            if (action == VS_EXIT_RESTART) {
                vs_state_init();
                vs_state_init_draw();
            } else {
                currentState = STATE_TITLE;
            }

            vctx->joy1Last = vctx->joy1;
            vctx->joy2Last = vctx->joy2;
            return;
        }

        vctx->joy1Last = vctx->joy1;
        vctx->joy2Last = vctx->joy2;
        return;
    }

    if (!vctx->leftDead) vs_tick_player_effects(&vctx->left, &vctx->leftNeedsRedraw);
    if (!vctx->rightDead) vs_tick_player_effects(&vctx->right, &vctx->rightNeedsRedraw);

    // --- ANIMATIONS-UPDATES ---
    if (!vctx->leftDead)  vs_update_player_animations(vctx, &vctx->left, TRUE, &vctx->leftNeedsRedraw);
    if (!vctx->rightDead) vs_update_player_animations(vctx, &vctx->right, FALSE, &vctx->rightNeedsRedraw);

    // --- PLAYER 1 (LINKS) LOGIK ---
    if (!vctx->leftDead && vctx->left.clearTimer == 0) {
        vs_update_player(vctx, &vctx->left, vctx->joy1, vctx->joy1Last, &vctx->leftDead, &vctx->leftNeedsRedraw, TRUE);
    }

    // --- PLAYER 2 / CPU (RECHTS) LOGIK ---
    if (!vctx->rightDead && vctx->right.clearTimer == 0) {
        if (vctx->rightAiEnabled) {
            // KI simuliert nur Eingaben (X und Rotation)
            vs_brain_update_player(vctx, &vctx->right, &vctx->rightDead, &vctx->rightNeedsRedraw);

            // Zentrale Schwerkraft-Steuerung für CPU
            // Re-check clearTimer after brain update to avoid gravity/lock in the same frame
            // in which a lock just started a line-clear animation.
            if (!vctx->rightDead && vctx->right.clearTimer == 0) {
                vctx->right.moveTimer++;
                if (vctx->right.moveTimer >= vs_get_gravity_threshold(&vctx->right, 0, FALSE)) {
                    if (!vs_try_step_down(&vctx->right)) {
                        if (!vs_lock_piece_for_player(vctx, &vctx->right, FALSE, 1, vctx->rightLastRotate) || 
                            !vs_spawn_piece_for_player(&vctx->right)) {
                            vctx->rightDead = TRUE;
                        }
                    }
                    vctx->right.moveTimer = 0;
                    vctx->rightNeedsRedraw = TRUE;
                }
            }
        } else {
            // Manueller Modus P2
            vs_update_player(vctx, &vctx->right, vctx->joy2, vctx->joy2Last, &vctx->rightDead, &vctx->rightNeedsRedraw, FALSE);
        }
    }

    // --- GARBAGE PROCESSING ---
    if (!vctx->leftDead && vctx->rightGarbagePending > 0 && vctx->left.clearTimer == 0 && (vctx->left.boardFlags & GF_PENDING_MASK) == 0) {
        if (!addGarbageLineForContext(&vctx->left)) vctx->leftDead = TRUE;
        vctx->rightGarbagePending--;
        vctx->leftNeedsRedraw = TRUE;
    }

    if (!vctx->rightDead && vctx->leftGarbagePending > 0 && vctx->right.clearTimer == 0 && (vctx->right.boardFlags & GF_PENDING_MASK) == 0) {
        if (!addGarbageLineForContext(&vctx->right)) vctx->rightDead = TRUE;
        vctx->leftGarbagePending--;
        vctx->rightNeedsRedraw = TRUE;
    }

    // --- DEATH & MATCH END CHECKS ---
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

    if (vctx->leftDead)  vs_step_game_over_animation(&vctx->left, VS_LEFT_X, &vctx->leftGameOverAnimRow, &vctx->leftNeedsRedraw);
    if (vctx->rightDead) vs_step_game_over_animation(&vctx->right, VS_RIGHT_X, &vctx->rightGameOverAnimRow, &vctx->rightNeedsRedraw);

    text_manager_update_vs_statuses();
    vs_sync_effect_sprites();
    sprites_update();

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
    vs_draw_event_text();

    if (GET_FLAG(config.flags, FLAG_DEBUG)) {
        vs_draw_debug_overlay();
    }

    if (vctx->matchOver) {
        if (vctx->matchExitAction == VS_EXIT_NONE && vctx->matchPromptVisible) {
            VDP_drawText("START: TRY AGAIN - C: RETURN TO MENU", VS_PROMPT_X, VS_PROMPT_Y);
        } else {
            VDP_drawText("                                    ", VS_PROMPT_X, VS_PROMPT_Y);
        }
    }

}

void vs_state_cleanup() {
    text_manager_glyphs_visible(FALSE);
    text_manager_cleanup();
    vctx = NULL;
    VDP_clearPlane(BG_A, TRUE);
}