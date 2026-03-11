#pragma once

#include <genesis.h>

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

bool checkCollision(s16 nx, s16 ny, u16 nr);
void spawnPiece();
void lockPiece();
u16 clearLines();
void finishLineClear();
void refillBag();
void performHold();
void addGarbageLine();
void triggerManualSort();

bool tryRotate(u16 newRotation);

