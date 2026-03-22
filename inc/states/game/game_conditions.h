#pragma once

#include <genesis.h>
#include "states/states.h"

// --- GAME CONDITIONS (runtime rule set for Free Game / Challenge) ---
#define GC_RULE_ALLOW_SHADOW      (1U << 0)
#define GC_RULE_ALLOW_HOLD        (1U << 1)
#define GC_RULE_SHOW_NEXT         (1U << 2)
#define GC_RULE_DEBUG_UI          (1U << 3)
#define GC_RULE_RANDOM_CHAOS      (1U << 4)
#define GC_RULE_ITEMS_ENABLED     (1U << 5)
#define GC_RULE_ITEMS_GOOD_ONLY   (1U << 6)
#define GC_RULE_ITEMS_BAD_ONLY    (1U << 7)

#define GC_GOAL_SCORE             (1UL << 0)
#define GC_GOAL_LINES             (1UL << 1)
#define GC_GOAL_HEARTS            (1UL << 2)
#define GC_GOAL_SKULLS            (1UL << 3)
#define GC_GOAL_SURVIVE_TIME      (1UL << 4)
#define GC_GOAL_SURVIVE_LINES     (1UL << 5)
#define GC_GOAL_PIECE_LIMIT       (1UL << 6)
#define GC_GOAL_CLEAN_BOARD       (1UL << 7)

typedef struct GameConditions {
    u16 speedLevel;
    u16 garbageFreq;
    u16 thresholdLRInitial;
    u16 thresholdLRRepeat;
    u16 thresholdSD;
    u16 ruleFlags;

    // Placeholders for challenge win conditions.
    u32 goalFlags;
    u32 goalScore;
    u16 goalLines;
    u16 goalHearts;
    u16 goalSkulls;
    u16 goalTimeSec;
    u16 goalSurviveLines;
    u16 goalPieceLimit;
    bool success;
} GameConditions;

extern GameConditions gameConditions;

void game_conditions_set_from_select(const SelectContext *selectCtx);
void game_conditions_set_challenge_training(void);

static inline bool gc_has_rule(u16 flag) {
    return (gameConditions.ruleFlags & flag) != 0;
}

static inline u16 gc_item_mode(void) {
    if (!gc_has_rule(GC_RULE_ITEMS_ENABLED)) return 0;
    if (gc_has_rule(GC_RULE_ITEMS_GOOD_ONLY)) return 2;
    if (gc_has_rule(GC_RULE_ITEMS_BAD_ONLY)) return 3;
    return 1;
}
