#include <genesis.h>
#include "game_core.h"
#include "game_logic.h"
#include "game_view.h"
#include "gfx.h"
#include "sound_manager.h"
#include "states.h"
#include "menu_bg.h"
#include <string.h>

// --- DEKLARATIONEN (Damit der Compiler weiß, dass diese existieren) ---
// Diese Arrays müssen irgendwo definiert sein (meistens game_core.c oder game.c oben)
const u16 GRAVITY_SPEEDS[] = { 9999, 60, 30, 15 };
const u16 GARBAGE_INTERVALS[] = { 0, 1200, 600, 300 };

// Context Pointer (aus game_core.h)
GameContext* ctx = NULL;

void game_init() {
    menu_bg_set_active(false);

    if (ctx != NULL) MEM_free(ctx); 
    ctx = MEM_alloc(sizeof(GameContext));
    memset(ctx, 0, sizeof(GameContext)); 
    
    SOUND_init();
    gfx_init();

    // Cache-Werte für die Performance-Heilung
    ctx->lastScore = 0xFFFFFFFF; 
    ctx->lastLevel = 0xFFFF;
    ctx->lastNextType = -2;
    ctx->lastHoldType = -2;

    // Diese Funktion muss in game_view.h deklariert sein!
    view_init_cache(); 

    ctx->score = 0;
    ctx->level = 1;
    ctx->linesTotal = 0;
    ctx->moveTimer = 0;
    ctx->holdType = -1;
    ctx->canHold = true;
    
    // Garbage Initialisierung
    ctx->garbageTimer = 0;
    if (config.garbageFreq > 0) {
        u16 base = GARBAGE_INTERVALS[config.garbageFreq];
        ctx->garbageNextThreshold = base + (random() % 120) - 60;
    }

    refillBag();
    ctx->nextType = ctx->bag[ctx->bagIndex];
    ctx->bagIndex++;
    
    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL0);
    VDP_drawText("SCORE:", 1, 1);
    VDP_drawText("LEVEL:", 1, 3);

    spawnPiece();
    drawBoard();
}

void game_update() {
    if (ctx == NULL) return;

    // 1. ANIMATIONS-PAUSE (Line Clear)
    // Wenn Zeilen gelöscht werden, steht die Spiellogik still.
    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            spawnPiece();
            // Nach dem Spawn Schatten einmalig berechnen
            ctx->ghostY = ctx->pieceY;
            while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) {
                ctx->ghostY++;
            }
        }
        drawBoard();
        return;
    }

    // 2. INPUT & BEWEGUNG
    u16 changed = joyState & ~lastJoyState;
    bool moved = false;

    // --- Seitliche Bewegung (DAS) ---
    const u16 dasDelay = 10;
    const u16 dasRepeat = 3;
    u16 currentDir = 0;

    if (joyState & BUTTON_LEFT) currentDir = BUTTON_LEFT;
    else if (joyState & BUTTON_RIGHT) currentDir = BUTTON_RIGHT;

    if (currentDir != 0) {
        if (changed & currentDir) {
            s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
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
                    s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
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

    // --- Rotation ---
    if (changed & (BUTTON_A | BUTTON_B)) {
        u16 nr = (changed & BUTTON_A) ? (ctx->rotation + 3) % 4 : (ctx->rotation + 1) % 4;
        if (!checkCollision(ctx->pieceX, ctx->pieceY, nr)) {
            ctx->rotation = nr;
            moved = true;
            SOUND_play(SND_ROTATE);
        }
    }

    // --- Hold ---
    if (config.allowHold && (changed & BUTTON_C)) {
        performHold();
        moved = true; 
    }

    // --- Hard Drop ---
    if (changed & BUTTON_UP) {
        SOUND_play(SND_HARD_DROP);
        while (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
        }
        lockPiece();
        if (clearLines() == 0) {
            spawnPiece();
            moved = true; // Schatten für neuen Stein berechnen
        }
        // Bei Hard Drop springen wir hier raus, drawBoard() erfolgt am Ende
    } else {
        // --- Schwerkraft & Soft Drop ---
        ctx->moveTimer++;
        s16 baseThreshold = (s16)GRAVITY_SPEEDS[config.speedLevel];
        s16 threshold = baseThreshold;
        if (config.speedLevel > 0) {
            threshold = baseThreshold - ((ctx->level - 1) * 4);
        }
        if (threshold < 2) threshold = 2;

        u16 finalThreshold = (joyState & BUTTON_DOWN) ? 2 : (u16)threshold;

        if (ctx->moveTimer >= finalThreshold) {
            if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
                ctx->pieceY++;
                moved = true;
                if (joyState & BUTTON_DOWN) SOUND_play(SND_SOFT_DROP);
            } else {
                // Stein setzt auf -> Prüfung auf volle Zeilen NUR hier
                lockPiece();
                if (clearLines() == 0) {
                    spawnPiece();
                    moved = true;
                }
            }
            ctx->moveTimer = 0;
        }
    }

    // 3. SCHATTEN NUR BEI RELEVANZ AKTUALISIEREN
    if (moved && config.showShadow) {
        ctx->ghostY = ctx->pieceY;
        while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) {
            ctx->ghostY++;
        }
    }

    // 4. GARBAGE LOGIK
    if (config.garbageFreq > 0 && ctx->clearTimer == 0) {
        ctx->garbageTimer++;
        if (ctx->garbageTimer >= ctx->garbageNextThreshold) {
            addGarbageLine();
            ctx->garbageTimer = 0;
            u16 base = GARBAGE_INTERVALS[config.garbageFreq];
            ctx->garbageNextThreshold = base + (random() % 120) - 60;
            moved = true; // Da sich das Board verschiebt, Schatten neu berechnen
        }
    }

    // 5. RENDERING
    drawBoard();
}

void game_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    // Falls SOUND_stopMusic nicht existiert, nutze XGM_stopPlay() oder ähnliches
    XGM_stopPlay(); 
    VDP_clearTextArea(0, 0, 40, 28);
}