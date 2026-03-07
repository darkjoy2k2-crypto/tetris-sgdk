#include "game_logic.h"
#include "game_core.h"
#include "states.h"
#include "sound_manager.h" // WICHTIG: Den Manager inkludieren
#include <genesis.h>
#include <string.h>

void check_and_update_highscore(u32 finalScore) {
    int insertPos = -1;

    // 1. Finde heraus, ob der Score gut genug ist
    for (int i = 0; i < 10; i++) {
        if (finalScore > highscores[i].score) {
            insertPos = i;
            break;
        }
    }

    // 2. Wenn ja, schiebe alles ab insertPos eins nach unten
    if (insertPos != -1) {
        for (int i = 9; i > insertPos; i--) {
            highscores[i].score = highscores[i-1].score;
            strncpy(highscores[i].name, highscores[i-1].name, 4);
        }

        // 3. Neuen Score eintragen
        highscores[insertPos].score = finalScore;
        strncpy(highscores[insertPos].name, config.playerName, 3);
        highscores[insertPos].name[3] = '\0';
    }
}

// Definition des Tetrimino-Arrays
const s8 PIECES[7][4][4][2] = {
    {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}}, // I
    {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}}, // O
    {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}}, // T
    {{{1,1},{2,1},{0,2},{1,2}}, {{1,0},{1,1},{2,1},{2,2}}, {{1,1},{2,1},{0,2},{1,2}}, {{1,0},{1,1},{2,1},{2,2}}}, // S
    {{{0,1},{1,1},{1,2},{2,2}}, {{2,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{1,2},{2,2}}, {{2,0},{1,1},{2,1},{1,2}}}, // Z
    {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}}, // J
    {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}}, {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}}  // L
};

void addGarbageLine() {
    // 1. Das gesamte Board eine Zeile nach oben schieben
    for (u16 y = 0; y < BOARD_HEIGHT - 1; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            ctx->board[x][y] = ctx->board[x][y + 1];
        }
    }

    // 2. Zufällige Farbe für diese spezifische Zeile wählen (ID 1 bis 7)
    u8 randomColor = 1 + (random() % 7);

    // 3. Die ideale Lücke finden (wo darüber ein Block ist)
    u16 holeX = random() % BOARD_WIDTH; 
    u16 candidates[BOARD_WIDTH];
    u16 count = 0;

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        if (ctx->board[x][BOARD_HEIGHT - 2] != 0) {
            candidates[count++] = x;
        }
    }

    if (count > 0) {
        holeX = candidates[random() % count];
    }

    // 4. Die unterste Zeile mit der Zufallsfarbe füllen
    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        if (x == holeX) {
            ctx->board[x][BOARD_HEIGHT - 1] = 0; // Das Loch
        } else {
            ctx->board[x][BOARD_HEIGHT - 1] = randomColor; // Zufällige Farbe
        }
    }

    // 5. Kollisions-Check für den aktiven Stein
    if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation)) {
        if (!checkCollision(ctx->pieceX, ctx->pieceY - 1, ctx->rotation)) {
            ctx->pieceY--;
        } else {
            SOUND_play(SND_GAME_OVER);
            currentState = STATE_GAMEOVER;
        }
    }
    
    // SOUND: Ein passender Sound für das Hochschieben
    // Vielleicht ein etwas tieferer Ton (z.B. ID 8 oder 9 aus deinem Sound-Test)
    SOUND_play(SND_GARBAGE); 
}

bool checkCollision(s16 nx, s16 ny, u16 nr) {
    for (u16 i = 0; i < 4; i++) {
        s16 px = nx + PIECES[ctx->type][nr][i][0];
        s16 py = ny + PIECES[ctx->type][nr][i][1];
        if (px < 0 || px >= BOARD_WIDTH || py >= BOARD_HEIGHT) return true;
        if (py >= 0 && ctx->board[px][py] != 0) return true;
    }
    return false;
}

void lockPiece() {
    for (u16 i = 0; i < 4; i++) {
        s16 px = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
        s16 py = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];
        if (py >= 0) ctx->board[px][py] = ctx->type + 1;
    }
    // SOUND: Der Stein rastet ein
    SOUND_play(SND_PIECE_LOCK);
}

u16 clearLines() {
    u16 cleared = 0;
    u16 oldLevel = ctx->level; // Merken für Level-Up Sound

    for(u16 y=0; y<BOARD_HEIGHT; y++) ctx->pendingLines[y] = false;

    for (s16 y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool full = true;
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (ctx->board[x][y] == 0) { full = false; break; }
        }
        if (full) {
            cleared++;
            ctx->pendingLines[y] = true;
        }
    }

    if (cleared > 0) {
        ctx->clearTimer = 8; 
        
        // SOUND: Unterscheidung zwischen normalem Clear und Tetris
        if (cleared == 4) {
            SOUND_play(SND_TETRIS);
        } else {
            SOUND_play(SND_LINE_CLEAR);
        }
        
        // SOUND: Combo-Sound ab der zweiten gelöschten Reihe in Folge
        if (ctx->comboCount >= 1) {
            SOUND_play(SND_COMBO);
        }

        u32 points = 0;
        bool isTetris = (cleared == 4);
        if (cleared == 1) { points = 100; strncpy(ctx->lastComment, "SINGLE", 20); }
        else if (cleared == 2) { points = 300; strncpy(ctx->lastComment, "DOUBLE!!", 20); }
        else if (cleared == 3) { points = 500; strncpy(ctx->lastComment, "TRIPLE!!!", 20); }
        else if (isTetris) {
            points = 800;
            if (ctx->b2bActive) { points = (points * 3) / 2; strncpy(ctx->lastComment, "B2B TETRIS!", 20); }
            else strncpy(ctx->lastComment, "TETRIS!!!!", 20);
            ctx->b2bActive = true;
        }
        if (!isTetris) ctx->b2bActive = false;
        
        if (ctx->comboCount > 0) {
            points += (50 * ctx->comboCount * ctx->level);
            if (ctx->comboCount >= 2) sprintf(ctx->lastComment, "COMBO X%d", ctx->comboCount);
        }

        ctx->score += points * ctx->level;
        ctx->linesTotal += cleared;
        ctx->level = 1 + (ctx->linesTotal / 10);
        ctx->comboCount++;
        ctx->commentTimer = 60;

        // SOUND: Level-Up Check
        if (ctx->level > oldLevel) {
            SOUND_play(SND_LEVEL_UP);
        }
    } else {
        ctx->comboCount = 0;
    }

    return cleared;
}

void finishLineClear() {
    for (s16 y = BOARD_HEIGHT - 1; y >= 0; y--) {
        if (ctx->pendingLines[y]) {
            for (s16 yy = y; yy > 0; yy--) {
                for (u16 x = 0; x < BOARD_WIDTH; x++) {
                    ctx->board[x][yy] = ctx->board[x][yy - 1];
                    ctx->pendingLines[yy] = ctx->pendingLines[yy - 1];
                }
            }
            for (u16 x = 0; x < BOARD_WIDTH; x++) ctx->board[x][0] = 0;
            ctx->pendingLines[0] = false;
            y++; 
        }
    }
}

void refillBag() {
    for (u8 i = 0; i < 7; i++) ctx->bag[i] = i;
    for (u8 i = 6; i > 0; i--) {
        u8 j = random() % (i + 1);
        u8 temp = ctx->bag[i];
        ctx->bag[i] = ctx->bag[j];
        ctx->bag[j] = temp;
    }
    ctx->bagIndex = 0;
}

void spawnPiece() {
    ctx->type = ctx->nextType;

    if (config.randMode == 0) {
        // --- FAIR MODE (7-Bag) ---
        ctx->nextType = ctx->bag[ctx->bagIndex];
        ctx->bagIndex++;
        if (ctx->bagIndex >= 7) refillBag();
    } else {
        // --- CHAOS MODE (Purer Zufall) ---
        // Hier kann theoretisch 10x der gleiche Stein kommen
        ctx->nextType = random() % 7;
    }

    ctx->rotation = 0;
    ctx->pieceX = 3;
    ctx->pieceY = 0;
    ctx->canHold = true;

    if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation)) {
        SOUND_play(SND_GAME_OVER);
        config.currentScore = ctx->score;
        currentState = STATE_GAMEOVER;
    }
}

void performHold() {
    if (!config.allowHold) return; 

    if (!ctx->canHold) return;
    // SOUND: Hold Aktion
    SOUND_play(SND_HOLD);

    if (ctx->holdType == -1) {
        ctx->holdType = ctx->type;
        spawnPiece();
    } else {
        s16 temp = ctx->type;
        ctx->type = ctx->holdType;
        ctx->holdType = temp;
        ctx->pieceX = 3;
        ctx->pieceY = 0;
        ctx->rotation = 0;
    }
    ctx->canHold = false;
}