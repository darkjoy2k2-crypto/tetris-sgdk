#include <genesis.h>
#include <string.h>

#include "states/vs/brain.h"
#include "states/game/game_core.h"
#include "states/game/game_logic.h"

#define AI_STATE_SCAN      0
#define AI_STATE_LOOKAHEAD 1
#define AI_STATE_EXECUTE   2
#define AI_SMARTNESS_LEVEL 9
#define AI_DECISIVENESS_LEVEL 2

#define AI_SCORE_MIN     (-2000000000L)
#define AI_NEXT_LOOKAHEAD_WEIGHT (30 + (AI_SMARTNESS_LEVEL * 4))
#define AI_TOP_CANDIDATES ((AI_SMARTNESS_LEVEL >= 8) ? 4 : ((AI_SMARTNESS_LEVEL >= 4) ? 3 : 2))
#define AI_CPU_LOAD_TARGET 88
#define AI_CPU_LOAD_HARD_LIMIT 92
#define AI_THINK_STEPS_MAX ((AI_SMARTNESS_LEVEL >= 8) ? 3 : ((AI_SMARTNESS_LEVEL >= 4) ? 2 : 1))
#define AI_SCAN_TIME_BUDGET (8 + (AI_SMARTNESS_LEVEL * 3))
#define AI_LOOKAHEAD_TIME_BUDGET (4 + (AI_SMARTNESS_LEVEL * 2))
#define AI_SURVIVAL_HEIGHT_THRESHOLD ((BOARD_HEIGHT * 8) / 10)
#define AI_SAFE_AVG_HEIGHT_THRESHOLD (BOARD_HEIGHT / 2)

static s16 aiPrevX = 0;
static s16 aiPrevY = 0;
static u16 aiPrevRot = 0;
static u16 aiStallFrames = 0;

typedef struct AiTopCandidate {
    bool valid;
    bool done;
    bool useSoftDrop;
    s16 x;
    s16 y;
    u16 rot;
    s32 baseScore;
    s32 nextBestScore;
    s32 totalScore;
    u16 nextScanRot;
    s16 nextScanX;
    u8 boardAfter[200];
} AiTopCandidate;

static AiTopCandidate aiTopCandidates[AI_TOP_CANDIDATES];
static u16 aiLookaheadCursor = 0;

static s16 ai_min_x_for(u16 type, u16 rotation);
static s16 ai_max_x_for(u16 type, u16 rotation);
static u16 ai_get_descent_tightness(const u8* board, u16 type, u16 rotation, s16 x, s16 startY, s16 targetY);
static bool ai_lock_and_respawn(VsContext* vctx, GameContext* player);

static void ai_reset_top_candidates(void) {
    for (u16 i = 0; i < AI_TOP_CANDIDATES; i++) {
        aiTopCandidates[i].valid = FALSE;
        aiTopCandidates[i].done = TRUE;
        aiTopCandidates[i].useSoftDrop = FALSE;
        aiTopCandidates[i].x = 0;
        aiTopCandidates[i].y = 0;
        aiTopCandidates[i].rot = 0;
        aiTopCandidates[i].baseScore = AI_SCORE_MIN;
        aiTopCandidates[i].nextBestScore = AI_SCORE_MIN;
        aiTopCandidates[i].totalScore = AI_SCORE_MIN;
        aiTopCandidates[i].nextScanRot = 0;
        aiTopCandidates[i].nextScanX = 0;
    }
    aiLookaheadCursor = 0;
}

static s16 ai_find_worst_candidate_slot(void) {
    s16 worstIdx = 0;
    s32 worstScore = 2147483000L;

    for (u16 i = 0; i < AI_TOP_CANDIDATES; i++) {
        if (!aiTopCandidates[i].valid) return (s16)i;
        if (aiTopCandidates[i].baseScore < worstScore) {
            worstScore = aiTopCandidates[i].baseScore;
            worstIdx = (s16)i;
        }
    }

    return worstIdx;
}

static void ai_try_store_top_candidate(s16 x, s16 y, u16 rot, bool useSoftDrop, s32 baseScore, const u8* boardAfter) {
    s16 slot = ai_find_worst_candidate_slot();

    if (aiTopCandidates[slot].valid && baseScore <= aiTopCandidates[slot].baseScore) {
        return;
    }

    aiTopCandidates[slot].valid = TRUE;
    aiTopCandidates[slot].done = FALSE;
    aiTopCandidates[slot].useSoftDrop = useSoftDrop;
    aiTopCandidates[slot].x = x;
    aiTopCandidates[slot].y = y;
    aiTopCandidates[slot].rot = rot;
    aiTopCandidates[slot].baseScore = baseScore;
    aiTopCandidates[slot].nextBestScore = AI_SCORE_MIN;
    aiTopCandidates[slot].totalScore = baseScore;
    aiTopCandidates[slot].nextScanRot = 0;
    aiTopCandidates[slot].nextScanX = 0;
    memcpy(aiTopCandidates[slot].boardAfter, boardAfter, 200);
}

static void ai_bind_player(GameContext* player, GameContext** savedCtx, u16* savedJoy, u16* savedLastJoy) {
    *savedCtx = ctx;
    *savedJoy = joyState;
    *savedLastJoy = lastJoyState;
    ctx = player;
    joyState = 0;
    lastJoyState = 0;
}

static void ai_unbind_player(GameContext* savedCtx, u16 savedJoy, u16 savedLastJoy) {
    ctx = savedCtx;
    joyState = savedJoy;
    lastJoyState = savedLastJoy;
}

static s16 ai_abs_s16(s16 v) {
    return (v < 0) ? (s16)-v : v;
}

static u16 ai_smartness_level(void) {
    if (AI_SMARTNESS_LEVEL < 1) return 1;
    if (AI_SMARTNESS_LEVEL > 10) return 10;
    return AI_SMARTNESS_LEVEL;
}

static u16 ai_decisiveness_level(void) {
    if (AI_DECISIVENESS_LEVEL < 1) return 1;
    if (AI_DECISIVENESS_LEVEL > 10) return 10;
    return AI_DECISIVENESS_LEVEL;
}

static u16 ai_execution_actions_per_frame(void) {
    /* Keep movement readable and fair: max one action per frame. */
    return 1;
}

static u16 ai_scale_think_timer(u16 baseTimer) {
    u16 decisiveness = ai_decisiveness_level();
    u16 scaled;

    /* High decisiveness plans faster: level 1 => 100%, level 10 => 40%. */
    scaled = (u16)((baseTimer * (14 - decisiveness)) / 13);
    if (scaled < 1) scaled = 1;
    return scaled;
}

static bool ai_is_lateral_gap_fill(const u8* board, u16 type, u16 rotation, s16 x, s16 targetY) {
    u16 tightness = ai_get_descent_tightness(board, type, rotation, x, -2, targetY);
    s16 lateralDist = ai_abs_s16((s16)(x - 3));

    return (lateralDist >= 2) && (targetY >= 10) && (tightness >= 18);
}

static bool ai_is_better_candidate(s32 score, s16 x, u16 rot, s32 bestScore, s16 bestX, u16 bestRot, s16 spawnX) {
    if (score > bestScore) return TRUE;
    if (score < bestScore) return FALSE;

    {
        s16 distNew = ai_abs_s16((s16)(x - spawnX));
        s16 distBest = ai_abs_s16((s16)(bestX - spawnX));
        if (distNew < distBest) return TRUE;
        if (distNew > distBest) return FALSE;
    }

    {
        s16 center = BOARD_WIDTH / 2;
        s16 centerNew = ai_abs_s16((s16)(x - center));
        s16 centerBest = ai_abs_s16((s16)(bestX - center));
        if (centerNew < centerBest) return TRUE;
        if (centerNew > centerBest) return FALSE;
    }

    return (rot < bestRot);
}

static bool ai_collides(const u8* board, u16 type, u16 rotation, s16 x, s16 y) {
    for (u16 i = 0; i < 4; i++) {
        s16 px = x + PIECES[type][rotation][i][0];
        s16 py = y + PIECES[type][rotation][i][1];

        if (px < 0 || px >= BOARD_WIDTH || py >= BOARD_HEIGHT) return TRUE;
        if (py >= 0 && board[(py * BOARD_WIDTH) + px] != 0) return TRUE;
    }
    return FALSE;
}

static u16 ai_get_descent_tightness(const u8* board, u16 type, u16 rotation, s16 x, s16 startY, s16 targetY) {
    u16 tightness = 0;

    for (s16 y = startY; y <= targetY; y++) {
        for (u16 i = 0; i < 4; i++) {
            s16 px = x + PIECES[type][rotation][i][0];
            s16 py = y + PIECES[type][rotation][i][1];

            if (py < 0 || py >= BOARD_HEIGHT) continue;

            if (px <= 0 || board[(py * BOARD_WIDTH) + (px - 1)] != 0) {
                tightness++;
            }
            if (px >= (BOARD_WIDTH - 1) || board[(py * BOARD_WIDTH) + (px + 1)] != 0) {
                tightness++;
            }
        }
    }

    return tightness;
}

static bool ai_should_use_softdrop(const u8* board, u16 type, u16 rotation, s16 spawnX, s16 x, s16 targetY) {
    u16 tightness = ai_get_descent_tightness(board, type, rotation, x, -2, targetY);
    bool lateralFill = ai_is_lateral_gap_fill(board, type, rotation, x, targetY);
    u16 smart = ai_smartness_level();
    u16 softDropChance;

    (void)spawnX;

    if (smart >= 10) {
        (void)type;
        (void)rotation;
        (void)tightness;
        (void)lateralFill;
        return FALSE;
    }

    /* Base scaling: smart=1 -> 50%, smart=9 -> 5%, smart=10 handled above. */
    softDropChance = (u16)(50 - ((smart - 1) * 45 / 9));

    if (lateralFill) {
        /* Favor softdrop for side-gap finesse, stronger on higher smartness. */
        softDropChance = (u16)(softDropChance + 20 + ((smart - 1) * 20 / 9));
    }

    if (type != 0 && tightness >= 16) {
        softDropChance = (u16)(softDropChance + 10);
    }

    if (softDropChance > 95) softDropChance = 95;

    if (softDropChance > 0) {
        return (u16)(random() % 100) < softDropChance;
    }

    return FALSE;
}

static s16 ai_find_drop_y(const u8* board, u16 type, u16 rotation, s16 x) {
    s16 y = -4;
    if (ai_collides(board, type, rotation, x, y)) return -32768;
    while (!ai_collides(board, type, rotation, x, (s16)(y + 1))) {
        y++;
    }
    return y;
}

static bool ai_place_piece(u8* board, u16 type, u16 rotation, s16 x, s16 y) {
    for (u16 i = 0; i < 4; i++) {
        s16 px = x + PIECES[type][rotation][i][0];
        s16 py = y + PIECES[type][rotation][i][1];

        if (px < 0 || px >= BOARD_WIDTH || py < 0 || py >= BOARD_HEIGHT) return FALSE;
        board[(py * BOARD_WIDTH) + px] = (u8)(type + 1);
    }
    return TRUE;
}

static u16 ai_clear_lines(u8* board) {
    u16 cleared = 0;

    for (s16 y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool full = TRUE;
        u16 row = (u16)(y * BOARD_WIDTH);

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (board[row + x] == 0) {
                full = FALSE;
                break;
            }
        }

        if (full) {
            cleared++;
            for (s16 yy = y; yy > 0; yy--) {
                memcpy(&board[yy * BOARD_WIDTH], &board[(yy - 1) * BOARD_WIDTH], BOARD_WIDTH);
            }
            memset(&board[0], 0, BOARD_WIDTH);
            y++;
        }
    }

    return cleared;
}

static void ai_get_board_heights(const u8* board, s16* heights, s32* holes, s16* maxHeight) {
    *holes = 0;
    *maxHeight = 0;

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        s16 h = 0;
        bool seenBlock = FALSE;

        for (u16 y = 0; y < BOARD_HEIGHT; y++) {
            u8 v = board[(y * BOARD_WIDTH) + x];
            if (v != 0 && !seenBlock) {
                seenBlock = TRUE;
                h = (s16)(BOARD_HEIGHT - y);
            }
            if (seenBlock && v == 0) (*holes)++;
        }

        heights[x] = h;
        if (h > *maxHeight) *maxHeight = h;
    }
}

static s32 ai_get_total_height(const s16* heights) {
    s32 totalHeight = 0;

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        totalHeight += heights[x];
    }

    return totalHeight;
}

static s32 ai_get_bumpiness(const s16* heights) {
    s32 bumpiness = 0;

    for (u16 x = 0; x < BOARD_WIDTH - 1; x++) {
        s16 diff = (s16)(heights[x] - heights[x + 1]);
        if (diff < 0) diff = (s16)-diff;
        bumpiness += diff;
    }

    return bumpiness;
}

static s32 ai_count_sealed_holes(const u8* board) {
    s32 sealedHoles = 0;

    for (u16 y = 1; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 idx = (u16)((y * BOARD_WIDTH) + x);
            bool leftBlocked;
            bool rightBlocked;

            if (board[idx] != 0 || board[idx - BOARD_WIDTH] == 0) continue;

            leftBlocked = (x == 0) || (board[idx - 1] != 0);
            rightBlocked = (x == (BOARD_WIDTH - 1)) || (board[idx + 1] != 0);

            if (leftBlocked && rightBlocked) {
                sealedHoles++;
            }
        }
    }

    return sealedHoles;
}

static s32 ai_count_rows_with_holes(const u8* board) {
    s32 rowsWithHoles = 0;

    for (u16 y = 1; y < BOARD_HEIGHT; y++) {
        bool rowHasHole = FALSE;

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 idx = (u16)((y * BOARD_WIDTH) + x);
            if (board[idx] == 0 && board[idx - BOARD_WIDTH] != 0) {
                rowHasHole = TRUE;
                break;
            }
        }

        if (rowHasHole) rowsWithHoles++;
    }

    return rowsWithHoles;
}

static s32 ai_get_hole_depth(const u8* board) {
    s32 holeDepth = 0;

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        s32 roof = 0;

        for (u16 y = 0; y < BOARD_HEIGHT; y++) {
            u8 cell = board[(y * BOARD_WIDTH) + x];

            if (cell != 0) {
                roof++;
            } else if (roof > 0) {
                holeDepth += roof;
            }
        }
    }

    return holeDepth;
}

static s32 ai_count_row_transitions(const u8* board) {
    s32 transitions = 0;

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        bool prevFilled = TRUE;

        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            bool filled = (board[(y * BOARD_WIDTH) + x] != 0);
            if (filled != prevFilled) transitions++;
            prevFilled = filled;
        }

        if (!prevFilled) transitions++;
    }

    return transitions;
}

static s32 ai_count_column_transitions(const u8* board) {
    s32 transitions = 0;

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        bool prevFilled = TRUE;

        for (u16 y = 0; y < BOARD_HEIGHT; y++) {
            bool filled = (board[(y * BOARD_WIDTH) + x] != 0);
            if (filled != prevFilled) transitions++;
            prevFilled = filled;
        }
    }

    return transitions;
}

static s32 ai_get_peak_penalty(const s16* heights) {
    s32 peaks = 0;

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        s16 left = (x == 0) ? heights[x] : heights[x - 1];
        s16 right = (x == (BOARD_WIDTH - 1)) ? heights[x] : heights[x + 1];
        s16 limit = (left > right) ? left : right;

        if (heights[x] > (s16)(limit + 1)) {
            peaks += (s32)(heights[x] - limit);
        }
    }

    return peaks;
}

static s32 ai_get_edge_well_depth(const s16* heights) {
    s16 leftDepth = (s16)(heights[1] - heights[0]);
    s16 rightDepth = (s16)(heights[BOARD_WIDTH - 2] - heights[BOARD_WIDTH - 1]);

    if (leftDepth < 0) leftDepth = 0;
    if (rightDepth < 0) rightDepth = 0;

    return (leftDepth > rightDepth) ? leftDepth : rightDepth;
}

static s32 ai_get_cumulative_wells(const s16* heights) {
    s32 wells = 0;

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        s16 left = (x == 0) ? BOARD_HEIGHT : heights[x - 1];
        s16 right = (x == (BOARD_WIDTH - 1)) ? BOARD_HEIGHT : heights[x + 1];
        s16 wall = (left < right) ? left : right;

        if (wall > heights[x]) {
            s32 depth = (s32)(wall - heights[x]);
            wells += (depth * (depth + 1)) / 2;
        }
    }

    return wells;
}

static s32 ai_get_interior_well_depth(const s16* heights) {
    s32 wellDepth = 0;

    for (u16 x = 1; x < BOARD_WIDTH - 1; x++) {
        s16 wall = heights[x - 1];
        if (heights[x + 1] < wall) wall = heights[x + 1];

        if (wall > heights[x]) {
            wellDepth += (s32)(wall - heights[x]);
        }
    }

    return wellDepth;
}

static s32 ai_count_cliff_penalty(const s16* heights) {
    s32 penalty = 0;

    for (u16 x = 0; x < BOARD_WIDTH - 1; x++) {
        s16 diff = (s16)(heights[x] - heights[x + 1]);
        if (diff < 0) diff = (s16)-diff;
        if (diff > 2) {
            penalty += (s32)(diff - 2);
        }
    }

    return penalty;
}

static s32 ai_count_zigzag_profile(const s16* heights) {
    s32 turns = 0;
    s16 prevSign = 0;

    for (u16 x = 0; x < BOARD_WIDTH - 1; x++) {
        s16 diff = (s16)(heights[x + 1] - heights[x]);
        s16 sign = 0;

        if (diff > 0) sign = 1;
        else if (diff < 0) sign = -1;

        if (sign != 0 && prevSign != 0 && sign != prevSign) {
            turns++;
        }

        if (sign != 0) prevSign = sign;
    }

    return turns;
}

static s32 ai_count_overhang_cells(const u8* board) {
    s32 overhangs = 0;

    for (u16 y = 0; y < BOARD_HEIGHT - 1; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u16 idx = (u16)((y * BOARD_WIDTH) + x);
            u16 below = (u16)(((y + 1) * BOARD_WIDTH) + x);

            if (board[idx] != 0 && board[below] == 0) {
                overhangs++;
            }
        }
    }

    return overhangs;
}

static s32 ai_get_center_height(const s16* heights) {
    s32 center = 0;

    for (u16 x = 3; x <= 6; x++) {
        center += heights[x];
    }

    return center / 4;
}

static s32 ai_get_contact_points(const u8* baseBoard, u16 type, u16 rotation, s16 x, s16 y) {
    s32 contacts = 0;

    for (u16 i = 0; i < 4; i++) {
        s16 px = x + PIECES[type][rotation][i][0];
        s16 py = y + PIECES[type][rotation][i][1];

        if (px <= 0 || px >= (BOARD_WIDTH - 1) || py >= (BOARD_HEIGHT - 1)) {
            contacts++;
        }

        if (py + 1 < BOARD_HEIGHT && py + 1 >= 0 && baseBoard[((py + 1) * BOARD_WIDTH) + px] != 0) {
            contacts++;
        }
        if (px > 0 && py >= 0 && py < BOARD_HEIGHT && baseBoard[(py * BOARD_WIDTH) + (px - 1)] != 0) {
            contacts++;
        }
        if (px < (BOARD_WIDTH - 1) && py >= 0 && py < BOARD_HEIGHT && baseBoard[(py * BOARD_WIDTH) + (px + 1)] != 0) {
            contacts++;
        }
    }

    return contacts;
}

static u16 ai_line_reward(u16 linesCleared, bool highStack) {
    switch (linesCleared) {
        case 1: return highStack ? 650 : 0;
        case 2: return highStack ? 1300 : 650;
        case 3: return highStack ? 2300 : 1500;
        case 4: return highStack ? 3800 : 3600;
        default: return 0;
    }
}

static s32 ai_get_landing_height(u16 type, u16 rotation, s16 y) {
    s16 minPY = 100;
    s16 maxPY = -100;

    for (u16 i = 0; i < 4; i++) {
        s16 py = PIECES[type][rotation][i][1];
        if (py < minPY) minPY = py;
        if (py > maxPY) maxPY = py;
    }

    return (s32)((BOARD_HEIGHT * 2) - ((y + minPY) + (y + maxPY)));
}

static s32 ai_get_eroded_piece_cells(const u8* placedBoard, u16 type, u16 rotation, s16 x, s16 y, u16 linesCleared) {
    s32 pieceCellsInClearedRows = 0;

    if (linesCleared == 0) return 0;

    for (u16 i = 0; i < 4; i++) {
        s16 py = y + PIECES[type][rotation][i][1];
        bool rowFull = TRUE;

        if (py < 0 || py >= BOARD_HEIGHT) continue;

        for (u16 col = 0; col < BOARD_WIDTH; col++) {
            if (placedBoard[(py * BOARD_WIDTH) + col] == 0) {
                rowFull = FALSE;
                break;
            }
        }

        if (rowFull) pieceCellsInClearedRows++;
    }

    return pieceCellsInClearedRows * linesCleared;
}

static s32 ai_eval_board(const u8* baseBoard, const u8* placedBoard, const u8* board, u16 linesCleared, u16 type, u16 rotation, s16 x, s16 y) {
    s16 baseHeights[BOARD_WIDTH];
    s16 heights[BOARD_WIDTH];
    s16 baseMaxHeight;
    s16 maxHeight;
    s32 baseHoles;
    s32 holes;
    s32 baseTotalHeight;
    s32 totalHeight;
    s32 bumpiness;
    s32 rowTransitions;
    s32 columnTransitions;
    s32 sealedHoles;
    s32 rowsWithHoles;
    s32 holeDepth;
    s32 peakPenalty;
    s32 baseEdgeWellDepth;
    s32 edgeWellDepth;
    s32 interiorWellDepth;
    s32 cumulativeWells;
    s32 cliffPenalty;
    s32 zigzagPenalty;
    s32 overhangCells;
    s32 centerHeight;
    s32 avgHeight;
    s32 contactPoints;
    s32 erodedPieceCells;
    s32 landingHeight;
    s32 riseSum = 0;
    s32 riseColumns = 0;
    s32 utility;
    s32 holeCareBonus;
    u16 smart;
    s16 rightColHeight;
    s16 minOtherHeight;
    bool inDanger;
    bool highStack;
    bool survivalMode;
    bool safeMode;

    ai_get_board_heights(baseBoard, baseHeights, &baseHoles, &baseMaxHeight);
    ai_get_board_heights(board, heights, &holes, &maxHeight);

    baseTotalHeight = ai_get_total_height(baseHeights);
    totalHeight = ai_get_total_height(heights);
    bumpiness = ai_get_bumpiness(heights);
    rowTransitions = ai_count_row_transitions(board);
    columnTransitions = ai_count_column_transitions(board);
    sealedHoles = ai_count_sealed_holes(board);
    rowsWithHoles = ai_count_rows_with_holes(board);
    holeDepth = ai_get_hole_depth(board);
    peakPenalty = ai_get_peak_penalty(heights);
    baseEdgeWellDepth = ai_get_edge_well_depth(baseHeights);
    edgeWellDepth = ai_get_edge_well_depth(heights);
    interiorWellDepth = ai_get_interior_well_depth(heights);
    cumulativeWells = ai_get_cumulative_wells(heights);
    cliffPenalty = ai_count_cliff_penalty(heights);
    zigzagPenalty = ai_count_zigzag_profile(heights);
    overhangCells = ai_count_overhang_cells(board);
    centerHeight = ai_get_center_height(heights);
    avgHeight = totalHeight / BOARD_WIDTH;
    contactPoints = ai_get_contact_points(baseBoard, type, rotation, x, y);
    erodedPieceCells = ai_get_eroded_piece_cells(placedBoard, type, rotation, x, y, linesCleared);
    landingHeight = ai_get_landing_height(type, rotation, y);
    smart = ai_smartness_level();
    holeCareBonus = (s32)(40 * (smart - 1));
    inDanger = (baseMaxHeight >= 12) || (baseHoles >= 3);
    highStack = (maxHeight >= (BOARD_HEIGHT * 2) / 3);
    survivalMode = (maxHeight >= AI_SURVIVAL_HEIGHT_THRESHOLD) || (holes >= 3);
    safeMode = (holes == 0) && (avgHeight <= AI_SAFE_AVG_HEIGHT_THRESHOLD) && (maxHeight <= 12);
    rightColHeight = heights[BOARD_WIDTH - 1];
    minOtherHeight = heights[0];

    for (u16 c = 1; c < BOARD_WIDTH - 1; c++) {
        if (heights[c] < minOtherHeight) minOtherHeight = heights[c];
    }

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        s16 rise = (s16)(heights[x] - baseHeights[x]);
        if (rise > 0) {
            riseSum += rise;
            riseColumns++;
        }
    }

    utility = (s32)ai_line_reward(linesCleared, highStack);
    utility += (s32)(240 * erodedPieceCells);
    utility += (s32)((750 + holeCareBonus) * (baseHoles - holes));
    utility += (s32)(180 * (baseMaxHeight - maxHeight));
    utility += (s32)(45 * (baseTotalHeight - totalHeight));

    utility -= (s32)(90 * landingHeight);
    utility -= (s32)((1100 + holeCareBonus) * holes);
    utility -= (s32)((520 + (holeCareBonus / 2)) * rowsWithHoles);
    utility -= (s32)((120 + (holeCareBonus / 5)) * holeDepth);
    utility -= (s32)((900 + holeCareBonus) * sealedHoles);
    utility -= (s32)(70 * bumpiness);
    utility -= (s32)(95 * rowTransitions);
    utility -= (s32)(80 * columnTransitions);
    utility -= (s32)(35 * cumulativeWells);
    utility -= (s32)(30 * totalHeight);
    utility -= (s32)(220 * maxHeight);
    utility -= (s32)(180 * peakPenalty);
    utility -= (s32)(160 * interiorWellDepth);
    utility -= (s32)(240 * overhangCells);
    utility -= (s32)(210 * cliffPenalty);
    utility -= (s32)(90 * zigzagPenalty);
    utility -= (s32)(55 * centerHeight);
    utility += (s32)(30 * contactPoints);

    if (avgHeight > AI_SAFE_AVG_HEIGHT_THRESHOLD) {
        utility -= (s32)(110 * (avgHeight - AI_SAFE_AVG_HEIGHT_THRESHOLD));
    }

    if (!inDanger) {
        if (edgeWellDepth >= 4 && holes == 0) {
            utility += (s32)(220 * edgeWellDepth);
        }

        if (baseEdgeWellDepth >= 4 && edgeWellDepth < baseEdgeWellDepth && linesCleared <= 1) {
            utility -= (s32)(1800 + (250 * (baseEdgeWellDepth - edgeWellDepth)));
        }

        if (linesCleared == 1 && baseEdgeWellDepth >= 4 && holes <= baseHoles) {
            utility -= 1200;
        }

        if (safeMode && linesCleared == 1 && holes == 0) {
            utility -= 900;
        }

        if (safeMode && rightColHeight <= (s16)(minOtherHeight + 1) && linesCleared == 0) {
            utility += 260;
        }
    } else {
        utility += (s32)(80 * edgeWellDepth);
    }

    if ((type == 0) && ((rotation & 1) != 0) && linesCleared == 4) {
        utility += 2000;
    }

    if (maxHeight > baseMaxHeight) {
        utility -= (s32)(5000 * (maxHeight - baseMaxHeight));
    }

    if (riseSum > 0) {
        utility -= (s32)(900 * riseSum);
        utility -= (s32)(400 * riseColumns);
    }

    if (survivalMode) {
        utility += (s32)(700 * linesCleared);
        utility += (s32)(1500 * (baseHoles - holes));
        utility -= (s32)(1800 * holes);
        utility -= (s32)(1200 * sealedHoles);
        utility -= (s32)(450 * overhangCells);
        utility -= (s32)(380 * cliffPenalty);
        utility -= (s32)(250 * maxHeight);
        if (linesCleared == 4) {
            utility -= 900;
        }
    }

    return utility;
}

static u16 ai_random_action_delay(GameContext* player, bool finalDrop) {
    u16 decisiveness = ai_decisiveness_level();
    u16 baseDelay;
    u16 jitter;
    u16 minDelay;
    u16 maxDelay;
    u16 span;

    (void)player;

    /* Level 1 feels human (~10 frames), level 10 still waits 1 frame per action. */
    if (decisiveness >= 10) {
        return 1;
    }

    baseDelay = (u16)(10 - (((decisiveness - 1) * 10) / 9));
    jitter = (u16)((11 - decisiveness) / 3);

    if (baseDelay > jitter) {
        minDelay = (u16)(baseDelay - jitter);
    } else {
        minDelay = 0;
    }
    maxDelay = (u16)(baseDelay + jitter);

    /* Decisiveness now acts about 2x faster across the range. */
    minDelay = (u16)(minDelay / 2);
    maxDelay = (u16)(maxDelay / 2);

    if (finalDrop && minDelay > 0) {
        minDelay--;
    }
    if (finalDrop && maxDelay > minDelay) {
        maxDelay--;
    }

    if (maxDelay < minDelay) {
        maxDelay = minDelay;
    }

    span = (u16)(maxDelay - minDelay + 1);
    return (u16)(minDelay + (random() % span));
}

static u16 ai_get_gravity_threshold(const GameContext* player) {
    s16 threshold = (s16)GET_TICKS(48 - (player->level > 1 ? (player->level - 1) * 2 : 0));

    if (threshold < 2) threshold = 2;
    return (u16)threshold;
}

static u16 ai_get_think_budget(const VsContext* vctx, const GameContext* player) {
    u16 load = SYS_getCPULoad();
    u16 decisiveness = ai_decisiveness_level();
    u16 threshold = ai_get_gravity_threshold(player);
    u16 framesUntilGravity = (player->moveTimer >= threshold) ? 0 : (u16)(threshold - player->moveTimer);
    u16 maxBudget = AI_THINK_STEPS_MAX;
    u16 budget;

    if (framesUntilGravity <= 1) {
        budget = 0;
    } else if (framesUntilGravity <= 4) {
        budget = 1;
    } else {
        budget = 2;
    }

    if (vctx->rightAiState == AI_STATE_LOOKAHEAD && budget > 0) {
        budget--;
    }

    if (decisiveness >= 10) {
        maxBudget = (u16)(maxBudget + 2);
    } else if (decisiveness >= 8) {
        maxBudget = (u16)(maxBudget + 1);
    }

    if (load >= AI_CPU_LOAD_HARD_LIMIT) {
        return 0;
    }

    if (load > AI_CPU_LOAD_TARGET) {
        u16 penalty = (u16)((load - AI_CPU_LOAD_TARGET) / 2);
        if (penalty >= budget) {
            budget = 0;
        } else {
            budget = (u16)(budget - penalty);
        }
    } else if (load + 16 < AI_CPU_LOAD_TARGET && budget < AI_THINK_STEPS_MAX) {
        budget++;
    }

    if (decisiveness >= 8 && budget < maxBudget) {
        budget++;
    }

    if (budget > maxBudget) {
        budget = maxBudget;
    }

    return budget;
}

static s16 ai_min_x_for(u16 type, u16 rotation) {
    s16 minX = 100;

    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[type][rotation][i][0];
        if (px < minX) minX = px;
    }

    return (s16)-minX;
}

static s16 ai_max_x_for(u16 type, u16 rotation) {
    s16 maxX = -100;

    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[type][rotation][i][0];
        if (px > maxX) maxX = px;
    }

    return (s16)((BOARD_WIDTH - 1) - maxX);
}

static void ai_begin_scan(VsContext* vctx, GameContext* player) {
    ai_reset_top_candidates();
    vctx->rightAiState = AI_STATE_SCAN;
    vctx->rightAiHasBest = FALSE;
    vctx->rightAiPlanReady = FALSE;
    vctx->rightAiHardDropArmed = FALSE;
    vctx->rightAiBestScore = AI_SCORE_MIN;
    vctx->rightAiTargetX = player->pieceX;
    vctx->rightAiTargetY = player->pieceY;
    vctx->rightAiTargetRot = (u16)(player->rotation & 3);
    vctx->rightAiScanRot = 0;
    vctx->rightAiScanX = ai_min_x_for(player->type, 0);
    vctx->rightAiPlannedType = (s16)player->type;
    vctx->rightAiThinkTimer = ai_scale_think_timer(AI_SCAN_TIME_BUDGET);
    vctx->rightAiUseSoftDrop = FALSE;
    vctx->rightAiPulseTimer = 0;
}

static void ai_finish_scan(VsContext* vctx, GameContext* player) {
    bool hasTop = FALSE;

    for (u16 i = 0; i < AI_TOP_CANDIDATES; i++) {
        if (aiTopCandidates[i].valid) {
            hasTop = TRUE;
            aiTopCandidates[i].done = FALSE;
            aiTopCandidates[i].nextBestScore = AI_SCORE_MIN;
            aiTopCandidates[i].nextScanRot = 0;
            aiTopCandidates[i].nextScanX = ai_min_x_for(player->nextType, 0);
        }
    }

    if (!vctx->rightAiHasBest) {
        vctx->rightAiTargetX = player->pieceX;
        vctx->rightAiTargetY = player->pieceY;
        vctx->rightAiTargetRot = (u16)(player->rotation & 3);
        vctx->rightAiUseSoftDrop = (ai_smartness_level() <= 5);
    }

    vctx->rightAiHardDropArmed = FALSE;
    if (hasTop) {
        vctx->rightAiPlanReady = FALSE;
        vctx->rightAiState = AI_STATE_LOOKAHEAD;
        vctx->rightAiThinkTimer = ai_scale_think_timer(AI_LOOKAHEAD_TIME_BUDGET);
    } else {
        vctx->rightAiPlanReady = TRUE;
        vctx->rightAiState = AI_STATE_EXECUTE;
        vctx->rightAiThinkTimer = 0;
    }
    vctx->rightAiPulseTimer = ai_random_action_delay(player, FALSE);
}

static bool ai_refine_one_top_candidate(GameContext* player) {
    for (u16 n = 0; n < AI_TOP_CANDIDATES; n++) {
        u16 idx = (u16)((aiLookaheadCursor + n) % AI_TOP_CANDIDATES);
        AiTopCandidate* c = &aiTopCandidates[idx];
        u16 rot;
        s16 x;
        s16 y;
        s16 maxX;
        u8 placedBoard[200];
        u8 boardCopy[200];
        u16 lines;
        s32 score;

        if (!c->valid || c->done) continue;

        rot = c->nextScanRot;
        if (rot >= 4) {
            c->done = TRUE;
            if (c->nextBestScore == AI_SCORE_MIN) c->nextBestScore = -4000000L;
            aiLookaheadCursor = (u16)((idx + 1) % AI_TOP_CANDIDATES);
            return FALSE;
        }

        x = c->nextScanX;
        maxX = ai_max_x_for(player->nextType, rot);
        y = ai_find_drop_y(c->boardAfter, player->nextType, rot, x);

        if (y != -32768) {
            memcpy(placedBoard, c->boardAfter, sizeof(placedBoard));
            if (ai_place_piece(placedBoard, player->nextType, rot, x, y)) {
                memcpy(boardCopy, placedBoard, sizeof(boardCopy));
                lines = ai_clear_lines(boardCopy);
                score = ai_eval_board(c->boardAfter, placedBoard, boardCopy, lines, player->nextType, rot, x, y);
                if (score > c->nextBestScore) {
                    c->nextBestScore = score;
                }
            }
        }

        if (x < maxX) {
            c->nextScanX++;
        } else {
            c->nextScanRot++;
            if (c->nextScanRot < 4) {
                c->nextScanX = ai_min_x_for(player->nextType, c->nextScanRot);
            } else {
                c->done = TRUE;
                if (c->nextBestScore == AI_SCORE_MIN) c->nextBestScore = -4000000L;
            }
        }

        aiLookaheadCursor = (u16)((idx + 1) % AI_TOP_CANDIDATES);
        return FALSE;
    }

    return TRUE;
}

static void ai_finish_lookahead(VsContext* vctx, GameContext* player) {
    bool found = FALSE;
    s32 bestScore = AI_SCORE_MIN;
    s16 bestX = player->pieceX;
    s16 bestY = player->pieceY;
    u16 bestRot = (u16)(player->rotation & 3);
    bool bestUseSoftDrop = FALSE;

    for (u16 i = 0; i < AI_TOP_CANDIDATES; i++) {
        AiTopCandidate* c = &aiTopCandidates[i];
        s32 total;

        if (!c->valid) continue;

        total = c->baseScore + (c->nextBestScore * AI_NEXT_LOOKAHEAD_WEIGHT) / 100;
        c->totalScore = total;

        if (!found || ai_is_better_candidate(total, c->x, c->rot, bestScore, bestX, bestRot, player->pieceX)) {
            found = TRUE;
            bestScore = total;
            bestX = c->x;
            bestY = c->y;
            bestRot = c->rot;
            bestUseSoftDrop = c->useSoftDrop;
        }
    }

    if (found) {
        vctx->rightAiHasBest = TRUE;
        vctx->rightAiBestScore = bestScore;
        vctx->rightAiTargetX = bestX;
        vctx->rightAiTargetY = bestY;
        vctx->rightAiTargetRot = bestRot;
        vctx->rightAiUseSoftDrop = bestUseSoftDrop;
    }

    vctx->rightAiPlanReady = TRUE;
    vctx->rightAiState = AI_STATE_EXECUTE;
    vctx->rightAiHardDropArmed = FALSE;
    vctx->rightAiThinkTimer = 0;
    vctx->rightAiPulseTimer = ai_random_action_delay(player, FALSE);
}

static void ai_scan_one_candidate(VsContext* vctx, GameContext* player) {
    u16 rot = vctx->rightAiScanRot;
    s16 x = vctx->rightAiScanX;
    s16 maxX;
    s16 y;
    u8 placedBoard[200];
    u8 boardCopy[200];
    u16 lines;
    s32 score;

    if (rot >= 4) {
        ai_finish_scan(vctx, player);
        return;
    }

    maxX = ai_max_x_for(player->type, rot);
    y = ai_find_drop_y(player->board, player->type, rot, x);

    if (y != -32768) {
        memcpy(placedBoard, player->board, sizeof(placedBoard));
        if (ai_place_piece(placedBoard, player->type, rot, x, y)) {
            bool useSoftDrop = ai_should_use_softdrop(player->board, player->type, rot, player->pieceX, x, y);
            memcpy(boardCopy, placedBoard, sizeof(boardCopy));
            lines = ai_clear_lines(boardCopy);
            score = ai_eval_board(player->board, placedBoard, boardCopy, lines, player->type, rot, x, y);
            ai_try_store_top_candidate(x, y, rot, useSoftDrop, score, boardCopy);

            if (!vctx->rightAiHasBest || ai_is_better_candidate(score, x, rot, vctx->rightAiBestScore, vctx->rightAiTargetX, vctx->rightAiTargetRot, player->pieceX)) {
                vctx->rightAiHasBest = TRUE;
                vctx->rightAiBestScore = score;
                vctx->rightAiTargetX = x;
                vctx->rightAiTargetY = y;
                vctx->rightAiTargetRot = rot;
                vctx->rightAiUseSoftDrop = useSoftDrop;
            }
        }
    }

    if (x < maxX) {
        vctx->rightAiScanX++;
    } else {
        vctx->rightAiScanRot++;
        if (vctx->rightAiScanRot < 4) {
            vctx->rightAiScanX = ai_min_x_for(player->type, vctx->rightAiScanRot);
        } else {
            ai_finish_scan(vctx, player);
        }
    }
}

static bool ai_try_rotate_live(GameContext* player, u16 newRotation) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;
    s16 kicks[5] = {0, 1, -1, 2, -2};

    ai_bind_player(player, &savedCtx, &savedJoy, &savedLastJoy);
    for (u16 i = 0; i < 5; i++) {
        if (!checkCollision(player->pieceX + kicks[i], player->pieceY, newRotation)) {
            player->pieceX += kicks[i];
            player->rotation = newRotation;
            calculate_ghost_y();
            ai_unbind_player(savedCtx, savedJoy, savedLastJoy);
            return TRUE;
        }
    }
    ai_unbind_player(savedCtx, savedJoy, savedLastJoy);
    return FALSE;
}

static bool ai_try_move_live(GameContext* player, s16 step) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;

    ai_bind_player(player, &savedCtx, &savedJoy, &savedLastJoy);
    if (!checkCollision(player->pieceX + step, player->pieceY, player->rotation)) {
        player->pieceX += step;
        calculate_ghost_y();
        ai_unbind_player(savedCtx, savedJoy, savedLastJoy);
        return TRUE;
    }
    ai_unbind_player(savedCtx, savedJoy, savedLastJoy);
    return FALSE;
}

static bool ai_try_step_down_live(GameContext* player) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;

    ai_bind_player(player, &savedCtx, &savedJoy, &savedLastJoy);
    if (!checkCollision(player->pieceX, player->pieceY + 1, player->rotation)) {
        player->pieceY++;
        calculate_ghost_y();
        ai_unbind_player(savedCtx, savedJoy, savedLastJoy);
        return TRUE;
    }
    ai_unbind_player(savedCtx, savedJoy, savedLastJoy);
    return FALSE;
}

static bool ai_handle_gravity(VsContext* vctx, GameContext* player, bool* deadFlag) {
    u16 threshold = ai_get_gravity_threshold(player);

    player->moveTimer++;
    if (player->moveTimer < threshold) {
        return FALSE;
    }

    player->moveTimer = 0;
    vctx->rightAiHardDropArmed = FALSE;

    if (!ai_try_step_down_live(player)) {
        if (!ai_lock_and_respawn(vctx, player)) {
            *deadFlag = TRUE;
        }
    }

    return TRUE;
}

static bool ai_step_towards_target(VsContext* vctx, GameContext* player) {
    u16 rotNow = (u16)(player->rotation & 3);
    u16 rotTarget = (u16)(vctx->rightAiTargetRot & 3);
    u16 cw = (u16)((rotTarget - rotNow) & 3);
    u16 ccw = (u16)((rotNow - rotTarget) & 3);

    if (vctx->rightAiPulseTimer > 0) {
        vctx->rightAiPulseTimer--;
        return FALSE;
    }

    if (cw != 0) {
        u16 nextRot = (u16)((rotNow + ((ccw < cw) ? 3 : 1)) & 3);
        if (ai_try_rotate_live(player, nextRot)) {
            vctx->rightAiHardDropArmed = FALSE;
            vctx->rightAiPulseTimer = ai_random_action_delay(player, FALSE);
            return TRUE;
        }
    } else if (player->pieceX < vctx->rightAiTargetX) {
        if (ai_try_move_live(player, 1)) {
            vctx->rightAiHardDropArmed = FALSE;
            vctx->rightAiPulseTimer = ai_random_action_delay(player, FALSE);
            return TRUE;
        }
    } else if (player->pieceX > vctx->rightAiTargetX) {
        if (ai_try_move_live(player, -1)) {
            vctx->rightAiHardDropArmed = FALSE;
            vctx->rightAiPulseTimer = ai_random_action_delay(player, FALSE);
            return TRUE;
        }
    }

    return FALSE;
}

static void ai_prepare_new_piece(VsContext* vctx, GameContext* player) {
    ai_begin_scan(vctx, player);
}

static bool ai_spawn_piece(VsContext* vctx, GameContext* player) {
    GameContext* savedCtx;
    u16 savedJoy;
    u16 savedLastJoy;

    ai_bind_player(player, &savedCtx, &savedJoy, &savedLastJoy);
    player->type = player->nextType;
    if (player->bagIndex >= 7) {
        refillBag();
    }
    player->nextType = player->bag[player->bagIndex++];
    player->rotation = 0;
    player->pieceX = 3;
    player->pieceY = (player->type == 0) ? -1 : 0;
    player->moveTimer = 0;
    calculate_ghost_y();

    {
        bool blocked = checkCollision(player->pieceX, player->pieceY, player->rotation);
        ai_unbind_player(savedCtx, savedJoy, savedLastJoy);
        ai_prepare_new_piece(vctx, player);
        player->boardFlags |= GF_NEEDS_DRAW;
        return !blocked;
    }
}

static u16 ai_clear_lines_simple(GameContext* player) {
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

static bool ai_lock_and_respawn(VsContext* vctx, GameContext* player) {
    for (u16 i = 0; i < 4; i++) {
        s16 gx = player->pieceX + PIECES[player->type][player->rotation][i][0];
        s16 gy = player->pieceY + PIECES[player->type][player->rotation][i][1];

        if (gx >= 0 && gx < BOARD_WIDTH && gy >= 0 && gy < BOARD_HEIGHT) {
            player->board[gx + ((gy << 3) + (gy << 1))] = (u8)(player->type + 1);
        }
    }

    ai_clear_lines_simple(player);
    return ai_spawn_piece(vctx, player);
}

void vs_brain_reset(VsContext* vctx) {
    vctx->rightAiHasBest = FALSE;
    vctx->rightAiPlanReady = FALSE;
    vctx->rightAiHardDropArmed = FALSE;
    vctx->rightAiState = AI_STATE_SCAN;
    vctx->rightAiScanRot = 0;
    vctx->rightAiTargetX = 3;
    vctx->rightAiScanX = 3;
    vctx->rightAiTargetY = 0;
    vctx->rightAiTargetRot = 0;
    vctx->rightAiPlannedType = -1;
    vctx->rightAiBestScore = AI_SCORE_MIN;
    vctx->rightAiThinkBudget = 0;
    vctx->rightAiThinkTimer = 0;
    vctx->rightAiUseSoftDrop = FALSE;
    vctx->rightAiPulseTimer = 0;

    aiPrevX = 0;
    aiPrevY = 0;
    aiPrevRot = 0;
    aiStallFrames = 0;
}

void vs_brain_update_player(VsContext* vctx, GameContext* player, bool* deadFlag, bool* needsRedraw) {
    bool dirty = FALSE;
    u16 thinkBudget;

    if (*deadFlag) return;

    if (vctx->rightAiPlannedType != (s16)player->type) {
        ai_begin_scan(vctx, player);
    }

    thinkBudget = ai_get_think_budget(vctx, player);
    vctx->rightAiThinkBudget = thinkBudget;

    if (vctx->rightAiState == AI_STATE_SCAN || vctx->rightAiState == AI_STATE_LOOKAHEAD) {
        if (vctx->rightAiThinkTimer > 0) {
            vctx->rightAiThinkTimer--;
        }
    }

    if (vctx->rightAiState == AI_STATE_SCAN) {
        while (thinkBudget > 0 && vctx->rightAiState == AI_STATE_SCAN) {
            ai_scan_one_candidate(vctx, player);
            thinkBudget--;
        }

        if (vctx->rightAiState == AI_STATE_SCAN && vctx->rightAiThinkTimer == 0) {
            ai_finish_scan(vctx, player);
        }

        if (vctx->rightAiState == AI_STATE_SCAN) {
            if (vctx->rightAiHasBest) {
                u16 actions = ai_execution_actions_per_frame();
                while (actions > 0) {
                    if (!ai_step_towards_target(vctx, player)) break;
                    dirty = TRUE;
                    actions--;
                }
            }

            if (ai_handle_gravity(vctx, player, deadFlag)) {
                dirty = TRUE;
            }

            aiPrevX = player->pieceX;
            aiPrevY = player->pieceY;
            aiPrevRot = player->rotation;
            aiStallFrames = 0;

            if (dirty) {
                player->boardFlags |= GF_NEEDS_DRAW;
                *needsRedraw = TRUE;
            }
            return;
        }

        aiPrevX = player->pieceX;
        aiPrevY = player->pieceY;
        aiPrevRot = player->rotation;
        aiStallFrames = 0;
    }

    if (vctx->rightAiState == AI_STATE_LOOKAHEAD) {
        bool done = FALSE;

        while (thinkBudget > 0 && vctx->rightAiState == AI_STATE_LOOKAHEAD) {
            done = ai_refine_one_top_candidate(player);
            thinkBudget--;

            if (done) {
                ai_finish_lookahead(vctx, player);
                break;
            }
        }

        if (vctx->rightAiState == AI_STATE_LOOKAHEAD && vctx->rightAiThinkTimer == 0) {
            ai_finish_lookahead(vctx, player);
        }

        if (vctx->rightAiState == AI_STATE_LOOKAHEAD) {
            if (vctx->rightAiHasBest) {
                u16 actions = ai_execution_actions_per_frame();
                while (actions > 0) {
                    if (!ai_step_towards_target(vctx, player)) break;
                    dirty = TRUE;
                    actions--;
                }
            }

            if (ai_handle_gravity(vctx, player, deadFlag)) {
                dirty = TRUE;
            }

            aiPrevX = player->pieceX;
            aiPrevY = player->pieceY;
            aiPrevRot = player->rotation;
            aiStallFrames = 0;

            if (dirty) {
                player->boardFlags |= GF_NEEDS_DRAW;
                *needsRedraw = TRUE;
            }
            return;
        }
    }

    if (vctx->rightAiPulseTimer > 0) {
        vctx->rightAiPulseTimer--;
    }

    if (vctx->rightAiPulseTimer == 0) {
        bool aligned = (player->pieceX == vctx->rightAiTargetX) && (((u16)(player->rotation & 3)) == ((u16)(vctx->rightAiTargetRot & 3)));

        if (!aligned) {
            u16 actions = ai_execution_actions_per_frame();
            while (actions > 0 && !aligned) {
                if (!ai_step_towards_target(vctx, player)) break;
                dirty = TRUE;
                actions--;
                aligned = (player->pieceX == vctx->rightAiTargetX) && (((u16)(player->rotation & 3)) == ((u16)(vctx->rightAiTargetRot & 3)));
            }
        }

        if (aligned) {
            if (ai_smartness_level() >= 10 && vctx->rightAiUseSoftDrop) {
                vctx->rightAiUseSoftDrop = FALSE;
                vctx->rightAiHardDropArmed = FALSE;
            }

            if (vctx->rightAiUseSoftDrop) {
                if (ai_try_step_down_live(player)) {
                    dirty = TRUE;
                    if (ai_smartness_level() <= 4 && player->pieceY >= (s16)(vctx->rightAiTargetY - 3)) {
                        u16 switchChance = (u16)((5 - ai_smartness_level()) * 12);
                        if ((u16)(random() % 100) < switchChance) {
                            vctx->rightAiUseSoftDrop = FALSE;
                            vctx->rightAiHardDropArmed = TRUE;
                        }
                    }
                    vctx->rightAiPulseTimer = ai_random_action_delay(player, TRUE);
                } else {
                    if (!ai_lock_and_respawn(vctx, player)) {
                        *deadFlag = TRUE;
                    }
                    vctx->rightAiHardDropArmed = FALSE;
                    vctx->rightAiPulseTimer = 0;
                    dirty = TRUE;
                }
            } else if (ai_smartness_level() >= 10) {
                while (ai_try_step_down_live(player)) {
                    dirty = TRUE;
                }
                if (!ai_lock_and_respawn(vctx, player)) {
                    *deadFlag = TRUE;
                }
                vctx->rightAiHardDropArmed = FALSE;
                vctx->rightAiPulseTimer = 0;
                dirty = TRUE;
            } else if (!vctx->rightAiHardDropArmed) {
                vctx->rightAiHardDropArmed = TRUE;
                vctx->rightAiPulseTimer = ai_random_action_delay(player, TRUE);
            } else {
                while (ai_try_step_down_live(player)) {
                    dirty = TRUE;
                }
                if (!ai_lock_and_respawn(vctx, player)) {
                    *deadFlag = TRUE;
                }
                vctx->rightAiHardDropArmed = FALSE;
                vctx->rightAiPulseTimer = 0;
                dirty = TRUE;
            }
        }
    }

    {
        bool tryingMove = (player->pieceX != vctx->rightAiTargetX) || ((player->rotation & 3) != (vctx->rightAiTargetRot & 3));
        bool unchanged = (player->pieceX == aiPrevX) && (player->pieceY == aiPrevY) && ((player->rotation & 3) == (aiPrevRot & 3));

        if (tryingMove && unchanged) {
            if (aiStallFrames < 0xFFFF) aiStallFrames++;
        } else {
            aiStallFrames = 0;
        }

        if (aiStallFrames > 20) {
            ai_begin_scan(vctx, player);
            return;
        }

        if (aiStallFrames > 45) {
            while (ai_try_step_down_live(player)) {
                dirty = TRUE;
            }
            if (!ai_lock_and_respawn(vctx, player)) {
                *deadFlag = TRUE;
            }
            aiStallFrames = 0;
            dirty = TRUE;
        }

        aiPrevX = player->pieceX;
        aiPrevY = player->pieceY;
        aiPrevRot = player->rotation;
    }

    if (ai_handle_gravity(vctx, player, deadFlag)) {
        dirty = TRUE;
    }

    if (dirty) {
        player->boardFlags |= GF_NEEDS_DRAW;
        *needsRedraw = TRUE;
    }
}
