#include <genesis.h>
#include <string.h>

#include "states/vs/brain.h"
#include "states/vs_state.h"
#include "states/game/game_core.h"
#include "states/game/game_logic.h"

#define AI_SCORE_MIN -2000000L

typedef struct { u16 rows[20]; } BitBoard;

// --- INTERNE SIMULATION (REINE BIT-LOGIK) ---

static bool ai_bit_collide(const BitBoard* bb, u16 type, u16 r, s16 x, s16 y) {
    for (u16 i = 0; i < 4; i++) {
        s16 px = x + PIECES[type][r & 3][i][0];
        s16 py = y + PIECES[type][r & 3][i][1];
        if (px < 0 || px >= 10 || py >= 20) return TRUE;
        if (py >= 0 && (bb->rows[py] & (0x8000 >> px))) return TRUE;
    }
    return FALSE;
}

static u16 ai_bit_clear_lines(BitBoard* bb) {
    u16 cleared = 0;
    for (s16 y = 19; y >= 0; y--) {
        if ((bb->rows[y] & 0xFFC0) == 0xFFC0) { // Check 10 Spalten
            cleared++;
            for (s16 yy = y; yy > 0; yy--) bb->rows[yy] = bb->rows[yy - 1];
            bb->rows[0] = 0;
            y++; 
        }
    }
    return cleared;
}

static s32 ai_evaluate(const BitBoard* bb, u16 lines) {
    s32 score = 0;
    u16 aggH = 0, holes = 0, bump = 0, colH[10] = {0};

    for (s16 x = 0; x < 10; x++) {
        u16 mask = (0x8000 >> x);
        bool blockFound = FALSE;
        for (s16 y = 0; y < 20; y++) {
            if (bb->rows[y] & mask) {
                if (!blockFound) { colH[x] = (u16)(20 - y); blockFound = TRUE; }
            } else if (blockFound) {
                holes++; 
            }
        }
        aggH += colH[x];
    }

    for (s16 x = 0; x < 9; x++) {
        s16 d = (s16)(colH[x] - colH[x+1]);
        bump += (d < 0) ? (s16)-d : d;
    }

    // Gewichtung für aggressives VS-Spiel
    score += (s32)lines * 1500;      
    score -= (s32)aggH * 15;         
    score -= (s32)holes * 250;       
    score -= (s32)bump * 10;         
    return score;
}

// --- KI CORE ---

void vs_brain_think(VsContext* vctx, GameContext* player) {
    BitBoard bb;
    for (u16 y = 0; y < 20; y++) {
        bb.rows[y] = 0;
        for (u16 x = 0; x < 10; x++)
            if (player->board[y * 10 + x]) bb.rows[y] |= (0x8000 >> x);
    }

    vctx->rightAiBestScore = AI_SCORE_MIN;

    for (u16 r = 0; r < 4; r++) {
        // Grenzen berechnen
        s16 minX = 100, maxX = -100;
        for (u16 i = 0; i < 4; i++) {
            s16 px = PIECES[player->type][r][i][0];
            if (px < minX) minX = px;
            if (px > maxX) maxX = px;
        }
        s16 startX = (s16)-minX;
        s16 endX = (s16)(9 - maxX);

        for (s16 x = startX; x <= endX; x++) {
            if (ai_bit_collide(&bb, player->type, r, x, 0)) continue;

            s16 y = 0;
            while (!ai_bit_collide(&bb, player->type, r, x, (s16)(y + 1))) y++;

            BitBoard tb = bb;
            for (u16 i = 0; i < 4; i++) {
                s16 bx = (s16)(x + PIECES[player->type][r][i][0]);
                s16 by = (s16)(y + PIECES[player->type][r][i][1]);
                if (by >= 0 && by < 20) tb.rows[by] |= (0x8000 >> bx);
            }

            u16 cleared = ai_bit_clear_lines(&tb);
            s32 s = ai_evaluate(&tb, cleared);

            if (s > vctx->rightAiBestScore) {
                vctx->rightAiBestScore = s;
                vctx->rightAiTargetX = x;
                vctx->rightAiTargetRot = r;
            }
        }
    }
    vctx->rightAiPlannedType = (s16)player->type;
    vctx->rightAiPlanReady = TRUE;
}

void vs_brain_update_player(VsContext* vctx, GameContext* player, bool* deadFlag, bool* needsRedraw) {
    if (*deadFlag || vctx->matchOver) return;

    if (vctx->rightAiPlannedType != (s16)player->type) {
        vs_brain_think(vctx, player);
    }

    // Kontext-Swap für game_logic Funktionen
    GameContext* savedCtx = ctx;
    ctx = player;

    bool dirty = FALSE;
    u16 targetRot = vctx->rightAiTargetRot & 3;

    if ((player->rotation & 3) != targetRot) {
        if (tryRotate(targetRot)) { 
            dirty = TRUE; 
            vctx->rightLastRotate = TRUE; 
        }
    } else if (player->pieceX != vctx->rightAiTargetX) {
        s16 step = (player->pieceX < vctx->rightAiTargetX) ? 1 : -1;
        if (!checkCollision(player->pieceX + step, player->pieceY, player->rotation)) {
            player->pieceX += step;
            dirty = TRUE;
            vctx->rightLastRotate = FALSE;
        }
    } else {
        if (!checkCollision(player->pieceX, (s16)(player->pieceY + 1), player->rotation)) {
            player->pieceY++;
            dirty = TRUE;
        } else {
            // Kontext zurücksetzen vor Lock/Spawn, da diese Funktionen im vs_state.c selbst binden
            ctx = savedCtx;
            if (!vs_lock_piece_for_player(vctx, player, FALSE, FALSE, vctx->rightLastRotate) || 
                !vs_spawn_piece_for_player(player)) {
                *deadFlag = TRUE;
            }
            *needsRedraw = TRUE;
            return;
        }
    }

    if (dirty) {
        calculate_ghost_y();
        player->boardFlags |= GF_NEEDS_DRAW;
        *needsRedraw = TRUE;
    }

    ctx = savedCtx;
}

void vs_brain_reset(VsContext* vctx) {
    vctx->rightAiBestScore = AI_SCORE_MIN;
    vctx->rightAiPlannedType = -1;
    vctx->rightAiPlanReady = FALSE;
}