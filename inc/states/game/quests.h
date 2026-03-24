#pragma once

#include <genesis.h>

#define QUEST_ACT_MOVE_LR      1
#define QUEST_ACT_ROTATE_AB    2
#define QUEST_ACT_SOFTDROP     3
#define QUEST_ACT_HOLD_STORE   4
#define QUEST_ACT_HARDDROP     5
#define QUEST_ACT_HOLD_RECALL  6

#define QUEST_TUTORIAL_STAGE_COUNT 6

bool quest_is_tutorial_level(u8 levelId);
void quest_on_action(u16 action);

const char* quest_get_stage_line(u16 stage, u16 line);
