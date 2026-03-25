#include <genesis.h>
#include <string.h>

#include "states/vs/brain.h"
#include "states/vs_state.h"
#include "states/game/game_core.h"
#include "states/game/game_logic.h"

// --- KONFIGURATION ---
#define AI_STRATEGY       3  
#define AI_REACTION_LEVEL 7  
#define PANIC_HEIGHT      7 
#define AI_SCORE_MIN      -2000000L

typedef struct { u16 rows[20]; } BitBoard;

// --- DYNAMISCHE LATENZ-BERECHNUNG ---

static u16 ai_get_base_delay(GameContext* player) {
    u16 level = AI_REACTION_LEVEL;
    if (level < 1) level = 1; if (level > 10) level = 10;
    
    u16 base = (u16)(60 - ((level - 1) * 57) / 9);
    
    // Stress-Faktor (Max-Höhe ermitteln)
    u16 maxH = 0;
    for (s16 x = 0; x < 10; x++) {
        u16 h = 0; u16 m = (0x8000 >> x);
        for (s16 y = 0; y < 20; y++) {
            if (player->board[(y << 3) + (y << 1) + x]) { h = (u16)(20 - y); break; }
        }
        if (h > maxH) maxH = h;
    }

    u16 var = (base >> 2) + 1;
    if (maxH > 10) var += (maxH - 10);
    s16 jitter = (s16)((random() % (var << 1)) - var);
    
    s16 final = (s16)base + jitter;
    return (final < 2) ? 2 : (u16)final;
}

// --- SIMULATION & HEURISTIK ---

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
        if ((bb->rows[y] & 0xFFC0) == 0xFFC0) { 
            cleared++;
            for (s16 yy = y; yy > 0; yy--) bb->rows[yy] = bb->rows[yy - 1];
            bb->rows[0] = 0; y++; 
        }
    }
    return cleared;
}

static s32 ai_evaluate(const BitBoard* bb, u16 lines) {
    s32 score = 0;
    u16 aggH = 0, holes = 0, bump = 0, maxH = 0, colH[10] = {0};
    u16 effectiveStrategy = AI_STRATEGY;

    for (s16 x = 0; x < 10; x++) {
        u16 m = (0x8000 >> x); bool b = FALSE;
        for (s16 y = 0; y < 20; y++) {
            if (bb->rows[y] & m) { if (!b) { colH[x] = (u16)(20 - y); if (colH[x] > maxH) maxH = colH[x]; b = TRUE; } }
            else if (b) holes++;
        }
        aggH += colH[x];
    }

    if (maxH >= PANIC_HEIGHT) effectiveStrategy = 1;
    for (s16 x = 0; x < 9; x++) {
        s16 d = (s16)(colH[x] - colH[x+1]);
        bump += (d < 0) ? (s16)-d : d;
    }

    if (lines > 0) {
        if (lines >= effectiveStrategy) score += (s32)(lines << 12);
        else score -= (s32)(4000 >> (effectiveStrategy - lines));
    }

    if (effectiveStrategy > 1) {
        if (colH[8] > colH[9]) {
            u16 depth = colH[8] - colH[9];
            if (depth <= effectiveStrategy) score += (s32)depth << 9;
            else score -= 1500;
        }
        if (colH[9] > 0) score -= 5000;
    } else if (colH[9] < colH[8]) score -= 2000;

    score -= (s32)aggH << 3; score -= (s32)holes << 8; score -= (s32)bump << 4;   
    return score;
}

// --- CORE ENGINE ---

void vs_brain_think_split(VsContext* vctx, GameContext* player) {
    static BitBoard bb;
    if (vctx->rightAiState == 0) {
        for (u16 y = 0; y < 20; y++) {
            bb.rows[y] = 0;
            for (u16 x = 0; x < 10; x++)
                if (player->board[(y << 3) + (y << 1) + x]) bb.rows[y] |= (0x8000 >> x);
        }
        vctx->rightAiBestScore = AI_SCORE_MIN; vctx->rightAiState = 1; return;
    }
    u16 r = (vctx->rightAiState - 1);
    if (r < 4) {
        s16 minX = 100, maxX = -100;
        for (u16 i = 0; i < 4; i++) {
            if (PIECES[player->type][r][i][0] < minX) minX = PIECES[player->type][r][i][0];
            if (PIECES[player->type][r][i][0] > maxX) maxX = PIECES[player->type][r][i][0];
        }
        for (s16 x = (s16)-minX; x <= (s16)(9 - maxX); x++) {
            if (ai_bit_collide(&bb, player->type, r, x, 0)) continue;
            s16 y = 0; while (!ai_bit_collide(&bb, player->type, r, x, (s16)(y + 1))) y++;
            BitBoard tb = bb;
            for (u16 i = 0; i < 4; i++) {
                s16 bx = x + PIECES[player->type][r][i][0], by = y + PIECES[player->type][r][i][1];
                if (by >= 0 && by < 20) tb.rows[by] |= (0x8000 >> bx);
            }
            s32 s = ai_evaluate(&tb, ai_bit_clear_lines(&tb));
            if (s > vctx->rightAiBestScore) {
                vctx->rightAiBestScore = s; vctx->rightAiTargetX = x; vctx->rightAiTargetRot = r;
            }
        }
        vctx->rightAiState++;
    }
    if (vctx->rightAiState >= 5) vctx->rightAiPlannedType = (s16)player->type;
}

void vs_brain_update_player(VsContext* vctx, GameContext* player, bool* deadFlag, bool* needsRedraw) {
    if (*deadFlag || vctx->matchOver) return;

    vs_brain_think_split(vctx, player);

    if (vctx->rightAiPulseTimer > 0) {
        vctx->rightAiPulseTimer--;
        return;
    }

    if (vctx->rightAiState >= 5) {
        GameContext* savedCtx = ctx; ctx = player;
        bool dirty = FALSE;
        u16 targetRot = vctx->rightAiTargetRot & 3;
        u16 baseDelay = ai_get_base_delay(player);

        // 1. ROTATION (Standard Delay vor der Aktion)
        if ((player->rotation & 3) != targetRot) {
            if (tryRotate(targetRot)) { 
                dirty = TRUE; vctx->rightLastRotate = TRUE; 
                vctx->rightAiPulseTimer = baseDelay; // Normale Verzögerung
            }
        } 
        // 2. BEWEGUNG (Schneller als Rotation)
        else if (player->pieceX != vctx->rightAiTargetX) {
            s16 step = (player->pieceX < vctx->rightAiTargetX) ? 1 : -1;
            if (!checkCollision(player->pieceX + step, player->pieceY, player->rotation)) {
                player->pieceX += step; dirty = TRUE; vctx->rightLastRotate = FALSE;
                // Bewegung ist 50% schneller als Rotation
                vctx->rightAiPulseTimer = (baseDelay >> 1) + 1; 
            }
        } 
        // 3. HARD DROP (Leichte Verzögerung bevor es knallt)
        else if (player->pieceX == vctx->rightAiTargetX && (player->rotation & 3) == targetRot) {
            player->pieceY = player->ghostY;
            ctx = savedCtx;
            if (!vs_lock_piece_for_player(vctx, player, FALSE, TRUE, vctx->rightLastRotate) || 
                !vs_spawn_piece_for_player(player)) {
                *deadFlag = TRUE;
            }
            vctx->rightAiState = 0; vctx->rightAiPlannedType = -1;
            player->moveTimer = 0;
            *needsRedraw = TRUE;
            return;
        }

        if (dirty) {
            calculate_ghost_y();
            player->boardFlags |= GF_NEEDS_DRAW;
            *needsRedraw = TRUE;
        }
        ctx = savedCtx;
    }
}

void vs_brain_reset(VsContext* vctx) {
    vctx->rightAiBestScore = AI_SCORE_MIN;
    vctx->rightAiPlannedType = -1;
    vctx->rightAiState = 0;
    vctx->rightAiPulseTimer = 0;
}