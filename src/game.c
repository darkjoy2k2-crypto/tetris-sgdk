#include <genesis.h>
#include "game_core.h"
#include "game_logic.h"
#include "game_view.h"
#include "gfx.h"
#include "sound_manager.h"
#include "states.h" // WICHTIG: Zugriff auf config

GameContext* ctx = NULL;

// Hilfs-Arrays für die Intervalle basierend auf deinen Menü-Optionen
// Speed: None (9999), Slow (60), Med (30), Fast (15)
const u16 GRAVITY_SPEEDS[] = { 9999, 60, 30, 15 };

// Garbage: None (0), Slow (1200), Med (600), Fast (300)
const u16 GARBAGE_INTERVALS[] = { 0, 1200, 600, 300 };

void game_init() {
    menu_bg_set_active(false);
    ctx = MEM_alloc(sizeof(GameContext));
    memset(ctx, 0, sizeof(GameContext)); // Gesamten Context nullen
    
    SOUND_init();
    gfx_init();

    ctx->score = 0;
    ctx->level = 1;
    ctx->linesTotal = 0;
    ctx->moveTimer = 0;
    ctx->holdType = -1;
    ctx->canHold = true;
    ctx->comboCount = 0;
    ctx->b2bActive = false;
    ctx->commentTimer = 0;
    memset(ctx->lastComment, 0, sizeof(ctx->lastComment));

    // Garbage-Initialisierung basierend auf Config
    ctx->garbageTimer = 0;
    if (config.garbageFreq > 0) {
        // Wir nehmen den Basiswert und addieren etwas Zufall (± 1 Sekunde)
        u16 base = GARBAGE_INTERVALS[config.garbageFreq];
        ctx->garbageNextThreshold = base + (random() % 120) - 60;
    }

    // Bag-System vorbereiten
    refillBag();
    
    // Ersten Stein vorbereiten
    ctx->nextType = ctx->bag[ctx->bagIndex];
    ctx->bagIndex++;
    
    // Spiel starten
    spawnPiece();

    VDP_clearTextArea(0, 0, 40, 28);
}

void game_update() {
    if (ctx == NULL) return;

    // --- PAUSE BEI LÖSCH-ANIMATION ---
    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            spawnPiece();
        }
        drawBoard();
        return;
    }

    // Input lesen
    u16 joy = JOY_readJoypad(JOY_1);
    static u16 lastJoy = 0;
    u16 changed = joy & ~lastJoy;
    lastJoy = joy;

    // --- SEITLICHE BEWEGUNG (DAS - Delayed Auto Shift) ---
    const u16 dasDelay = 10;
    const u16 dasRepeat = 3;

    u16 currentDir = 0;
    if (joy & BUTTON_LEFT) currentDir = BUTTON_LEFT;
    else if (joy & BUTTON_RIGHT) currentDir = BUTTON_RIGHT;

    if (currentDir != 0) {
        if (changed & currentDir) {
            s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
            if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                ctx->pieceX += step;
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
    if (changed & BUTTON_A) { 
        u16 nr = (ctx->rotation + 3) % 4; // Links
        if (!checkCollision(ctx->pieceX, ctx->pieceY, nr)) {
            ctx->rotation = nr;
            SOUND_play(SND_ROTATE);
        }
    }
    if (changed & BUTTON_B) { 
        u16 nr = (ctx->rotation + 1) % 4; // Rechts
        if (!checkCollision(ctx->pieceX, ctx->pieceY, nr)) {
            ctx->rotation = nr;
            SOUND_play(SND_ROTATE);
        }
    }

    // --- HOLD ---
    if (changed & BUTTON_C) performHold();
    
    // --- HARD DROP ---
    if (changed & BUTTON_UP) {
        SOUND_play(SND_HARD_DROP);
        while (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
        }
        lockPiece();
        if (clearLines() == 0) spawnPiece();
    }

    // --- SCHWERKRAFT / SOFT DROP (SPEED-CONFIG) ---
    ctx->moveTimer++;

    // Basis-Geschwindigkeit aus Config holen
    s16 baseThreshold = (s16)GRAVITY_SPEEDS[config.speedLevel];
    
    // Mit steigendem Level wird es schneller (außer bei Speed: None)
    s16 threshold = baseThreshold;
    if (config.speedLevel > 0) {
        threshold = baseThreshold - ((ctx->level - 1) * 4);
    }
    
    // Untergrenze einhalten (Minimum 2 Frames pro Fall)
    if (threshold < 2) threshold = 2;

    // Soft Drop überschreibt das Intervall
    u16 finalThreshold = (joy & BUTTON_DOWN) ? 2 : (u16)threshold;

    if (ctx->moveTimer >= finalThreshold) {
        if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
            if (joy & BUTTON_DOWN) SOUND_play(SND_SOFT_DROP); 
        } else {
            lockPiece();
            if (clearLines() == 0) spawnPiece();
        }
        ctx->moveTimer = 0;
    }

    // --- GARBAGE LOGIK (GARBAGE-CONFIG) ---
    if (config.garbageFreq > 0 && ctx->clearTimer == 0) {
        ctx->garbageTimer++;
        
        if (ctx->garbageTimer >= ctx->garbageNextThreshold) {
            addGarbageLine();
            
            // Timer zurücksetzen und neues Intervall würfeln (± 1 Sekunde Variation)
            ctx->garbageTimer = 0;
            u16 base = GARBAGE_INTERVALS[config.garbageFreq];
            ctx->garbageNextThreshold = base + (random() % 120) - 60;
        }
    }

    drawBoard();
}

void game_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    SOUND_stopMusic();
    VDP_clearTextArea(0, 0, 40, 28);
}