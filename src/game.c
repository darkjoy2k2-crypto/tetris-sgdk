#include <genesis.h>
#include "game_core.h"
#include "game_logic.h"
#include "game_view.h"
#include "gfx.h"
#include "sound_manager.h"
#include "states.h"
#include "menu_bg.h"
#include "fonts.h"
#include "bg.h"
#include <string.h>

const u16 GRAVITY_SPEEDS[] = { 9999, 60, 30, 15 };
const u16 GARBAGE_INTERVALS[] = { 0, 1200, 600, 300 };

GameContext* ctx = NULL;

void game_init() {
    menu_bg_set_mode(BG_MODE_SPACE);
    if (ctx != NULL) MEM_free(ctx); 
    ctx = MEM_alloc(sizeof(GameContext));
    memset(ctx, 0, sizeof(GameContext)); 
    
    SOUND_init();
    load_background();

    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    VDP_setTextPalette(PAL3);

    // Cache-Reset für UI
    ctx->lastScore = 0xFFFFFFFF; 
    ctx->lastLevel = 0xFFFF;
    ctx->lastLinesNext = 0xFFFF;
    ctx->lastComboCount = 0xFFFF;
    ctx->lastActiveBadEffect = 99; // Erzwingt Refresh
    ctx->lastBadEffectTimer = -1;
    ctx->lastNextType = -2;
    ctx->lastHoldType = -2;

    view_init_cache(); 

    ctx->score = 0;
    // Level-Setup
    if (config.speedLevel == 0) ctx->startLevel = 1;
    else if (config.speedLevel == 1) ctx->startLevel = 1;
    else if (config.speedLevel == 2) ctx->startLevel = 5;
    else ctx->startLevel = 10;

    ctx->level = ctx->startLevel;
    ctx->linesTotal = 0;
    ctx->moveTimer = 0;
    ctx->holdType = -1;
    ctx->canHold = true;
    
    // Effekt-Initialisierung
    ctx->activeBadEffect = EFFECT_NONE;
    ctx->badEffectTimer = 0;

    // Garbage-Setup
    ctx->garbageTimer = 0;
    if (config.garbageFreq > 0) {
        u16 base = GARBAGE_INTERVALS[config.garbageFreq];
        ctx->garbageNextThreshold = base + (random() % 120) - 60;
    }
ctx->heartTriggered = false; // Sicherstellen, dass beim Start alles auf Null ist
    ctx->sortingRow = -1;    refillBag();
    ctx->nextType = ctx->bag[ctx->bagIndex];
    ctx->bagIndex++;
    spawnPiece();
    drawBoard();
    view_fade_in_frame();  
}

void game_update() {
if (ctx == NULL) return;

    // --- PHASE 1: LINE CLEAR ANIMATION ---
    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            // Nur spawnen, wenn KEINE Sortierung gestartet wurde
            if (ctx->sortingRow == -1) spawnPiece();
        }
        drawBoard();
        return;
    }

    // --- PHASE 2: SORTIER ANIMATION (Herz-Effekt) ---
    if (ctx->sortingRow != -1) {
        u16 y = ctx->sortingRow;
        u8 tempRow[10];
        u16 filled = 0;

        // Zeile kompaktieren (alle Blöcke nach links schieben)
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (ctx->board[x][y] != 0) {
                tempRow[filled++] = ctx->board[x][y];
            }
        }
        for (u16 x = 0; x < filled; x++) ctx->board[x][y] = tempRow[x];
        for (u16 x = filled; x < BOARD_WIDTH; x++) ctx->board[x][y] = 0;

        ctx->sortingRow++;
        
        // Wenn alle Zeilen (0-19) durch sind
        if (ctx->sortingRow >= BOARD_HEIGHT) {
            ctx->sortingRow = -1;
            spawnPiece(); // Jetzt erst darf der nächste Stein kommen
        }
        
        drawBoard();
        return; // Frame beenden, während sortiert wird
    }



    // --- INPUT MAPPING PHASE ---
    u16 currentJoy = joyState;
    u16 changed = joyState & ~lastJoyState;

    // Virtuelle Rollen zuweisen
    u16 vBtnLeft     = BUTTON_LEFT;
    u16 vBtnRight    = BUTTON_RIGHT;
    u16 vBtnSoftDrop = BUTTON_DOWN;
    u16 vBtnHardDrop = BUTTON_UP;
    u16 vBtnRotCCW    = BUTTON_A; // Uhrzeigersinn
    u16 vBtnRotCW   = BUTTON_B; // Gegen Uhrzeigersinn

    // EFFEKT: Tetris Party Wii Style Mapping
    if (ctx->activeBadEffect == EFFECT_REVERSED) {
        vBtnLeft     = BUTTON_B;     // Links auf B
        vBtnRight    = BUTTON_A;     // Rechts auf A
        vBtnRotCW    = BUTTON_UP;    // Oben rotiere rechts
        vBtnRotCCW   = BUTTON_DOWN;  // Unten rotiere links
        vBtnSoftDrop = BUTTON_LEFT;  // Links Soft Drop
        vBtnHardDrop = BUTTON_RIGHT; // Rechts Harddrop
    }

    bool moved = false;

    // --- BEWEGUNG (DAS) ---
    const u16 dasDelay = 6;
    const u16 dasRepeat = 2;
    u16 currentDir = 0;

    if (currentJoy & vBtnLeft) currentDir = vBtnLeft;
    else if (currentJoy & vBtnRight) currentDir = vBtnRight;

    if (currentDir != 0) {
        if (changed & currentDir) {
            s16 step = (currentDir == vBtnLeft) ? -1 : 1;
            if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                ctx->pieceX += step;
                moved = true;
                SOUND_play(SND_MOVE);
            }
            ctx->dasTimer = 0;
            ctx->dasDir = currentDir;
        } else if (ctx->dasDir == currentDir) {
            ctx->dasTimer++;
            if (ctx->dasTimer >= dasDelay) {
                if ((ctx->dasTimer - dasDelay) % dasRepeat == 0) {
                    s16 step = (currentDir == vBtnLeft) ? -1 : 1;
                    if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                        ctx->pieceX += step;
                        moved = true;
                        SOUND_play(SND_MOVE);
                    }
                }
            }
        }
    } else {
        ctx->dasTimer = 0;
        ctx->dasDir = 0;
    }

    // --- ROTATION ---
    if (changed & (vBtnRotCW | vBtnRotCCW)) {
        if (ctx->activeBadEffect == EFFECT_NO_ROTATE) {
            // Bestrafung: Stein fällt 1 Feld tiefer
            if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
                ctx->pieceY++;
                moved = true;
            }
            SOUND_play(SND_BAD_ITEM); // Fehler-Sound
        } else {
            // (ctx->rotation + 3) % 4 ist CCW, (ctx->rotation + 1) % 4 ist CW
            u16 nr = (changed & vBtnRotCW) ? (ctx->rotation + 1) % 4 : (ctx->rotation + 3) % 4;
            
            s16 kicks[] = {0, 1, -1, 2, -2};
            for(u8 i=0; i<5; i++) {
                if (!checkCollision(ctx->pieceX + kicks[i], ctx->pieceY, nr)) {
                    ctx->pieceX += kicks[i];
                    ctx->rotation = nr;
                    moved = true;
                    SOUND_play(SND_ROTATE);
                    break;
                }
            }
        }
    }

    // --- HOLD ---
    if (config.allowHold && (changed & BUTTON_C)) {
        if (ctx->activeBadEffect != EFFECT_HOLD_LOCK) {
            performHold();
            moved = true; 
        } else {
            SOUND_play(SND_BAD_ITEM);
        }
    }

    // --- HARD DROP ---
    if (changed & vBtnHardDrop) {
        SOUND_play(SND_HARD_DROP);
        while (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
        }
        lockPiece();
        if (clearLines() == 0) {
            spawnPiece();
            moved = true;
        }
    } 
    
    // --- GRAVITATION / SOFT DROP ---
    else {
        ctx->moveTimer++;
        
        s16 threshold = 60 - ((ctx->level - 1) * 3);
        if (threshold < 2) threshold = 2;
        if (ctx->activeBadEffect == EFFECT_FULLSPEED) threshold = 2;

        u16 finalThreshold;
        if (currentJoy & vBtnSoftDrop) {
            finalThreshold = (u16)(threshold / 12); 
            if (finalThreshold < 4) finalThreshold = 4;
        } else {
            finalThreshold = (u16)threshold;
        }

        if (ctx->moveTimer >= finalThreshold) {
            if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
                ctx->pieceY++;
                moved = true;
                if (currentJoy & vBtnSoftDrop) {
                    ctx->score++;
                    SOUND_play(SND_SOFT_DROP);
                }
            } else {
                lockPiece();
                if (clearLines() == 0) {
                    spawnPiece();
                    moved = true;
                }
            }
            ctx->moveTimer = 0;
        }
    }

    // Garbage-Logik
    if (config.garbageFreq > 0 && ctx->clearTimer == 0) {
        ctx->garbageTimer++;
        if (ctx->garbageTimer >= ctx->garbageNextThreshold) {
            addGarbageLine();
            ctx->garbageTimer = 0;
            u16 base = GARBAGE_INTERVALS[config.garbageFreq];
            ctx->garbageNextThreshold = base + (random() % 120) - 60;
            moved = true;
        }
    }

    // Schatten-Update
    if (moved && config.showShadow) {
        ctx->ghostY = ctx->pieceY;
        while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) {
            ctx->ghostY++;
        }
    }

    // Timer Update (Zeit-basierte Effekte)
    if (ctx->badEffectTimer > 0 && ctx->activeBadEffect >= 3) {
        ctx->badEffectTimer--;
        if (ctx->badEffectTimer == 0) ctx->activeBadEffect = EFFECT_NONE;
    }

    drawBoard();
}

void game_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearPlane(BG_A, TRUE);
}