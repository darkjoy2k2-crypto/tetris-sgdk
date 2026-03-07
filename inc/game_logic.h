#pragma once
#include <genesis.h>

bool checkCollision(s16 nx, s16 ny, u16 nr);
void lockPiece();
u16 clearLines();
void spawnPiece();
void performHold();
void refillBag(); // Neu hinzugefügt