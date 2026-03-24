#include "states/game/conditions.h"

#include "states/challenge.h"
#include "states/game/game_conditions.h"
#include "states/game/quests.h"
#include "sound_manager.h"

GameConditions gameConditions;

static void game_conditions_reset_goals(void)
{
    gameConditions.goalFlags = 0;
    gameConditions.goalScore = 0;
    gameConditions.goalLines = 0;
    gameConditions.goalHearts = 0;
    gameConditions.goalSkulls = 0;
    gameConditions.goalTimeSec = 0;
    gameConditions.goalSurviveLines = 0;
    gameConditions.goalPieceLimit = 0;
    gameConditions.goalClearCount = 0;
    gameConditions.currentClearCount = 0;
    gameConditions.goalDoubleCount = 0;
    gameConditions.currentDoubleCount = 0;
    gameConditions.goalTetrisCount = 0;
    gameConditions.currentTetrisCount = 0;
    gameConditions.goalProgress = 0;
    gameConditions.success = FALSE;
}

static void game_conditions_update_success_state(u32 score)
{
    bool met = game_conditions_is_success(score);
    if (met && !gameConditions.success) {
        gameConditions.success = TRUE;
        SOUND_play(SND_TETRIS);
    }
}

void game_conditions_set_from_select(const SelectContext *selectCtx)
{
    if (selectCtx == NULL) return;

    gameConditions.speedLevel = selectCtx->speedLevel;
    gameConditions.garbageFreq = selectCtx->garbageFreq;
    gameConditions.thresholdLRInitial = config.thresholdLRInitial;
    gameConditions.thresholdLRRepeat = config.thresholdLRRepeat;
    gameConditions.thresholdSD = config.thresholdSD;
    gameConditions.ruleFlags = 0;

    if (GET_FLAG(selectCtx->flags, FLAG_SHADOW)) gameConditions.ruleFlags |= GC_RULE_ALLOW_SHADOW;
    if (GET_FLAG(selectCtx->flags, FLAG_HOLD)) gameConditions.ruleFlags |= GC_RULE_ALLOW_HOLD;
    if (GET_FLAG(selectCtx->flags, FLAG_NEXT)) gameConditions.ruleFlags |= GC_RULE_SHOW_NEXT;
    if (GET_FLAG(selectCtx->flags, FLAG_DEBUG)) gameConditions.ruleFlags |= GC_RULE_DEBUG_UI;
    if (selectCtx->randMode != 0) gameConditions.ruleFlags |= GC_RULE_RANDOM_CHAOS;

    if (selectCtx->itemMode != 0) {
        gameConditions.ruleFlags |= GC_RULE_ITEMS_ENABLED;
        if (selectCtx->itemMode == 2) gameConditions.ruleFlags |= GC_RULE_ITEMS_GOOD_ONLY;
        if (selectCtx->itemMode == 3) gameConditions.ruleFlags |= GC_RULE_ITEMS_BAD_ONLY;
    }

    game_conditions_reset_goals();
}

void game_conditions_set_challenge_training(void)
{
    gameConditions.speedLevel = 1;
    gameConditions.garbageFreq = 0;
    gameConditions.thresholdLRInitial = config.thresholdLRInitial;
    gameConditions.thresholdLRRepeat = config.thresholdLRRepeat;
    gameConditions.thresholdSD = config.thresholdSD;
    gameConditions.ruleFlags = GC_RULE_ALLOW_SHADOW | GC_RULE_ALLOW_HOLD | GC_RULE_SHOW_NEXT;

    game_conditions_reset_goals();
    gameConditions.goalFlags = GC_GOAL_TUTORIAL_QUEST;
    gameConditions.goalScore = 1000;
}

void game_conditions_set_challenge_score_training(void)
{
    gameConditions.speedLevel = 1;
    gameConditions.garbageFreq = 0;
    gameConditions.thresholdLRInitial = config.thresholdLRInitial;
    gameConditions.thresholdLRRepeat = config.thresholdLRRepeat;
    gameConditions.thresholdSD = config.thresholdSD;
    gameConditions.ruleFlags = GC_RULE_ALLOW_SHADOW | GC_RULE_ALLOW_HOLD | GC_RULE_SHOW_NEXT;

    game_conditions_reset_goals();
    gameConditions.goalFlags = GC_GOAL_SCORE;
    gameConditions.goalScore = 1000;
}

static void game_conditions_set_challenge_level2(void)
{
    gameConditions.speedLevel = 1;
    gameConditions.garbageFreq = 0;
    gameConditions.thresholdLRInitial = config.thresholdLRInitial;
    gameConditions.thresholdLRRepeat = config.thresholdLRRepeat;
    gameConditions.thresholdSD = config.thresholdSD;
    gameConditions.ruleFlags = GC_RULE_ALLOW_SHADOW | GC_RULE_ALLOW_HOLD | GC_RULE_SHOW_NEXT;

    game_conditions_reset_goals();
    gameConditions.goalFlags = GC_GOAL_CLEARS;
    gameConditions.goalClearCount = 4;
}

static void game_conditions_set_challenge_level3(void)
{
    gameConditions.speedLevel = 1;
    gameConditions.garbageFreq = 0;
    gameConditions.thresholdLRInitial = config.thresholdLRInitial;
    gameConditions.thresholdLRRepeat = config.thresholdLRRepeat;
    gameConditions.thresholdSD = config.thresholdSD;
    gameConditions.ruleFlags = GC_RULE_ALLOW_SHADOW | GC_RULE_ALLOW_HOLD | GC_RULE_SHOW_NEXT;

    game_conditions_reset_goals();
    gameConditions.goalFlags = GC_GOAL_DOUBLES;
    gameConditions.goalDoubleCount = 3;
}

static void game_conditions_set_challenge_level4(void)
{
    gameConditions.speedLevel = 1;
    gameConditions.garbageFreq = 0;
    gameConditions.thresholdLRInitial = config.thresholdLRInitial;
    gameConditions.thresholdLRRepeat = config.thresholdLRRepeat;
    gameConditions.thresholdSD = config.thresholdSD;
    gameConditions.ruleFlags = GC_RULE_ALLOW_SHADOW | GC_RULE_ALLOW_HOLD | GC_RULE_SHOW_NEXT;

    game_conditions_reset_goals();
    gameConditions.goalFlags = GC_GOAL_TETRISES;
    gameConditions.goalTetrisCount = 2;
}

void game_conditions_set_for_challenge_level(u8 levelId)
{
    u8 translated = challenge_translate_grid_to_level(levelId);

    if (translated == CHALLENGE_LEVEL_INVALID) {
        game_conditions_set_challenge_score_training();
        return;
    }

    if (translated == 0 || quest_is_tutorial_level(levelId)) {
        game_conditions_set_challenge_training();
    } else if (translated == 1) {
        game_conditions_set_challenge_level2();
    } else if (translated == CHALLENGE_LEVEL_DOPPELBAU) {
        game_conditions_set_challenge_level3();
    } else if (translated == CHALLENGE_LEVEL_VIERER_CHALLENGE) {
        game_conditions_set_challenge_level4();
    } else {
        game_conditions_set_challenge_score_training();
    }
}

void game_conditions_update_progress_from_score(u32 score)
{
    if (config.runtime.gameMode != GAME_MODE_CHALLENGE) return;

    game_conditions_update_success_state(score);
}

void game_conditions_on_lines_cleared(u16 linesCleared, u32 score)
{
    if (config.runtime.gameMode != GAME_MODE_CHALLENGE) return;

    if ((gameConditions.goalFlags & GC_GOAL_CLEARS) && linesCleared > 0) {
        if (gameConditions.currentClearCount < 9999) {
            gameConditions.currentClearCount++;
        }
    }

    if ((gameConditions.goalFlags & GC_GOAL_DOUBLES) && linesCleared >= 2) {
        if (gameConditions.currentDoubleCount < 9999) {
            gameConditions.currentDoubleCount++;
        }
    }

    if ((gameConditions.goalFlags & GC_GOAL_TETRISES) && linesCleared >= 4) {
        if (gameConditions.currentTetrisCount < 9999) {
            gameConditions.currentTetrisCount++;
        }
    }

    game_conditions_update_success_state(score);
}

bool game_conditions_is_success(u32 score)
{
    bool hasExplicitGoals = FALSE;

    if (gameConditions.goalFlags & GC_GOAL_TUTORIAL_QUEST) {
        hasExplicitGoals = TRUE;
        if ((gameConditions.goalProgress < QUEST_TUTORIAL_STAGE_COUNT) && !gameConditions.success) return FALSE;
    }

    if (gameConditions.goalFlags & GC_GOAL_DOUBLES) {
        hasExplicitGoals = TRUE;
        if (gameConditions.currentDoubleCount < gameConditions.goalDoubleCount) return FALSE;
    }

    if (gameConditions.goalFlags & GC_GOAL_CLEARS) {
        hasExplicitGoals = TRUE;
        if (gameConditions.currentClearCount < gameConditions.goalClearCount) return FALSE;
    }

    if (gameConditions.goalFlags & GC_GOAL_TETRISES) {
        hasExplicitGoals = TRUE;
        if (gameConditions.currentTetrisCount < gameConditions.goalTetrisCount) return FALSE;
    }

    if (gameConditions.goalFlags & GC_GOAL_SCORE) {
        hasExplicitGoals = TRUE;
        if (score < gameConditions.goalScore) return FALSE;
    }

    if (!hasExplicitGoals) return gameConditions.success;
    return TRUE;
}
