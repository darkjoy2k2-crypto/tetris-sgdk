#include <genesis.h>
#include <string.h>

#include "states/vs/brain.h"
#include "states/vs_state.h"
#include "states/game/game_core.h"
#include "states/game/game_logic.h"

// --- KONFIGURATION ---
#define AI_BASE_STRATEGY  4  
#define AI_BASE_REACTION  4  
#define PANIC_HEIGHT      8 
#define AI_SCORE_MIN      -2000000L
#define TUCK_PENALTY      30000 

typedef struct { u16 rows[20]; } BitBoard;

// --- HELFER-FUNKTIONEN ---

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

static u16 ai_get_human_delay(VsContext* vctx, GameContext* player) {
    u16 maxH = 0;
    for (s16 x = 0; x < 10; x++) {
        for (s16 y = 0; y < 20; y++) {
            if (player->board[(y << 3) + (y << 1) + x]) { 
                u16 h = (u16)(20 - y); if (h > maxH) maxH = h; break; 
            }
        }
    }

    u16 effLevel = AI_BASE_REACTION + (player->level >> 1) + (maxH >> 2);
    if (effLevel > 10) effLevel = 10;
    if (effLevel < 1)  effLevel = 1;

    // Neue Skalierung: 120 Ticks bei Level 1, 3 Ticks bei Level 10
    // Steigung m = (3 - 120) / (10 - 1) = -117 / 9 = -13
    u16 base = (u16)(120 - ((effLevel - 1) * 13));
    
    u16 var = (base >> 2) + 1;
    s16 jitter = (s16)((random() % (var << 1)) - var);
    s16 final = (s16)base + jitter;
    
    return (final < 2) ? 2 : (u16)final;
}

static s32 ai_evaluate(VsContext* vctx, const BitBoard* bb, u16 lines) {
    s32 score = 0;
    u16 aggH = 0, holes = 0, bump = 0, maxH = 0, colH[10] = {0};
    u16 effStrat = AI_BASE_STRATEGY;

    for (s16 x = 0; x < 10; x++) {
        u16 m = (0x8000 >> x); bool b = FALSE;
        for (s16 y = 0; y < 20; y++) {
            if (bb->rows[y] & m) { 
                if (!b) { colH[x] = (u16)(20 - y); if (colH[x] > maxH) maxH = colH[x]; b = TRUE; } 
            } else if (b) holes++;
        }
        aggH += colH[x];
    }

    if (vctx->rightGarbagePending > 0 || maxH >= PANIC_HEIGHT) {
        effStrat = 1; if (lines > 0) score += (s32)40000;
    }

    for (s16 x = 0; x < 9; x++) {
        s16 d = (s16)(colH[x] - colH[x+1]);
        u16 absD = (d < 0) ? (u16)-d : (u16)d;
        bump += absD;
        if (absD > 2) score -= (s32)absD << 10; 
    }

    if (lines > 0) {
        if (lines >= effStrat) score += (s32)(lines << 14);
        else score -= (s32)(20000 >> (effStrat - lines)); 
    }

    if (effStrat > 1) {
        if (colH[8] > colH[9]) score += (s32)(colH[8] - colH[9]) << 9;
        if (colH[9] > 0) score -= 10000;
    } else if (colH[9] < colH[8]) score -= 5000;

    score -= (s32)aggH << 6;   
    score -= (s32)holes << 12; 
    score -= (s32)bump << 8;    
    
    return score;
}

void vs_brain_think_split(VsContext* vctx, GameContext* player) {
    static BitBoard bb;
    if (vctx->rightAiState == 0) {
        vctx->rightAiThinkBudget = 0;
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
            s32 s = ai_evaluate(vctx, &tb, ai_bit_clear_lines(&tb));
            if (s > vctx->rightAiBestScore) {
                vctx->rightAiBestScore = s; vctx->rightAiTargetX = x;
                vctx->rightAiTargetY = y; vctx->rightAiTargetRot = r; vctx->rightAiEntryX = x;
            }

            for (s16 slide = -1; slide <= 1; slide++) {
                if (slide == 0) continue;
                vctx->rightAiThinkBudget++;
                s16 tx = x + slide; s16 ty = y;
                if (ai_bit_collide(&bb, player->type, r, tx, ty)) continue;
                while (!ai_bit_collide(&bb, player->type, r, tx, (s16)(ty + 1))) ty++;

                if (ty > y) {
                    BitBoard tbt = bb;
                    for (u16 i = 0; i < 4; i++) {
                        s16 bx = tx + PIECES[player->type][r][i][0], by = ty + PIECES[player->type][r][i][1];
                        if (by >= 0 && by < 20) tbt.rows[by] |= (0x8000 >> bx);
                    }
                    s32 st = ai_evaluate(vctx, &tbt, ai_bit_clear_lines(&tbt)) - TUCK_PENALTY;
                    if (st > vctx->rightAiBestScore) {
                        vctx->rightAiBestScore = st; vctx->rightAiTargetX = tx;
                        vctx->rightAiTargetY = ty; vctx->rightAiTargetRot = r; vctx->rightAiEntryX = x;
                    }
                }
            }
        }
        vctx->rightAiState++;
    }
    if (vctx->rightAiState >= 5) vctx->rightAiPlannedType = (s16)player->type;
}

void vs_brain_update_player(VsContext* vctx, GameContext* player, bool* deadFlag, bool* needsRedraw) {
    if (*deadFlag || vctx->matchOver) return;
    vs_brain_think_split(vctx, player);
    if (vctx->rightAiPulseTimer > 0) { vctx->rightAiPulseTimer--; return; }

    if (vctx->rightAiState >= 5) {
        GameContext* savedCtx = ctx; ctx = player;
        bool dirty = FALSE;
        u16 targetRot = vctx->rightAiTargetRot & 3;
        u16 baseDelay = ai_get_human_delay(vctx, player);

        if ((player->rotation & 3) != targetRot) {
            if (tryRotate(targetRot)) { dirty = TRUE; vctx->rightLastRotate = TRUE; vctx->rightAiPulseTimer = baseDelay; }
        } else if (player->pieceX != vctx->rightAiEntryX) {
            s16 step = (player->pieceX < vctx->rightAiEntryX) ? 1 : -1;
            if (!checkCollision(player->pieceX + step, player->pieceY, player->rotation)) {
                player->pieceX += step; dirty = TRUE; vctx->rightAiPulseTimer = (baseDelay >> 1) + 1;
            }
        } else {
            if (player->pieceX == vctx->rightAiTargetX) {
                player->pieceY = player->ghostY; ctx = savedCtx;
                if (!vs_lock_piece_for_player(vctx, player, FALSE, TRUE, vctx->rightLastRotate) || !vs_spawn_piece_for_player(player)) { *deadFlag = TRUE; }
                vctx->rightAiState = 0; vctx->rightAiPlannedType = -1;
                player->moveTimer = 0; *needsRedraw = TRUE; return;
            } else {
                if (player->pieceY < vctx->rightAiTargetY) {
                    if (!checkCollision(player->pieceX, player->pieceY + 1, player->rotation)) {
                        player->pieceY++; dirty = TRUE; vctx->rightAiPulseTimer = 1; 
                    } else { player->pieceX = vctx->rightAiTargetX; }
                } else {
                    s16 step = (player->pieceX < vctx->rightAiTargetX) ? 1 : -1;
                    if (!checkCollision(player->pieceX + step, player->pieceY, player->rotation)) {
                        player->pieceX += step; dirty = TRUE; vctx->rightAiPulseTimer = (baseDelay >> 1) + 1;
                    }
                }
            }
        }
        if (dirty) { calculate_ghost_y(); player->boardFlags |= GF_NEEDS_DRAW; *needsRedraw = TRUE; }
        ctx = savedCtx;
    }
}

void vs_brain_reset(VsContext* vctx) {
    vctx->rightAiBestScore = AI_SCORE_MIN; vctx->rightAiPlannedType = -1;
    vctx->rightAiState = 0; vctx->rightAiPulseTimer = 0;
    vctx->rightAiThinkBudget = 0; vctx->rightAiEntryX = 0; vctx->rightAiTargetY = 0;
}