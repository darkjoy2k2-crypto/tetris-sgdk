#pragma once
#include <genesis.h>

// Zeichnet das komplette Spielfeld inklusive Schatten (Ghost Piece)
void drawBoard();

// Zeichnet eine kleine Vorschau (für Next oder Hold Fenster)
void drawPreview(s16 type, u16 x, u16 y);

// Zeichnet die statischen UI-Elemente wie "SCORE", "NEXT" etc.
void initUI();