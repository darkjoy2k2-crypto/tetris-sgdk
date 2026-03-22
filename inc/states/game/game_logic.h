#pragma once

#include <genesis.h>

// Vorwärtsdeklaration: Sagt dem Compiler, dass es diesen Typ gibt
typedef struct GameContext GameContext; 

#define ITEM_ID_SKULL 10
#define ITEM_ID_HEART 11
#define EFFECT_RAINBOW 10
#define EFFECT_SHADOW_BOARD 11
#define TILE_ID_GARBAGE 8   // Grauer Block
#define TILE_ID_FLASH   11  // Weißer Flash-Block
#define ITEM_ID_NONE 0

#define ITEM_SPAWN_RATE_MIN 2
#define ITEM_SPAWN_RATE_MAX 4
#define ITEM_RATIO_HEART 50 

extern const s8 PIECES[7][4][4][2];
extern const u16 GARBAGE_INTERVALS[];




// Funktionen
bool checkCollision(s16 nx, s16 ny, u16 nr);
void spawnPiece();
void lockPiece();
u16 clearLines();
void finishLineClear();
void update_blinking_animation(); // Blink-Animation vor PENDING
void refillBag();
void performHold();
void addGarbageLine();
void triggerManualSort();
void reset_game_logic();
void calculate_ghost_y();
void update_board_animations(); // Hinzugefügt
bool handle_active_animations(GameContext *ctx);
void set_game_comment(const char* text, u16 duration);
bool tryRotate(u16 newRotation);
