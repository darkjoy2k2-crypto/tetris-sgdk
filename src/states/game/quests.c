#include "states/game/quests.h"

#include "states/challenge.h"
#include "states/game/game_conditions.h"
#include "states/game/game_logic.h"
#include "sound_manager.h"
#include "states/states.h"

static const char* const questStageLines[QUEST_TUTORIAL_STAGE_COUNT][4] = {
    {"Try to move", "", "Push LEFT", "Push RIGHT"},
    {"Try rotate", "", "A for left", "B for right"},
    {"Try softdrop", "", "Push DOWN", "Hold DOWN"},
    {"Try hold", "", "Push C", "Store tile"},
    {"Try harddrop", "", "Push UP", "Fast drop"},
    {"Take from hold", "", "Push C", "Swap back"}
};

bool quest_is_tutorial_level(u8 levelId)
{
    return levelId == CHALLENGE_TUTORIAL_ENTRY_ID;
}

void quest_on_action(u16 action)
{
    if (config.runtime.gameMode != GAME_MODE_CHALLENGE) return;
    if ((gameConditions.goalFlags & GC_GOAL_TUTORIAL_QUEST) == 0) return;
    if (gameConditions.success) return;

    if (gameConditions.goalProgress == 0 && action == QUEST_ACT_MOVE_LR) {
        gameConditions.goalProgress = 1;
        set_game_comment("2/6 ROTATE A/B", 90);
        return;
    }
    if (gameConditions.goalProgress == 1 && action == QUEST_ACT_ROTATE_AB) {
        gameConditions.goalProgress = 2;
        set_game_comment("3/6 SOFTDROP", 90);
        return;
    }
    if (gameConditions.goalProgress == 2 && action == QUEST_ACT_SOFTDROP) {
        gameConditions.goalProgress = 3;
        set_game_comment("4/6 HOLD C", 90);
        return;
    }
    if (gameConditions.goalProgress == 3 && action == QUEST_ACT_HOLD_STORE) {
        gameConditions.goalProgress = 4;
        set_game_comment("5/6 HARDDROP", 90);
        return;
    }
    if (gameConditions.goalProgress == 4 && action == QUEST_ACT_HARDDROP) {
        gameConditions.goalProgress = 5;
        set_game_comment("6/6 TAKE HOLD C", 90);
        return;
    }
    if (gameConditions.goalProgress == 5 && action == QUEST_ACT_HOLD_RECALL) {
        gameConditions.goalProgress = 6;
        gameConditions.success = TRUE;
        set_game_comment("START TO EXIT", 120);
        SOUND_play(SND_TETRIS);
    }
}

const char* quest_get_stage_line(u16 stage, u16 line)
{
    if (stage >= QUEST_TUTORIAL_STAGE_COUNT || line >= 4) return "";
    return questStageLines[stage][line];
}
