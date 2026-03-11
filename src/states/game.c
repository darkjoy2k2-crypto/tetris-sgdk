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

static bool handle_active_animations(GameContext* ctx) {
    // --- PHASE 1: LINE CLEAR ---
    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            if (ctx->sortingRow == -1) spawnPiece();
        }
        ctx->needsBoardDraw = true;
        return true; // Blockiert
    }

    // --- PHASE 2: SORTIEREN ---
    if (ctx->sortingRow != -1) {
        u16 y = ctx->sortingRow;
        // Rainbow/Shadow/Sort Logik (hier deine ~25 Zeilen einfügen)
        // ...
        ctx->sortingRow++;
        if (ctx->sortingRow >= BOARD_HEIGHT) {
            ctx->sortingRow = -1;
            if (ctx->activeBadEffect == EFFECT_RAINBOW || ctx->activeBadEffect == EFFECT_SHADOW_BOARD) 
                ctx->activeBadEffect = EFFECT_NONE;
            spawnPiece();
        }
        ctx->needsBoardDraw = true;
        return true; // Blockiert
    }
    return false; // Nichts blockiert, weiter im Text
}

static bool handle_gravity(GameContext* ctx) {
    u16 vBtnSoftDrop = (ctx->activeBadEffect == EFFECT_REVERSED) ? BUTTON_LEFT : BUTTON_DOWN;
    bool moved = false;
    
    ctx->moveTimer++;
    s16 threshold = 60 - ((ctx->level - 1) * 3);
    if (threshold < 2) threshold = 3;
    if (ctx->activeBadEffect == EFFECT_FULLSPEED) threshold = 3;

    u16 finalThreshold = (joyState & vBtnSoftDrop) ? (threshold / 12) : 
                        ((ctx->activeBadEffect == EFFECT_FREEZE) ? 9999 : threshold);
    if (finalThreshold < 4 && (joyState & vBtnSoftDrop)) finalThreshold = 4;

    if (ctx->moveTimer >= finalThreshold) {
        if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
            moved = true;
            if (joyState & vBtnSoftDrop) { ctx->score++; SOUND_play(SND_SOFT_DROP); }
        } else {
            lockPiece();
            if (clearLines() == 0) spawnPiece();
            moved = true;
        }
        ctx->moveTimer = 0;
    }
    return moved;
}

static void handle_environment(GameContext* ctx, bool moved) {
    // Garbage
    if (config.garbageFreq > 0 && ctx->clearTimer == 0) {
        ctx->garbageTimer++;
        if (ctx->garbageTimer >= ctx->garbageNextThreshold) {
            addGarbageLine();
            ctx->garbageTimer = 0;
            ctx->garbageNextThreshold = GARBAGE_INTERVALS[config.garbageFreq] + (random() % 120) - 60;
            ctx->needsBoardDraw = true;
        }
    }

    // Shadow
    if (moved && config.showShadow) {
        ctx->ghostY = ctx->pieceY;
        while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) ctx->ghostY++;
    }

    // Effect Timer
    if (ctx->badEffectTimer > 0 && ctx->activeBadEffect >= 3) {
        ctx->badEffectTimer--;
        if (ctx->badEffectTimer == 0) {
            ctx->activeBadEffect = EFFECT_NONE;
            SOUND_play(SND_GOOD_ITEM);
        }
    }
}

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

void game_init_draw() {
    // Sicherheitscheck: Ohne Context kein Zeichnen
    if (ctx == NULL) return;

    // 1. VRAM Säuberung
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    // 2. Grafik-Ressourcen laden
    load_background(); 

    // 3. Tile-Cache Initialisierung
    view_init_cache(); 

    // 4. Paletten-Setup für UI und Text
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    VDP_setTextPalette(PAL3);

    // 5. Visueller Start-Effekt
    view_fade_in_frame();  
}

void game_update() {
    if (ctx == NULL) return;

    // 1. Animationen prüfen (und ggf. abbrechen)
    if (handle_active_animations(ctx)) {
        lastJoyState = joyState;
        return;
    }

    // 2. Eingaben verarbeiten (dein bereits ausgelagertes Skript)
    bool moved = controls_update(ctx);

    // 3. Gravitation berechnen
    if (handle_gravity(ctx)) moved = true;

    // 4. Garbage, Schatten und Timer
    handle_environment(ctx, moved);

    // 5. Finaler Check für den Draw-Frame
    if (moved) ctx->needsBoardDraw = true;
    
    lastJoyState = joyState; 
}

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