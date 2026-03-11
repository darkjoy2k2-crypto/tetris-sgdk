#include <genesis.h>
#include "states/game/game_core.h"
#include "states/game/game_logic.h"
#include "states/game/game_view.h"
#include "states/game/game_controls.h" // NEU

#include "gfx.h"
#include "sound_manager.h"
#include "states/states.h"
#include "menu_bg.h"
#include "fonts.h"
#include "bg.h"
#include <string.h>

const u16 GRAVITY_SPEEDS[] = { 9999, 60, 30, 15 };
const u16 GARBAGE_INTERVALS[] = { 0, 1200, 600, 300 };

GameContext* ctx = NULL;

// --- REINE LOGIK INITIALISIERUNG ---
void game_init() {
    // 1. Speicher & Context absichern
    if (ctx != NULL) {
        MEM_free(ctx); 
        ctx = NULL;
    }
    ctx = MEM_alloc(sizeof(GameContext));
    memset(ctx, 0, sizeof(GameContext)); 
    
    // 2. System-Zustände setzen
    SOUND_init();
    menu_bg_set_mode(BG_MODE_SPACE);

    // 3. UI Cache-Reset (Zwingt das UI beim ersten Frame zum Zeichnen)
    ctx->lastScore          = 0xFFFFFFFF; 
    ctx->lastLevel          = 0xFFFF;
    ctx->lastLinesNext      = 0xFFFF;
    ctx->lastComboCount     = 0xFFFF;
    ctx->lastActiveBadEffect = 99; 
    ctx->lastBadEffectTimer  = -1;
    ctx->lastNextType       = -2;
    ctx->lastHoldType       = -2;

    // 4. Spiel-Parameter & Level-Logik
    ctx->score = 0;
    if (config.speedLevel == 0)      ctx->startLevel = 1;
    else if (config.speedLevel == 1) ctx->startLevel = 1;
    else if (config.speedLevel == 2) ctx->startLevel = 5;
    else                             ctx->startLevel = 10;

    ctx->level      = ctx->startLevel;
    ctx->linesTotal = 0;
    ctx->moveTimer  = 0;
    ctx->holdType   = -1; // -1 = Kein Stein im Speicher
    ctx->canHold    = true;
    
    // 5. Effekt- & Animations-Initialisierung
    ctx->activeBadEffect = EFFECT_NONE;
    ctx->badEffectTimer  = 0;
    ctx->heartTriggered  = false;
    ctx->sortingRow      = -1; // -1 = Keine Animation aktiv
    ctx->clearTimer      = 0;

    // 6. Garbage-Setup (Zufallswert basierend auf config)
    ctx->garbageTimer = 0;
    if (config.garbageFreq > 0) {
        u16 base = GARBAGE_INTERVALS[config.garbageFreq];
        ctx->garbageNextThreshold = base + (random() % 120) - 60;
    }

    // 7. Piece-Logik (Bag füllen & ersten Stein spawnen)
    refillBag();
    ctx->nextType = ctx->bag[ctx->bagIndex];
    ctx->bagIndex++;
    
    // Berechnet PieceX, PieceY und Rotation für den Start
    spawnPiece();

    // 8. Dirty-Flag für den ersten Frame
    ctx->needsBoardDraw = true; 
}

// --- REINE GRAFIK INITIALISIERUNG ---
void game_init_draw() {
    // Sicherheitscheck: Ohne Context kein Zeichnen
    if (ctx == NULL) return;

    // 1. VRAM Säuberung
    // Wir stellen sicher, dass die Ebenen leer sind, bevor wir neues laden
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    // 2. Grafik-Ressourcen laden
    // Diese Funktion (in game_view.c) lädt das Hintergrundbild (game_bg),
    // die 7 Tetris-Tiles, sowie Skull- und Heart-Tiles in den VRAM.
    load_background(); 

    // 3. Tile-Cache Initialisierung
    // Setzt das tileCache[10][20] Array komplett auf 0xFFFF.
    // Das zwingt die erste drawBoard() Funktion, wirklich jedes Feld einmal zu malen.
    view_init_cache(); 

    // 4. Paletten-Setup für UI und Text
    // Wir laden die Standard-Schrift-Palette in Slot 3
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    // Und sagen dem VDP, dass VDP_drawText standardmäßig PAL3 nutzen soll
    VDP_setTextPalette(PAL3);

    // 5. Visueller Start-Effekt
    // Da load_background die Palette oft erst auf Schwarz setzt, 
    // triggern wir hier das weiche Einblenden des Spielfelds.
    view_fade_in_frame();  
}

void game_update() {
    if (ctx == NULL) return;

    // --- PHASE 1: LINE CLEAR (BLOCKIEREND) ---
    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            if (ctx->sortingRow == -1) spawnPiece();
        }
        ctx->needsBoardDraw = true;
        lastJoyState = joyState; 
        return; 
    }

    // --- PHASE 2: SORTIER-ANIMATIONEN (BLOCKIEREND) ---
    if (ctx->sortingRow != -1) {
        u16 y = ctx->sortingRow;
        if (ctx->activeBadEffect == EFFECT_RAINBOW) {
            u8 rowColor = (y % 7) + 1;
            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                if (ctx->board[x][y] != 0 && ctx->board[x][y] < 10) ctx->board[x][y] = rowColor;
            }
        } 
        else if (ctx->activeBadEffect == EFFECT_SHADOW_BOARD) {
            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                if (ctx->board[x][y] != 0 && ctx->board[x][y] < 10) ctx->board[x][y] = 8;
            }
        }
        else {
            u8 tempRow[10]; u16 filled = 0;
            for (u16 x = 0; x < BOARD_WIDTH; x++) if (ctx->board[x][y] != 0) tempRow[filled++] = ctx->board[x][y];
            for (u16 x = filled; x < BOARD_WIDTH; x++) ctx->board[x][y] = tempRow[x];
            for (u16 x = filled; x < BOARD_WIDTH; x++) ctx->board[x][y] = 0;
        }
        ctx->sortingRow++;
        if (ctx->sortingRow >= BOARD_HEIGHT) {
            ctx->sortingRow = -1;
            if (ctx->activeBadEffect == EFFECT_RAINBOW || ctx->activeBadEffect == EFFECT_SHADOW_BOARD) ctx->activeBadEffect = EFFECT_NONE;
            spawnPiece();
        }
        ctx->needsBoardDraw = true;
        lastJoyState = joyState; 
        return; 
    }

    // --- PHASE 3: CONTROLS (Das neue Skript) ---
    bool moved = controls_update(ctx);

    // --- PHASE 4: GRAVITATION ---
    u16 vBtnSoftDrop = (ctx->activeBadEffect == EFFECT_REVERSED) ? BUTTON_LEFT : BUTTON_DOWN;
    ctx->moveTimer++;
    
    s16 threshold = 60 - ((ctx->level - 1) * 3);
    if (threshold < 2) threshold = 3;
    if (ctx->activeBadEffect == EFFECT_FULLSPEED) threshold = 3;

    u16 finalThreshold;
    if (joyState & vBtnSoftDrop) {
        finalThreshold = (u16)(threshold / 12); 
        if (finalThreshold < 4) finalThreshold = 4;
    } 
    else if (ctx->activeBadEffect == EFFECT_FREEZE) {
        finalThreshold = 9999; 
    } 
    else {
        finalThreshold = (u16)threshold;
    }

    if (ctx->moveTimer >= finalThreshold) {
        if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
            moved = true;
            if (joyState & vBtnSoftDrop) {
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

    // --- PHASE 5: GARBAGE-LOGIK ---
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

    // --- PHASE 6: SCHATTEN-UPDATE ---
    if (moved && config.showShadow) {
        ctx->ghostY = ctx->pieceY;
        while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) {
            ctx->ghostY++;
        }
    }

    // --- PHASE 7: EFFECT TIMER ---
    if (ctx->badEffectTimer > 0 && ctx->activeBadEffect >= 3) {
        ctx->badEffectTimer--;
        if (ctx->badEffectTimer == 0) {
            ctx->activeBadEffect = EFFECT_NONE;
            SOUND_play(SND_GOOD_ITEM);
        }
    }

    // --- FINALE ---
    if (moved) ctx->needsBoardDraw = true;
    lastJoyState = joyState; 
} // <--- DIESE Klammer muss die Einzige am Ende der Funktion sein!

void game_draw() {

    if (ctx == NULL) return;

    if (ctx->needsBoardDraw) {
        drawBoard(); 
        
        ctx->needsBoardDraw = false; 
    }

    view_update_ui(ctx); 
}

void game_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearPlane(BG_A, TRUE);
}

