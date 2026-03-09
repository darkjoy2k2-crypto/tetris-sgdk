#pragma once

#include <genesis.h>

#define ITEM_ID_SKULL 10
#define ITEM_ID_HEART 11

extern const s8 PIECES[7][4][4][2];

bool checkCollision(s16 nx, s16 ny, u16 nr);
void spawnPiece();
void lockPiece();
u16 clearLines();
void finishLineClear();
void refillBag();
void performHold();
void addGarbageLine();
bool tryRotate(u16 newRotation);

