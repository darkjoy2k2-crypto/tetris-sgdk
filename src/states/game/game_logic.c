#include <genesis.h>
#include <string.h>
#include "states/game/game_logic.h"
#include "states/game/game_core.h"
#include "states/game/game_view.h"
#include "menu_bg.h"
#include "states/states.h"
#include "sound_manager.h"
#include "sounds.h"
#include "sprite.h"

static bool highscoreUpdated = false;

void triggerBadEffect();
static void handle_item_spawn_logic();
void play_game_over_animation();
static void handle_rainbow_row(u16 y);
static void handle_shadow_row(u16 y);
static void handle_sort_row(u16 y);

// Hier das PIECES Array einfügen (wie gehabt)
const s8 PIECES[7][4][4][2] = {
    // ... deine 7 Pieces ...
    { {{0,1}, {1,1}, {2,1}, {3,1}}, {{2,0}, {2,1}, {2,2}, {2,3}}, {{3,2}, {2,2}, {1,2}, {0,2}}, {{1,3}, {1,2}, {1,1}, {1,0}} },
    { {{1,0}, {2,0}, {2,1}, {1,1}}, {{2,0}, {2,1}, {1,1}, {1,0}}, {{2,1}, {1,1}, {1,0}, {2,0}}, {{1,1}, {1,0}, {2,0}, {2,1}} },
    { {{1,0}, {0,1}, {1,1}, {2,1}}, {{2,1}, {1,0}, {1,1}, {1,2}}, {{1,2}, {2,1}, {1,1}, {0,1}}, {{0,1}, {1,2}, {1,1}, {1,0}} },
    { {{1,0}, {2,0}, {0,1}, {1,1}}, {{2,1}, {2,2}, {1,1}, {1,0}}, {{1,2}, {0,2}, {2,1}, {1,1}}, {{0,1}, {0,0}, {1,1}, {1,2}} },
    { {{0,0}, {1,0}, {1,1}, {2,1}}, {{2,0}, {2,1}, {1,1}, {1,2}}, {{2,2}, {1,2}, {1,1}, {0,1}}, {{0,2}, {0,1}, {1,1}, {1,0}} },
    { {{0,0}, {0,1}, {1,1}, {2,1}}, {{2,0}, {1,0}, {1,1}, {1,2}}, {{2,2}, {2,1}, {1,1}, {0,1}}, {{0,2}, {1,2}, {1,1}, {1,0}} },
    { {{2,0}, {2,1}, {1,1}, {0,1}}, {{2,2}, {1,2}, {1,1}, {1,0}}, {{0,2}, {0,1}, {1,1}, {2,1}}, {{0,0}, {1,0}, {1,1}, {1,2}} }
};
// Sicherstellen, dass dies oben in game_logic.c steht:
static u8 sortBuffer[200];



void triggerManualSort() {
    if (ctx == NULL || ctx->sortingRow != -1) return;

    // 1. Das gesamte Board in den Puffer kopieren
    // memcpy nutzt auf dem 68k optimierte MOVE.L Befehle für 32-Bit Transfers
    memcpy(sortBuffer, ctx->board, 200);

    // 2. Selection Sort: Wir füllen das Board von UNTEN (19) nach OBEN (0)
    for (s16 targetY = 19; targetY >= 0; targetY--) {
        u16 maxBlocksIndex = 0;
        u16 maxBlocksCount = 0;

        // Suche im verbleibenden oberen Teil die vollste Reihe
        for (u16 currentY = 0; currentY <= (u16)targetY; currentY++) {
            u16 currentCount = 0;
            // Manueller Zeilen-Offset mit Bitshifts (y * 10)
            u16 rowOffset = (currentY << 3) + (currentY << 1);
            
            for (u16 x = 0; x < 10; x++) {
                if (sortBuffer[rowOffset + x] != 0) {
                    currentCount++;
                }
            }

            // Stabilität: >= sorgt dafür, dass bei gleicher Blockanzahl die 
            // untere Reihe bevorzugt wird (weniger Sprünge)
            if (currentCount >= maxBlocksCount) {
                maxBlocksCount = currentCount;
                maxBlocksIndex = currentY;
            }
        }

        // Tausch der Reihen im Puffer (10 Bytes pro Zeile)
        u16 targetOffset = (targetY << 3) + (targetY << 1);
        u16 maxOffset    = (maxBlocksIndex << 3) + (maxBlocksIndex << 1);

        // Nur tauschen, wenn es nicht dieselbe Zeile ist
        if (targetOffset != maxOffset) {
            for (u16 x = 0; x < 10; x++) {
                u8 temp = sortBuffer[targetOffset + x];
                sortBuffer[targetOffset + x] = sortBuffer[maxOffset + x];
                sortBuffer[maxOffset + x] = temp;
            }
        }
    }

    // 3. Animation-Trigger
    // Die game_update wird nun Frame für Frame ctx->board aus sortBuffer füllen
    ctx->sortingRow = 0; 
    SOUND_play(SND_TETRIS);
}

bool checkCollision(s16 nx, s16 ny, u16 nr) {
    for (u16 i = 0; i < 4; i++) {
        s16 gx = nx + PIECES[ctx->type][nr][i][0];
        s16 gy = ny + PIECES[ctx->type][nr][i][1];

        // Wand- und Boden-Check (Immer aktiv)
        if (gx < 0 || gx >= 10 || gy >= 20) return true;

        // Board-Check: Nur wenn der Block im sichtbaren Bereich (0-19) ist
        if (gy >= 0) {
            if (get_board_tile(gx, gy) != 0) return true;
        }
    }
    return false;
}

void calculate_ghost_y() {
    ctx->ghostY = ctx->pieceY;
    while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) ctx->ghostY++;
}


void refillBag() {
    for (u8 i = 0; i < 7; i++) ctx->bag[i] = i;
    for (u8 i = 6; i > 0; i--) {
        u8 j = random() % (i + 1);
        u8 temp = ctx->bag[i]; ctx->bag[i] = ctx->bag[j]; ctx->bag[j] = temp;
    }
    ctx->bagIndex = 0;
}

static void trigger_multiclear() {
    // CLEARLINE+: Randomly clear 1-4 non-full lines with animation
    KLog("TRIGGER_MULTICLEAR: Start");
    
    u16 nonFullLines[20];
    u16 nonFullCount = 0;
    
    // Find all non-full lines
    for (u16 y = 0; y < 20; y++) {
        bool full = false;
        u16 rowOffset = (y << 3) + (y << 1);
        
        for (u16 x = 0; x < 10; x++) {
            if (ctx->board[rowOffset + x] == 0) {
                full = false;
                break;
            }
            full = true;
        }
        
        if (!full) {
            nonFullLines[nonFullCount++] = y;
        }
    }
    
    if (nonFullCount == 0) {
        KLog("TRIGGER_MULTICLEAR: No non-full lines available");
        // One-shot effect fallback: release slot immediately on no-op.
        if (ctx->activeBadEffect == EFFECT_MULTIPLIER) {
            ctx->activeBadEffect = EFFECT_NONE;
            ctx->badEffectTimer = 0;
            ctx->lastActiveBadEffect = 99;
        }
        return;
    }
    
    // Randomly select 1-4 lines
    u16 linesToClear = (random() % 4) + 1;  // 1 to 4
    if (linesToClear > nonFullCount) linesToClear = nonFullCount;
    
    KLog_U1("TRIGGER_MULTICLEAR: Selected lines:", linesToClear);
    
    // Backup and mark selected lines for clearing
    ctx->clearingLineMask = 0;
    for (u16 i = 0; i < linesToClear; i++) {
        u16 randomIdx = random() % nonFullCount;
        u16 y = nonFullLines[randomIdx];
        u16 rowOffset = (y << 3) + (y << 1);
        
        // Backup blocks
        for (u16 x = 0; x < 10; x++) {
            ctx->clearingLineBackup[rowOffset + x] = ctx->board[rowOffset + x];
        }
        
        // Mark for clearing
        ctx->clearingLineMask |= (1U << y);
        KLog_U1("TRIGGER_MULTICLEAR: Line marked:", y);
        
        // Remove this line from pool
        nonFullLines[randomIdx] = nonFullLines[nonFullCount - 1];
        nonFullCount--;
    }
    
    // Start blink animation
    ctx->clearTimer = GET_TICKS(20);
    SOUND_play(52);
    ctx->boardFlags |= GF_NEEDS_DRAW;
    
    KLog("TRIGGER_MULTICLEAR: Animation started");
}

static void handle_item_spawn_logic() {
    if (config.itemMode == 0) {
        ctx->itemSlot = -1;
        ctx->itemType = ITEM_ID_NONE;
        return;
    }
    if (ctx->itemSpawnCounter <= 0) {
        ctx->itemSlot = random() % 4;
        if (config.itemMode == 1) ctx->itemType = (random() % 100 < ITEM_RATIO_HEART) ? ITEM_ID_HEART : ITEM_ID_SKULL;
        else ctx->itemType = (config.itemMode == 2) ? ITEM_ID_HEART : ITEM_ID_SKULL;
        ctx->itemSpawnCounter = (random() % 2) + 1;
    } else {
        ctx->itemSlot = -1;
        ctx->itemType = ITEM_ID_NONE;
        ctx->itemSpawnCounter--;
    }
}

void transfer_piece_to_board() {
    u16 i;
    for (i = 0; i < 4; i++) {
        s16 gx = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
        s16 gy = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];

        if (is_within_board(gx, gy)) {
            // Prüfung: Ist dieser spezifische Block der Träger des Items?
            // Wenn ja, schreibe die Item-ID (ctx->itemType), sonst die Steinfarbe.
            u8 tileValue = (i == ctx->itemSlot) ? ctx->itemType : (ctx->type + 1);
            
            set_board_tile(gx, gy, tileValue);
        }
    }
    
    ctx->boardFlags |= GF_NEEDS_DRAW;
}

static void apply_scoring(u16 lines) {
    const u16 basePoints[] = { 0, 100, 300, 500, 800 };
    u16 gain = basePoints[lines] * ctx->level;
    if (ctx->comboCount > 0) gain += (50 * ctx->comboCount * ctx->level);
    
    ctx->score += gain;
    ctx->linesTotal += lines;

    u16 oldLevel = ctx->level;
    ctx->level = ctx->startLevel + (ctx->linesTotal / 10);

    if (ctx->level > oldLevel) {
        menu_bg_set_intensity(1);
    } else {
        menu_bg_set_intensity((ctx->linesTotal % 10) + 1);
    }
}



void set_game_comment(const char* text, u16 duration) {
    strncpy(ctx->lastComment, text, 20);
    ctx->lastComment[19] = '\0';
    ctx->commentTimer = duration;
}

void triggerGoodEffect() {
    KLog("TRIGGER_GOOD_EFFECT: Start");
    if (ctx == NULL) {
        KLog("TRIGGER_GOOD_EFFECT: Error - Context is NULL");
        return;
    }

    // Single-slot effect system: do not overwrite an already active effect.
    if (ctx->activeBadEffect != EFFECT_NONE) {
        KLog_U1("TRIGGER_GOOD_EFFECT: Skipped, effect already active:", ctx->activeBadEffect);
        return;
    }

    u16 roll = random() % 5;  // 5 good effects
    KLog_U1("TRIGGER_GOOD_EFFECT: Roll for effect (0-4):", roll);
    
    if (roll == 0) {
        // I-RAIN: Spawn 5 I-pieces from top
        KLog("TRIGGER_GOOD_EFFECT: I-RAIN - 5 pieces");
        ctx->activeBadEffect = EFFECT_I_RAIN; 
        ctx->badEffectTimer = DUR_I_RAIN_SPAWNS; 
        set_game_comment("I-RAIN!", 90); 
    }
    else if (roll == 1) { 
        // SORT BOARD: Sort lines by hole density
        KLog("TRIGGER_GOOD_EFFECT: SORT BOARD - Reorganizing lines");
        triggerManualSort();
        set_game_comment("SORTED!", 90); 
    }
    else if (roll == 2) {
        // RAINBOW: Colorize all board blocks with rainbow colors
        KLog("TRIGGER_GOOD_EFFECT: RAINBOW - Board colors rainbow");
        ctx->activeBadEffect = EFFECT_RAINBOW; 
        ctx->badEffectTimer = 0; 
        ctx->sortingRow = 0;
        set_game_comment("RAINBOW!", 90); 
    }
    else if (roll == 3) { 
        // FREEZE: No gravity for 5 seconds
        KLog_U1("TRIGGER_GOOD_EFFECT: FREEZE - Ticks:", DUR_FREEZE_TICKS);
        ctx->activeBadEffect = EFFECT_FREEZE; 
        ctx->badEffectTimer = DUR_FREEZE_TICKS; 
        set_game_comment("FROZEN!", 90); 
    }
    else { 
        // CLEARLINE+: Clear 1-4 random lines with animation
        KLog("TRIGGER_GOOD_EFFECT: CLEARLINE+ - Clear up to 4 random lines");
        ctx->activeBadEffect = EFFECT_MULTIPLIER;  // Use multiplier effect code
        ctx->badEffectTimer = 1;  // Trigger immediate clear
        set_game_comment("CLEARED!", 90);
        trigger_multiclear();  // Execute the clear
    }

    // Clean up Heart-Items from board
    for (u16 i = 0; i < 200; i++) {
        if (ctx->board[i] == ITEM_ID_HEART) {
            ctx->board[i] = 1 + (random() % 7);
        }
    }

    SOUND_play(SND_GOOD_ITEM);
    ctx->commentTimer = 60;
    ctx->boardFlags |= GF_NEEDS_DRAW;
    KLog("TRIGGER_GOOD_EFFECT: Finished");
}


void triggerBadEffect() {
    KLog("TRIGGER_BAD_EFFECT: Start");
    if (ctx == NULL) {
        KLog("TRIGGER_BAD_EFFECT: Error - Context is NULL");
        return;
    }

    // Single-slot debuff system: do not overwrite an already active bad effect.
    if (ctx->activeBadEffect != EFFECT_NONE) {
        KLog_U1("TRIGGER_BAD_EFFECT: Skipped, effect already active:", ctx->activeBadEffect);
        return;
    }

    u16 roll = random() % 7;  // 7 bad effects
    bool playGenericBadSound = true;
    KLog_U1("TRIGGER_BAD_EFFECT: Roll for effect (0-6):", roll);

    switch(roll) {
        case 0:
            // NOROTATION: Can't rotate for 5 seconds
            ctx->activeBadEffect = EFFECT_NO_ROTATE;
            ctx->badEffectTimer = DUR_NO_ROTATE_TICKS;
            KLog_U1("TRIGGER_BAD_EFFECT: NOROTATION - Ticks:", ctx->badEffectTimer);
            set_game_comment("NO ROTATE!", 90); 
            break;

        case 1:
            // CONFUSION: Reversed controls for 5 seconds
            ctx->activeBadEffect = EFFECT_REVERSED;
            ctx->badEffectTimer = DUR_REVERSED_TICKS;
            KLog_U1("TRIGGER_BAD_EFFECT: CONFUSION - Ticks:", ctx->badEffectTimer);
            set_game_comment("CONFUSED!", 90); 
            break;

        case 2:
            // HIGHSPEED: Pieces fall fast for 5 spawns
            ctx->activeBadEffect = EFFECT_FULLSPEED;
            ctx->badEffectTimer = GET_TICKS(120) + DUR_FULLSPEED_SPAWNS;
            playGenericBadSound = false;
            KLog_U1("TRIGGER_BAD_EFFECT: HIGHSPEED - Warning+Spawns:", ctx->badEffectTimer);
            set_game_comment("FAST!", 90); 
            break;

        case 3:
            // SAMETILE: Forced same piece type for 5 spawns
            ctx->activeBadEffect = EFFECT_SAME_TILES; 
            ctx->badEffectTimer = DUR_SAME_TILES_SPAWNS;
            ctx->forcedPieceType = random() % 7; 
            KLog_U2("TRIGGER_BAD_EFFECT: SAMETILE - Type:", ctx->forcedPieceType, "Spawns:", ctx->badEffectTimer);
            set_game_comment("SAME!", 90); 
            break;

        case 4:
            // NOHOLD: Hold disabled for 5 seconds
            ctx->activeBadEffect = EFFECT_HOLD_LOCK;
            ctx->badEffectTimer = DUR_HOLD_LOCK_TICKS;
            ctx->lastHoldType = ctx->holdType; 
            ctx->holdType = -1;                
            ctx->flags &= ~GF_CAN_HOLD;        
            KLog_U1("TRIGGER_BAD_EFFECT: NOHOLD - Ticks:", ctx->badEffectTimer);
            set_game_comment("HOLD LOCK!", 90);
            break;

        case 5:
            // NONEXT: Next piece hidden for 5 seconds
            ctx->activeBadEffect = EFFECT_HIDE_NEXT;
            ctx->badEffectTimer = DUR_HIDE_NEXT_TICKS;
            KLog_U1("TRIGGER_BAD_EFFECT: NONEXT - Ticks:", ctx->badEffectTimer);
            set_game_comment("NO NEXT!", 90); 
            break;

        case 6:
            // LIGHTSOUT: All blocks black for 5 seconds, then rainbow
            ctx->activeBadEffect = EFFECT_SHADOW_BOARD;
            ctx->badEffectTimer = DUR_SHADOW_TICKS;
            ctx->sortingRow = 0;
            KLog_U1("TRIGGER_BAD_EFFECT: LIGHTSOUT - Ticks:", ctx->badEffectTimer);
            set_game_comment("FADE!", 90); 
            break;
            
        default:
            KLog_U1("TRIGGER_BAD_EFFECT: Warning - Unhandled Effect ID:", roll);
            break;
    }

    if (playGenericBadSound) {
        SOUND_play(SND_BAD_ITEM);
    }
    ctx->commentTimer = 60;
    ctx->lastActiveBadEffect = 99; 
    
    KLog("TRIGGER_BAD_EFFECT: Finished");
}



void lockPiece() {
    KLog("LOCK_PIECE: Start");
    bool lockedAbove = false;
    u16 i;

    KLog("LOCK_PIECE: Calling transfer_piece_to_board...");
    transfer_piece_to_board(); 

    for (i = 0; i < 4; i++) {
        s16 gy = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];
        s16 gx = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];

        if (!is_within_board(gx, gy) && gy < 0) {
            KLog_U2("LOCK_PIECE: Piece locked outside upper boundary! X:", gx, "Y:", gy);
            lockedAbove = true;
            break; 
        }
    }

    SOUND_play(SND_PIECE_LOCK);

    if (lockedAbove) {
        KLog("LOCK_PIECE: Game Over condition met (lockedAbove).");
        SOUND_play(SND_GAME_OVER);
        play_game_over_animation();
    } else {
        KLog("LOCK_PIECE: Piece secured. Checking for full lines...");
        clearLines(); // Scannt & setzt clearTimer (Phase 4)
        
        // Spawn sofortiger neuer Stein oben (Phase 4)
        KLog("LOCK_PIECE: Spawning next piece immediately.");
        spawnPiece();
    }
    KLog("LOCK_PIECE: Finished");
}


void check_and_update_highscore(u32 score) {
    if (highscoreUpdated) return; // Eintrag schon gemacht

    s16 insertIdx = -1;
    
    // 1. Prüfen, ob der Score für die Top 10 reicht
    for (u16 i = 0; i < 10; i++) {
        config.highscores[i].isNew = 0; // Alle alten "Neu"-Markierungen löschen
        if (insertIdx == -1 && score > config.highscores[i].score) {
            insertIdx = (s16)i;
        }
    }

    // 2. Wenn ein Platz gefunden wurde, Liste nach unten schieben
    if (insertIdx != -1) {
        for (u16 i = 9; i > (u16)insertIdx; i--) {
            config.highscores[i] = config.highscores[i - 1];
        }

        // 3. Neuen Eintrag setzen
        strncpy(config.highscores[insertIdx].name, config.playerName, 3);
        config.highscores[insertIdx].name[3] = '\0';
        config.highscores[insertIdx].score = score;
        config.highscores[insertIdx].isNew = 1; // Markierung für Highscore-Screen
    }

    highscoreUpdated = true;
}

bool tryRotate(u16 newRotation) {
    // Die klassischen 5 Test-Positionen (0=Original, 1=Rechts, 2=Links, 3=2xRechts, 4=2xLinks)
    s16 kicks[] = {0, 1, -1, 2, -2};

    for (u16 i = 0; i < 5; i++) {
        if (!checkCollision(ctx->pieceX + kicks[i], ctx->pieceY, newRotation)) {
            ctx->pieceX += kicks[i];
            ctx->rotation = newRotation;
            
            // Wenn der Schatten aktiv ist, müssen wir ihn neu berechnen
            if (GET_FLAG(config.flags, FLAG_SHADOW)) calculate_ghost_y();


            return true; // Rotation erfolgreich
        }
    }
    return false; // Rotation blockiert
}




u16 clearLines() {
    KLog("CLEAR_LINES: Start");
    u16 linesCleared = 0;
    ctx->clearingLineMask = 0; // Reset blink mask

    for (u16 y = 0; y < 20; y++) {
        bool full = true;
        u16 h = 0, s = 0;
        
        u16 rowOffset = (y << 3) + (y << 1);

        for (u16 x = 0; x < 10; x++) {
            u8 t = ctx->board[rowOffset + x];
            
            if (t == 0) { 
                full = false; 
                break; 
            }
            if (t == ITEM_ID_HEART) h++;
            else if (t == ITEM_ID_SKULL) s++;
        }

        if (full) {
            linesCleared++;
            KLog_U1("CLEAR_LINES: Row is full. Y:", y);

            // Speichere die Original-Blöcke für die Blink-Animation
            for (u16 x = 0; x < 10; x++) {
                ctx->clearingLineBackup[rowOffset + x] = ctx->board[rowOffset + x];
            }

            // Markiere diese Zeile in der Blink-Maske (2 Sekunden Animation)
            ctx->clearingLineMask |= (1U << y);

            // Bestimme Effekt basierend auf Items (für später)
            if (h == 0 && s == 0) {
                KLog("CLEAR_LINES: Regular line detected.");
            }
            else if (h > s) { 
                ctx->flags |= GF_HEART_TRIG; 
                KLog_U2("CLEAR_LINES: Heart dominance. Count:", h, "Flags:", ctx->flags);
            }   
            else if (s > h) { 
                ctx->flags |= GF_SKULL_TRIG; 
                KLog_U2("CLEAR_LINES: Skull dominance. Count:", s, "Flags:", ctx->flags);
            }   
            else {
                KLog("CLEAR_LINES: Balanced items.");
            }

            // WICHTIG: Board NICHT ändern während Blink-Animation!
            // Blocks bleiben sichtbar damit sie zweimal blinken
        }
    }

    if (linesCleared > 0) {
        KLog_U2("CLEAR_LINES: Lines cleared:", linesCleared, "Combo:", ctx->comboCount + 1);
        // Timer für Blink-Animation: 2x blinken = ~20 Frames
        ctx->clearTimer = GET_TICKS(20); 

        // Sound direkt beim Start der Einfärb-/Blinkanimation
        SOUND_play(52 + ctx->comboCount + 1);

        ctx->boardFlags |= GF_NEEDS_DRAW;
    } else {
        ctx->comboCount = 0;
    }

    KLog_U1("CLEAR_LINES: Finished. Total cleared:", linesCleared);
    return linesCleared;
}

void play_game_over_animation() {
    u16 x, y, i;

    for (y = 0; y < 20; y++) {
        bool rowChanged = false;
        // Zeilen-Offset berechnen: (y * 10)
        u16 rowOffset = (y << 3) + (y << 1);

        for (x = 0; x < 10; x++) {
            // Zugriff auf das flache Board-Array
            if (ctx->board[rowOffset + x] != 0) {
                ctx->board[rowOffset + x] = TILE_ID_GARBAGE; // ID 8 (Shadow)
                rowChanged = true;
            }
        }

        if (rowChanged) {
            ctx->boardFlags |= GF_NEEDS_DRAW;
            // Da wir uns in einer blockierenden Animation befinden, 
            // erzwingen wir das Zeichnen sofort
            drawBoard(); 
            
            // Kurze Pause für den visuellen Effekt (von oben nach unten)
            for (i = 0; i < 3; i++) {
                SYS_doVBlankProcess();
            }
        }
    }

    // Längere Pause am Ende der Animation (ca. 0.6s bei 60Hz)
    for (i = 0; i < 40; i++) {
        SYS_doVBlankProcess();
    }
    
    config.currentScore = ctx->score;
    
    // Highscore berechnen UND im SRAM sichern (Wichtig!)
    check_and_update_highscore(config.currentScore);
    
    currentState = STATE_GAMEOVER;
}


void spawnPiece()
{
    KLog("SPAWN_PIECE: Start");
    if (ctx == NULL)
    {
        KLog("SPAWN_PIECE: Error - Context is NULL");
        return;
    }

    if (ctx->activeBadEffect != EFFECT_NONE)
    {
        if (ctx->activeBadEffect == EFFECT_FULLSPEED)
        {
            if (ctx->badEffectTimer > 0 && ctx->badEffectTimer <= DUR_FULLSPEED_SPAWNS)
            {
                ctx->badEffectTimer--;
                KLog_U1("SPAWN_PIECE: EFFECT_FULLSPEED remaining fast pieces:", ctx->badEffectTimer);

                if (ctx->badEffectTimer <= 0)
                {
                    KLog("SPAWN_PIECE: EFFECT_FULLSPEED expired.");
                    ctx->activeBadEffect = EFFECT_NONE;
                    ctx->badEffectTimer = 0;
                    ctx->lastActiveBadEffect = 99;
                    SOUND_play(SND_GOOD_ITEM);
                }
            }
        }
        else if (ctx->activeBadEffect == EFFECT_SAME_TILES || 
                 ctx->activeBadEffect == EFFECT_I_RAIN)
        {
            if (ctx->badEffectTimer > 0)
            {
                ctx->badEffectTimer--;
                KLog_U2("SPAWN_PIECE: Piece-based effect timer:", ctx->activeBadEffect, "Remaining:", ctx->badEffectTimer);
                
                if (ctx->badEffectTimer <= 0)
                {
                    KLog_U1("SPAWN_PIECE: Effect expired:", ctx->activeBadEffect);
                    ctx->activeBadEffect = EFFECT_NONE;
                    ctx->badEffectTimer = 0;
                    ctx->lastActiveBadEffect = 99;
                    SOUND_play(SND_GOOD_ITEM);
                }
            }
        }
    }

    ctx->type = ctx->nextType;
    KLog_U1("SPAWN_PIECE: Current type set to:", ctx->type);

    if (config.randMode == 0) {
        ctx->nextType = ctx->bag[ctx->bagIndex++];
        KLog_U1("SPAWN_PIECE: Bag Randomizer. Next:", ctx->nextType);
        if (ctx->bagIndex >= 7) 
        {
            KLog("SPAWN_PIECE: Bag empty. Refilling...");
            refillBag();
        }
    } else {
        ctx->nextType = random() % 7;
        KLog_U1("SPAWN_PIECE: Pure Randomizer. Next:", ctx->nextType);
    }

    if (ctx->activeBadEffect == EFFECT_SAME_TILES) 
    {
        ctx->type = ctx->forcedPieceType;
        KLog_U1("SPAWN_PIECE: Overridden by EFFECT_SAME_TILES:", ctx->type);
    }
    if (ctx->activeBadEffect == EFFECT_I_RAIN) 
    {
        ctx->type = 0; 
        KLog("SPAWN_PIECE: Overridden by EFFECT_I_RAIN: Type 0");
    }

    ctx->rotation = 0;
    ctx->pieceY = (ctx->type == 0) ? -1 : 0;

    s16 minRelX = 5, maxRelX = -5;
    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[ctx->type][0][i][0];
        if (px < minRelX) minRelX = px;
        if (px > maxRelX) maxRelX = px;
    }

    if (ctx->pieceX < -minRelX) ctx->pieceX = -minRelX;
    if (ctx->pieceX > (9 - maxRelX)) ctx->pieceX = 9 - maxRelX;
    
    KLog_U2("SPAWN_PIECE: Position adjusted. X:", ctx->pieceX, "Y:", ctx->pieceY);

    ctx->flags |= GF_CAN_HOLD;
    ctx->moveTimer = 0; 
    
    KLog("SPAWN_PIECE: Triggering Item Spawn Logic");
    handle_item_spawn_logic();

    if (GET_FLAG(config.flags, FLAG_SHADOW)) calculate_ghost_y();
    ctx->boardFlags |= GF_NEEDS_DRAW;

    if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation))
    {
        KLog("SPAWN_PIECE: Collision detected at spawn! GAME OVER.");
        SOUND_play(SND_GAME_OVER);
        play_game_over_animation();
    }
    
    KLog("SPAWN_PIECE: Finished");
}

// Verwaltet die Blink-Animation vor PENDING
void update_blinking_animation() {
    if (ctx->clearTimer == 0 || ctx->clearingLineMask == 0) return;

    // Blink-pattern mit 20 Frames:
    // Frames 20-15: Show blocks (normal)
    // Frames 14-10: Hide blocks (empty)
    // Frames 9-5: Show blocks (normal)
    // Frames 4-1: Hide blocks (empty)
    
    bool shouldShowBlocks;
    if (ctx->clearTimer > 10) {
        // Frames 20-11: First blink cycle
        shouldShowBlocks = (ctx->clearTimer > 15); // Show in 20-16, hide in 15-11
    } else {
        // Frames 10-1: Second blink cycle
        shouldShowBlocks = (ctx->clearTimer > 5); // Show in 10-6, hide in 5-1
    }

    // Wende Show/Hide auf alle markierten Zeilen an
    for (u16 y = 0; y < 20; y++) {
        if (ctx->clearingLineMask & (1U << y)) {
            u16 rowOffset = (y << 3) + (y << 1);
            
            if (shouldShowBlocks) {
                // Zeige die Original-Blöcke aus clearingLineBackup
                for (u16 x = 0; x < 10; x++) {
                    ctx->board[rowOffset + x] = ctx->clearingLineBackup[rowOffset + x];
                }
            } else {
                // Verstecke die Blöcke (setze auf 0)
                for (u16 x = 0; x < 10; x++) {
                    ctx->board[rowOffset + x] = 0;
                }
            }
        }
    }
}


// handle_active_animations entfernt, Logic nun direkt in game_update

void finishLineClear() {
    GameContext *ctx = &sctx->game;
    u16 linesFound = 0;
    u16 totalHearts = 0;
    u16 totalSkulls = 0;

    // 0. Restore items from backup before counting (animation may have hidden them)
    for (u16 y = 0; y < 20; y++) {
        if (ctx->clearingLineMask & (1U << y)) {
            u16 rowOffset = (y << 3) + (y << 1);
            for (u16 x = 0; x < 10; x++) {
                ctx->board[rowOffset + x] = ctx->clearingLineBackup[rowOffset + x];
            }
        }
    }

    // 1. First pass: count items across all cleared lines
    for (u16 y = 0; y < 20; y++) {
        if (ctx->clearingLineMask & (1U << y)) {
            u16 rowOffset = (y << 3) + (y << 1);
            for (u16 x = 0; x < 10; x++) {
                u8 tile = ctx->board[rowOffset + x];
                if (tile == ITEM_ID_HEART) totalHearts++;
                else if (tile == ITEM_ID_SKULL) totalSkulls++;
            }
        }
    }

    // 2. Second pass: process cleared lines
    for (u16 y = 0; y < 20; y++) {
        if (ctx->clearingLineMask & (1U << y)) {
            linesFound++;
            u16 rowOffset = (y << 3) + (y << 1);
            
            // Setze PENDING_FLAGS für die betroffenen Zeilen
            SET_LINE_PENDING(y);
            
            // Leere die Zeilen im Board-Array (0 setzen)
            for (u16 x = 0; x < 10; x++) {
                ctx->board[rowOffset + x] = 0;
            }
        }
    }

    // 3. Determine effect based on item weight
    if (linesFound > 0) {
        ctx->comboCount++;
        apply_scoring(linesFound);
        
        // Trigger effect based on heart vs skull count
        if (totalHearts > totalSkulls) {
            // Good effect: trigger heart effect
            ctx->flags |= GF_HEART_TRIG;
            triggerGoodEffect();
            KLog_U2("FINISH_LINE_CLEAR: Hearts dominate. Hearts:", totalHearts, "Skulls:", totalSkulls);
        } else if (totalSkulls > totalHearts) {
            // Bad effect: trigger skull/bad effect
            triggerBadEffect();
            KLog_U2("FINISH_LINE_CLEAR: Skulls dominate. Skulls:", totalSkulls, "Hearts:", totalHearts);
        } else if (totalHearts > 0 || totalSkulls > 0) {
            // Equal items: no effect
            KLog_U2("FINISH_LINE_CLEAR: Equal items. Hearts:", totalHearts, "Skulls:", totalSkulls);
        }
    } else {
        ctx->comboCount = 0;
    }

    // Reset die Blink-Maske
    ctx->clearingLineMask = 0;

    // One-shot CLEARLINE+ must always release the single effect slot.
    if (ctx->activeBadEffect == EFFECT_MULTIPLIER) {
        ctx->activeBadEffect = EFFECT_NONE;
        ctx->badEffectTimer = 0;
        ctx->lastActiveBadEffect = 99;
        KLog("FINISH_LINE_CLEAR: CLEARLINE+ one-shot effect released.");
    }

    KLog_U1("FINISH_LINE_CLEAR: Processed lines:", linesFound);
}

void handle_board_collapse() {
    // Phase 2: Collapse logic
    // Find lowest PENDING marker Y
    for (s16 y = 19; y >= 0; y--) {
        if (GET_LINE_PENDING(y)) {
            KLog_U1("COLLAPSE: Processing Row Y:", y);
            
            // Verschiebe ALLES darüber (Y-1 bis 0) nach unten
            if (y > 0) {
                for (s16 row = y; row > 0; row--) {
                    memcpy(&ctx->board[row * 10], &ctx->board[(row - 1) * 10], 10);
                }
            }
            memset(&ctx->board[0], 0, 10);

            // Aktualisiere verbleibende Marker (Shift +1 für Marker OBERHALB von y)
            // Bits für Rows 0 bis y-1 müssen um 1 nach links (höherer Index = höheres Y) geschoben werden.
            u32 currentFlags = ctx->boardFlags;
            
            // Maske für pending flags unterhalb (höheres Y als aktuelles y), bleiben unverändert
            // (Sollten eigentlich keine sein, da wir von unten iterieren, aber sicher ist sicher)
            u32 maskBelow = ~( (1UL << (y + GF_PENDING_SHIFT + 1)) - 1 );
            
            // Maske für pending flags oberhalb (kleineres Y als aktuelles y)
            u32 maskAbove = (1UL << (y + GF_PENDING_SHIFT)) - 1;
            
            // Bit für aktuelles y wird gelöscht durch Shift-Logik oder explizit
            
            u32 flagsAbove = (currentFlags & maskAbove) & GF_PENDING_MASK;
            u32 flagsBelow = currentFlags & maskBelow;
            u32 otherFlags = currentFlags & ~GF_PENDING_MASK;

            // Shift flagsAbove "down" (Bit index + 1)
            flagsAbove <<= 1;
            
            ctx->boardFlags = otherFlags | flagsBelow | flagsAbove;
            
            // Nur einen Schritt pro Frame verarbeiten
            return;
        }
    }
}

void update_shadows(bool moved, bool collapse, bool garbage) {
    // 6. shadow_update
    if (moved || collapse || garbage) {
        if (GET_FLAG(config.flags, FLAG_SHADOW)) calculate_ghost_y();
    }
}


void performHold() {
    // Prüft das Verhaltens-Bit GF_CAN_HOLD in ctx->flags
    if (!GET_FLAG(config.flags, FLAG_HOLD)) return;

    // Check for skull FIRST - penalty happens even if hold already used
    if (ctx->itemSlot < 4 && ctx->itemType == ITEM_ID_SKULL) { 
        triggerBadEffect();
        ctx->itemSlot = 255; 
        return;
    }
    
    // Check hold availability AFTER skull check
    if (!(ctx->flags & GF_CAN_HOLD)) return;

    if (ctx->activeBadEffect == EFFECT_SAME_TILES) {
        if (ctx->holdType != -1) { 
            ctx->holdType = -1; 
            ctx->lastHoldType = -2; 
            SOUND_play(SND_BAD_ITEM); 
        }
        // Bit löschen statt bool-Zuweisung
        ctx->flags &= ~GF_CAN_HOLD; 
        return; 
    }

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
        
        // Generate new item for swapped piece
        if (config.itemMode == 1) ctx->itemType = (random() % 100 < ITEM_RATIO_HEART) ? ITEM_ID_HEART : ITEM_ID_SKULL;
        else ctx->itemType = (config.itemMode == 2) ? ITEM_ID_HEART : ITEM_ID_SKULL;
        ctx->itemSlot = (ctx->itemType == 0) ? 255 : (random() % 4);
    }

    ctx->itemSlot = 255; 
    // Bit löschen am Ende des Hold-Vorgangs
    ctx->flags &= ~GF_CAN_HOLD;
}

void addGarbageLine() {
    if (ctx == NULL) return;

    // 1. Board physisch nach OBEN schieben (Y-1)
    for (u16 row = 0; row < 19; row++) {
        memcpy(&ctx->board[row * 10], &ctx->board[(row + 1) * 10], 10);
    }

    // 2. Neue Garbage-Zeile ganz unten (Y=19) generieren
    u8 randomColor = 1 + (random() % 7); 
    u16 holeX = random() % 10;
    // Zeile 19 beginnt bei Index 190
    memset(&ctx->board[190], randomColor, 10);
    ctx->board[190 + holeX] = 0;

    // 3. Piece-Push vermeiden: aktive Figur unverändert lassen
    // Bei Folge-Kollision: Game Over (keine automatische Verschiebung).
    if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation)) {
        play_game_over_animation();
        return;
    }

    // 4. FLAGS FIX: Pending-Bits rücken nach oben (Richtung Bit 0)
    // Wir isolieren Bits 10-29 (Pending Lines)
    u32 lineFlags = (ctx->boardFlags & 0xFFFFFC00); 
    
    // Shift nach rechts verringert den Bit-Index (entspricht Y-1)
    lineFlags >>= 1; 
    
    // WICHTIG: Das Bit, das auf Pos 9 gerutscht ist, MUSS gelöscht werden
    lineFlags &= 0xFFFFFC00; 

    // System-Flags (Bit 0-9) erhalten und neue Line-Flags einsetzen
    ctx->boardFlags = (ctx->boardFlags & ~0xFFFFFC00) | lineFlags;

    // 5. Update & Sound
    if (GET_FLAG(config.flags, FLAG_SHADOW)) calculate_ghost_y();
    
    SOUND_play(SND_GARBAGE);
    ctx->boardFlags |= GF_NEEDS_DRAW;
}


void update_comment_timer() {
    if (ctx->commentTimer > 0) {
        ctx->commentTimer--;
        if (ctx->commentTimer == 0) {
            ctx->lastComment[0] = '\0';
        }
    }
}


void finalize_game_session() {
    if (ctx == NULL) return;

    // Highscore-Update
    check_and_update_highscore(ctx->score);

    // Statistik-Vorbereitung für Game Over Screen
    config.currentScore = ctx->score;
}

void reset_game_logic() {
    if (ctx == NULL) return;

    // 1. Board & Basis-Daten
    memset(ctx->board, 0, 200); // board[200]
    memset(ctx->clearingLineBackup, 0, 200); // clearingLineBackup[200]
    ctx->score = 0;
    ctx->linesTotal = 0;
    ctx->comboCount = 0;
    
    // Initialer Startwert für Persistent X
    ctx->pieceX = 3; 

    // 2. Level-Berechnung
    // Start-Level is fixed; speed setting now directly affects gravity.
    ctx->startLevel = 1;
    ctx->level = ctx->startLevel;

    // 3. Status & Effekte
    ctx->activeBadEffect = EFFECT_NONE;
    ctx->badEffectTimer = 0;
    ctx->sortingRow = -1;
    ctx->clearTimer = 0;
    ctx->clearingLineMask = 0;  // Reset blink animation mask
    highscoreUpdated = false;
    ctx->moveTimer = 0;
    ctx->holdType = -1;
    
    if (GET_FLAG(config.flags, FLAG_HOLD)) ctx->flags |= GF_CAN_HOLD; 
    else ctx->flags &= ~GF_CAN_HOLD;

    ctx->flags &= ~(GF_HEART_TRIG | GF_SKULL_TRIG);
    ctx->boardFlags = GF_NEEDS_DRAW;

    // 4. DAS Timer Initialisierung
    ctx->dasTimer = 0;
    ctx->dasDir = 0;
    ctx->dasNextThreshold = config.thresholdLRInitial;

    // 5. Garbage Timer Initialisierung
    ctx->garbageTimer = 0;
    u16 garbageSetting = (config.garbageFreq > GARBAGE_FREQ_MAX) ? GARBAGE_FREQ_MAX : config.garbageFreq;
    if (garbageSetting > 0) {
        u16 base = GARBAGE_INTERVALS[garbageSetting];
        ctx->garbageNextThreshold = GET_TICKS(base + (random() % 120) - 60);
        if (ctx->garbageNextThreshold < GET_TICKS(30)) {
            ctx->garbageNextThreshold = GET_TICKS(30);
        }
    } else {
        ctx->garbageNextThreshold = 0; // Explizit nullen
    }

    // 5. RNG & Erster Stein
    refillBag();
    ctx->nextType = ctx->bag[ctx->bagIndex++];
    spawnPiece();

    // 6. UI-Cache Invalidierung (Sortiert nach Typ)
    ctx->lastActiveBadEffect = 99; 
    ctx->lastBadEffectTimer  = -1;
    ctx->lastComboCount      = 0xFFFF;
    ctx->lastHoldType        = -2;
    ctx->lastLevel           = 0xFFFF;
    ctx->lastLinesNext       = 0xFFFF;
    ctx->lastNextType        = -2;
    ctx->lastScore           = 0xFFFFFFFF; 

    ctx->boardFlags |= GF_NEEDS_DRAW;
}

static void handle_rainbow_row(u16 y) {
    u8 rowColor = (random() % 7) + 1;
    
    u16 rowOffset = (y << 3) + (y << 1);

    for (u16 x = 0; x < 10; x++) {
        u16 index = rowOffset + x;
        u8 tile = ctx->board[index];

        if (tile != 0) {
            ctx->board[index] = rowColor;
        }
    }
}

static void handle_shadow_row(u16 y) {
    u16 rowOffset = (y << 3) + (y << 1);

    for (u16 x = 0; x < 10; x++) {
        u16 index = rowOffset + x;
        u8 tile = ctx->board[index];

        if (tile != 0) {
            ctx->board[index] = 8;
        }
    }
}


static void handle_sort_row(u16 y) {
    u8 tempRow[10];
    u16 x, filled = 0;
    
    u16 rowOffset = (y << 3) + (y << 1);

    for (x = 0; x < 10; x++) {
        u8 tile = ctx->board[rowOffset + x];
        if (tile != 0) {
            tempRow[filled++] = tile;
        }
    }

    for (x = 0; x < filled; x++) {
        ctx->board[rowOffset + x] = tempRow[x];
    }

    if (filled < 10) {
        memset(&ctx->board[rowOffset + filled], 0, 10 - filled);
    }
}

void trigger_line_items(u16 y) {
    // Einmalige Berechnung des Zeilen-Offsets: (y << 3) + (y << 1)
    u16 rowOffset = (y << 3) + (y << 1);

    for (u16 x = 0; x < 10; x++) {
        u8 tile = ctx->board[rowOffset + x];

        // Item-Checks: only mark flags, do not trigger effects directly here.
        if (tile == ITEM_ID_SKULL) {
            ctx->flags |= GF_SKULL_TRIG;
        }
        else if (tile == ITEM_ID_HEART) {
            ctx->flags |= GF_HEART_TRIG;
        }
    }
}

void update_board_animations() {
    // Handle RAINBOW animation line-by-line
    if (ctx->activeBadEffect == EFFECT_RAINBOW) {
        if (ctx->sortingRow >= 0 && ctx->sortingRow <= 19) {
            handle_rainbow_row((u16)ctx->sortingRow);
            ctx->boardFlags |= GF_NEEDS_DRAW;
            ctx->sortingRow++;

            if (ctx->sortingRow >= 20) {
                ctx->sortingRow = -1;
                ctx->activeBadEffect = EFFECT_NONE;
                ctx->badEffectTimer = 0;
            }
        }
        return;
    }

    // Handle LIGHTSOUT application line-by-line (once), effect timer remains active
    if (ctx->activeBadEffect == EFFECT_SHADOW_BOARD) {
        if (ctx->sortingRow >= 0 && ctx->sortingRow <= 19) {
            handle_shadow_row((u16)ctx->sortingRow);
            ctx->boardFlags |= GF_NEEDS_DRAW;
            ctx->sortingRow++;

            if (ctx->sortingRow >= 20) {
                ctx->sortingRow = -1;
            }
        }
        return;
    }

    // Handle SORT BOARD animation
    if (ctx->sortingRow >= 0 && ctx->sortingRow <= 19) {
        u16 y = (u16)ctx->sortingRow;
        u16 rowOffset = (y << 3) + (y << 1);

        for (u16 x = 0; x < 10; x++) {
            u16 index = rowOffset + x;
            ctx->board[index] = sortBuffer[index];
        }

        handle_sort_row(y);
    ctx->boardFlags |= GF_NEEDS_DRAW;
        ctx->sortingRow++;

        if (ctx->sortingRow >= 20) {
            ctx->sortingRow = -1; // Animation complete
            ctx->boardFlags |= GF_NEEDS_DRAW;
            
            // Check for newly formed lines or spawn new piece
            if (clearLines() == 0) {
                spawnPiece();
            }
        }
        return;
    }
}

void handle_game_over() {
    SOUND_play(SND_GAME_OVER);
    finalize_game_session();
    view_animate_grayscale();
    
    for (u16 i = 0; i < 30; i++) SYS_doVBlankProcess();
    
    view_fade_out_frame();
    currentState = STATE_GAMEOVER;
}