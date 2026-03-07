#include "game_core.h"
#include "game_logic.h"
#include "game_view.h"
#include "gfx.h"
#include "sound_manager.h" // WICHTIG: Sound-Manager inkludieren

GameContext* ctx = NULL;

void game_init() {
    ctx = MEM_alloc(sizeof(GameContext));
    memset(ctx->board, 0, sizeof(ctx->board));
    
    // Sound-System starten
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
    ctx->garbageTimer = 0;
    // 10 bis 20 Sekunden bei 60 FPS = 600 bis 1200 Frames
    ctx->garbageNextThreshold = 600 + (random() % 601);    refillBag();
    ctx->nextType = ctx->bag[ctx->bagIndex];
    ctx->bagIndex++;
    spawnPiece();

    VDP_clearTextArea(0, 0, 40, 28);
    
    // Optional: Hier die Musik starten
    // SOUND_playMusic();
}

void game_update() {
    if (ctx == NULL) return;

    const u16 delay = 6;
    const u16 repeat = 2;

    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            spawnPiece();
        }
        drawBoard();
        return;
    }

    u16 joy = JOY_readJoypad(JOY_1);
    static u16 lastJoy = 0;
    u16 changed = joy & ~lastJoy;
    lastJoy = joy;

    // --- SEITLICHE BEWEGUNG ---
    u16 currentDir = 0;
    if (joy & BUTTON_LEFT) currentDir = BUTTON_LEFT;
    else if (joy & BUTTON_RIGHT) currentDir = BUTTON_RIGHT;

    if (currentDir != 0) {
        if (changed & currentDir) {
            s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
            if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                ctx->pieceX += step;
                SOUND_play(SND_MOVE); // SOUND: Einzelschritt
            }
            ctx->dasTimer = 0;
            ctx->dasDir = currentDir;
        } else if (ctx->dasDir == currentDir) {
            ctx->dasTimer++;
            if (ctx->dasTimer >= delay) {
                if ((ctx->dasTimer - delay) % repeat == 0) {
                    s16 step = (currentDir == BUTTON_LEFT) ? -1 : 1;
                    if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                        ctx->pieceX += step;
                        SOUND_play(SND_MOVE); // SOUND: Auto-Repeat-Schritt
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
        u16 nr = (ctx->rotation + 3) % 4; 
        if (!checkCollision(ctx->pieceX, ctx->pieceY, nr)) {
            ctx->rotation = nr;
            SOUND_play(SND_ROTATE); // SOUND: Drehung links
        }
    }
    if (changed & BUTTON_B) { 
        u16 nr = (ctx->rotation + 1) % 4; 
        if (!checkCollision(ctx->pieceX, ctx->pieceY, nr)) {
            ctx->rotation = nr;
            SOUND_play(SND_ROTATE); // SOUND: Drehung rechts
        }
    }

    // Hold (Sound ist bereits in performHold() integriert)
    if (changed & BUTTON_C) performHold();
    
    // --- HARD DROP ---
    if (changed & BUTTON_UP) {
        SOUND_play(SND_HARD_DROP); // SOUND: Aufschlag oben triggern
        while (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) ctx->pieceY++;
        lockPiece();
        if (clearLines() == 0) spawnPiece();
    }

    // --- SCHWERKRAFT / SOFT DROP ---
    ctx->moveTimer++;
    s16 threshold = 30 - ((ctx->level - 1) * 3);
    if (threshold < 2) threshold = 2;
    u16 finalThreshold = (joy & BUTTON_DOWN) ? 2 : (u16)threshold;

    if (ctx->moveTimer >= finalThreshold) {
        if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
            // Nur Sound spielen, wenn der Spieler aktiv nach unten drückt
            if (joy & BUTTON_DOWN) SOUND_play(SND_SOFT_DROP); 
        } else {
            lockPiece();
            if (clearLines() == 0) spawnPiece();
        }
        ctx->moveTimer = 0;
    }
// Timer nur laufen lassen, wenn keine Animation aktiv ist
if (ctx->clearTimer == 0) {
    ctx->garbageTimer++;
    
    if (ctx->garbageTimer >= ctx->garbageNextThreshold) {
        addGarbageLine();
        
        // Timer zurücksetzen und neues Intervall würfeln
        ctx->garbageTimer = 0;
        ctx->garbageNextThreshold = 600 + (random() % 601);
    }
}

    drawBoard();
}

void game_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    SOUND_stopMusic(); // Musik stoppen
    VDP_clearTextArea(0, 0, 40, 28);
}