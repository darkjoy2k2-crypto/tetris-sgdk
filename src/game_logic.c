#include "game_logic.h"
#include "game_core.h"
#include "game_view.h"
#include "menu_bg.h"
#include "states.h"
#include "sound_manager.h" 
#include <genesis.h>
#include <string.h>

#define ITEM_SPAWN_RATE_MIN 2
#define ITEM_SPAWN_RATE_MAX 4
#define ITEM_RATIO_HEART 50 // 50% Herz, Rest Skull

// ID Definitionen (Müssen mit game_view.c übereinstimmen)
#define ITEM_ID_SKULL 10
#define ITEM_ID_HEART 11

void check_and_update_highscore(u32 score) {
    s16 insertIdx = -1;
    for (u16 i = 0; i < 10; i++) {
        highscores[i].isNew = false;
        if (insertIdx == -1 && score > highscores[i].score) insertIdx = i;
    }
    if (insertIdx != -1) {
        for (u16 i = 9; i > insertIdx; i--) highscores[i] = highscores[i - 1];
        strncpy(highscores[insertIdx].name, config.playerName, 3);
        highscores[insertIdx].name[3] = '\0';
        highscores[insertIdx].score = score;
        highscores[insertIdx].isNew = true;
    }
}

const s8 PIECES[7][4][4][2] = {
    // 0: I-Piece (Hellblau) - Echte 4-Stufen-Rotation
    { 
        {{0,1}, {1,1}, {2,1}, {3,1}}, // R0: Waagerecht
        {{2,0}, {2,1}, {2,2}, {2,3}}, // R1: Senkrecht
        {{3,2}, {2,2}, {1,2}, {0,2}}, // R2: Waagerecht (verschoben)
        {{1,3}, {1,2}, {1,1}, {1,0}}  // R3: Senkrecht (verschoben)
    },
    // 1: O-Piece (Gelb) - Form bleibt Quadrat, aber INDIZES rotieren für das Item!
    { 
        {{1,0}, {2,0}, {2,1}, {1,1}}, // R0: 0=TL, 1=TR, 2=BR, 3=BL
        {{2,0}, {2,1}, {1,1}, {1,0}}, // R1: 0=TR, 1=BR, 2=BL, 3=TL
        {{2,1}, {1,1}, {1,0}, {2,0}}, // R2: 0=BR, 1=BL, 2=TL, 3=TR
        {{1,1}, {1,0}, {2,0}, {2,1}}  // R3: 0=BL, 1=TL, 2=TR, 3=BR
    },
    // 2: T-Piece (Lila)
    { 
        {{1,0}, {0,1}, {1,1}, {2,1}}, {{2,1}, {1,0}, {1,1}, {1,2}}, 
        {{1,2}, {2,1}, {1,1}, {0,1}}, {{0,1}, {1,2}, {1,1}, {1,0}} 
    },
    // 3: S-Piece (Grün)
    { 
        {{1,0}, {2,0}, {0,1}, {1,1}}, {{2,1}, {2,2}, {1,1}, {1,0}}, 
        {{1,2}, {0,2}, {2,1}, {1,1}}, {{0,1}, {0,0}, {1,1}, {1,2}} 
    },
    // 4: Z-Piece (Rot)
    { 
        {{0,0}, {1,0}, {1,1}, {2,1}}, {{2,0}, {2,1}, {1,1}, {1,2}}, 
        {{2,2}, {1,2}, {1,1}, {0,1}}, {{0,2}, {0,1}, {1,1}, {1,0}} 
    },
    // 5: J-Piece (Dunkelblau)
    { 
        {{0,0}, {0,1}, {1,1}, {2,1}}, {{2,0}, {1,0}, {1,1}, {1,2}}, 
        {{2,2}, {2,1}, {1,1}, {0,1}}, {{0,2}, {1,2}, {1,1}, {1,0}} 
    },
    // 6: L-Piece (Orange)
    { 
        {{2,0}, {2,1}, {1,1}, {0,1}}, {{2,2}, {1,2}, {1,1}, {1,0}}, 
        {{0,2}, {0,1}, {1,1}, {2,1}}, {{0,0}, {1,0}, {1,1}, {1,2}} 
    }
};

void addGarbageLine() {
    for (u16 y = 0; y < BOARD_HEIGHT - 1; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            ctx->board[x][y] = ctx->board[x][y + 1];
        }
    }

    u8 randomColor = 1 + (random() % 7);
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

    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        if (x == holeX) {
            ctx->board[x][BOARD_HEIGHT - 1] = 0;
        } else {
            ctx->board[x][BOARD_HEIGHT - 1] = randomColor;
        }
    }

    if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation)) {
        if (!checkCollision(ctx->pieceX, ctx->pieceY - 1, ctx->rotation)) {
            ctx->pieceY--;
        } else {
            SOUND_play(SND_GAME_OVER);
            view_animate_grayscale();
            for(u16 i=0; i<30; i++) SYS_doVBlankProcess();        
            view_fade_out_frame();
            SYS_doVBlankProcess();
            currentState = STATE_GAMEOVER;
        }
    }
    SOUND_play(SND_GARBAGE); 
}

bool tryRotate(u16 newRotation) {
    s16 kicks[] = {0, 1, -1, 2, -2};
    for (u16 i = 0; i < 5; i++) {
        if (!checkCollision(ctx->pieceX + kicks[i], ctx->pieceY, newRotation)) {
            ctx->pieceX += kicks[i];
            ctx->rotation = newRotation;
            return true;
        }
    }
    return false;
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

void triggerGoodEffect() {
    // 1. Negativen Effekt sofort stoppen
    ctx->activeBadEffect = EFFECT_NONE;
    ctx->badEffectTimer = 0;

    // 2. Unterste Zeile (Row 19) löschen und alles nachrücken
    for (s16 y = BOARD_HEIGHT - 1; y > 0; y--) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            ctx->board[x][y] = ctx->board[x][y - 1];
        }
    }
    // Die oberste Zeile leeren
    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        ctx->board[x][0] = 0;
    }

    SOUND_play(SND_GOOD_ITEM);
    
    // UI Refresh erzwingen
    ctx->lastActiveBadEffect = 99;
}

void triggerBadEffect() {
    ctx->activeBadEffect = (random() % 6) + 1; 

    switch(ctx->activeBadEffect) {
        case EFFECT_FULLSPEED:  
        case EFFECT_SAME_TILES: 
            ctx->badEffectTimer = 5;    // 5 Steine
            break;
            
        case EFFECT_NO_ROTATE:  
            ctx->badEffectTimer = 180;  // 3 Sek * 60 Frames
            break;
            
        case EFFECT_REVERSED:   
            ctx->badEffectTimer = 240;  // 4 Sek * 60 Frames
            break;
            
        case EFFECT_HOLD_LOCK:  
        case EFFECT_HIDE_NEXT:  
            ctx->badEffectTimer = 300;  // 5 Sek * 60 Frames
            break;
    }

    SOUND_play(SND_BAD_ITEM);
    
    if (ctx->activeBadEffect == EFFECT_HOLD_LOCK) {
        ctx->holdType = -1; 
        ctx->canHold = false;
    }
}

u16 clearLines() {
    u16 cleared = 0;
    s16 hFound = 0;
    s16 sFound = 0;

    for(u16 y=0; y<BOARD_HEIGHT; y++) ctx->pendingLines[y] = false;

    for (s16 y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool full = true;
        u16 hInRow = 0, sInRow = 0;
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (ctx->board[x][y] == 0) { full = false; break; }
            if (ctx->board[x][y] == ITEM_ID_HEART) hInRow++;
            if (ctx->board[x][y] == ITEM_ID_SKULL) sInRow++;
        }
        if (full) {
            cleared++;
            ctx->pendingLines[y] = true;
            hFound += hInRow;
            sFound += sInRow;
        }
    }

    if (cleared > 0) {
        // --- BALANCE CHECK ---
        s16 balance = hFound - sFound;
        if (balance < 0) {
            triggerBadEffect(); 
        } else if (balance > 0) {
            ctx->score += (balance * 500); 
            // WICHTIG: Nur den Merker setzen!
            ctx->heartTriggered = true; 
        }

        ctx->clearTimer = 8; 
        if (cleared == 4) SOUND_play(SND_TETRIS);
        else SOUND_play(SND_LINE_CLEAR);
        
        ctx->score += (100 * cleared * ctx->level);
        ctx->linesTotal += cleared;
        ctx->level = ctx->startLevel + (ctx->linesTotal / 10);
    }
    return cleared;
}

void finishLineClear() {
    // 1. Volle Zeilen physikalisch löschen (Dein Standard-Code)
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

    // 2. WICHTIG: Herz-Effekt ausführen und Merker RESETTEN
    if (ctx->heartTriggered) {
        triggerGoodEffect();
        ctx->heartTriggered = false; // <-- Das verhindert das Dauer-Feuern!
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
    if (ctx->badEffectTimer > 0 && ctx->activeBadEffect <= 2) {
        ctx->badEffectTimer--;
        if (ctx->badEffectTimer == 0) ctx->activeBadEffect = EFFECT_NONE;
    }

    if (ctx->activeBadEffect != EFFECT_SAME_TILES) {
        ctx->type = ctx->nextType;
        if (config.randMode == 0) {
            ctx->nextType = ctx->bag[ctx->bagIndex];
            ctx->bagIndex++;
            if (ctx->bagIndex >= 7) refillBag();
        } else {
            ctx->nextType = random() % 7;
        }
    }

    ctx->rotation = 0;
    ctx->pieceX = 3;
    ctx->pieceY = 0;
    ctx->canHold = true;

    ctx->itemSlot = -1;
    if (config.itemMode != 0) {
        if (ctx->itemSpawnCounter <= 0) {
            ctx->itemSlot = random() % 4;
            if (config.itemMode == 1) ctx->itemType = (random() % 100 < ITEM_RATIO_HEART) ? ITEM_ID_HEART : ITEM_ID_SKULL;
            else if (config.itemMode == 2) ctx->itemType = ITEM_ID_HEART;
            else if (config.itemMode == 3) ctx->itemType = ITEM_ID_SKULL;
            
            ctx->itemSpawnCounter = (random() % (ITEM_SPAWN_RATE_MAX - ITEM_SPAWN_RATE_MIN + 1)) + ITEM_SPAWN_RATE_MIN;
        } else {
            ctx->itemSpawnCounter--;
        }
    }

    if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation)) {
        SOUND_play(SND_GAME_OVER);
        view_animate_grayscale();
        for(u16 i=0; i<30; i++) SYS_doVBlankProcess();        
        view_fade_out_frame();
        config.currentScore = ctx->score;
        currentState = STATE_GAMEOVER;
    }
}

void lockPiece() {
    bool lockedAbove = false;
    for (u16 i = 0; i < 4; i++) {
        s16 px = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
        s16 py = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];
        
        if (py >= 0) {
            if (i == ctx->itemSlot) ctx->board[px][py] = ctx->itemType;
            else ctx->board[px][py] = ctx->type + 1;
        } else {
            lockedAbove = true;
        }
    }
    SOUND_play(SND_PIECE_LOCK);

    if (lockedAbove) {
        config.currentScore = ctx->score;
        currentState = STATE_GAMEOVER;
    }
}

void performHold() {
    if (!config.allowHold || !ctx->canHold) return;

    // 1. Cursed Hold: Skull triggert sofort
    // Wir prüfen auf < 4, da u16 niemals negativ wird. 
    // Wenn kein Item da ist, sollte itemSlot auf 255 oder 4 stehen.
    if (ctx->itemSlot < 4 && ctx->itemType == ITEM_ID_SKULL) {
        triggerBadEffect();
        ctx->itemSlot = 255; // "Löschen" durch ungültigen Index
        SOUND_play(SND_BAD_ITEM); 
    }

    // --- SAME TILES DEBUFF LOGIK ---
    if (ctx->activeBadEffect == EFFECT_SAME_TILES) {
        if (ctx->holdType != -1) {
            ctx->holdType = -1;      
            ctx->lastHoldType = -2;  
            
            // Fix: Benutze einen Sound, den du sicher hast (z.B. SND_BAD_ITEM)
            // oder ersetze SND_BAD_ITEM durch deinen Bezeichner aus der .res Datei
            SOUND_play(SND_BAD_ITEM); 
        }
        ctx->canHold = false; 
        return; 
    }

    // 2. Normaler Hold-Vorgang
    SOUND_play(SND_HOLD);

    if (ctx->holdType == -1) {
        ctx->holdType = ctx->type;
        ctx->itemSlot = 255; 
        spawnPiece();
    } else {
        s16 temp = ctx->type;
        ctx->type = ctx->holdType;
        ctx->holdType = temp;
        
        ctx->pieceX = 3;
        ctx->pieceY = 0;
        ctx->rotation = 0;
        ctx->itemSlot = 255;
    }
    
    ctx->canHold = false;
}