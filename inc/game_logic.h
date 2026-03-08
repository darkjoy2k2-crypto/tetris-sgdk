#ifndef _GAME_LOGIC_H_
#define _GAME_LOGIC_H_

#include <genesis.h>

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

#endif