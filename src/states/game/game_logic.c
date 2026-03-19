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
        ctx->itemSpawnCounter = (random() % 3) + 2;
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
    if (ctx == NULL) return;

    ctx->activeBadEffect = EFFECT_NONE;
    ctx->badEffectTimer = 0;
    ctx->lastActiveBadEffect = 99;

    u16 chance = random() % 6;
    
    if (chance == 0) {
        // HEAL & CLEAR: Board von unten nach oben kopieren (Shift Down)
        for (s16 row = 19; row > 0; row--) {
            memcpy(&ctx->board[row * 10], &ctx->board[(row - 1) * 10], 10);
        }
        memset(&ctx->board[0], 0, 10);
        set_game_comment("HEAL & CLEAR!", 90);
    } 
    else if (chance == 1) { 
        ctx->sortingRow = 0; 
        set_game_comment("HEAL & SORT!", 90); 
    }
    else if (chance == 2) {
        for (u16 i = 0; i < 200; i++) {
            if (ctx->board[i] == ITEM_ID_SKULL) {
                ctx->board[i] = 1 + (random() % 7);
            }
        }
        set_game_comment("SKULLS RECLAIMED!", 90);
    }
    else if (chance == 3) { 
        ctx->activeBadEffect = EFFECT_I_RAIN; 
        ctx->badEffectTimer = DUR_I_RAIN_SPAWNS; 
        set_game_comment("I-BEAM RAIN!", 90); 
    }
    else if (chance == 4) { 
        ctx->activeBadEffect = EFFECT_FREEZE; 
        ctx->badEffectTimer = DUR_FREEZE_TICKS; 
        set_game_comment("TIME FREEZE!", 90); 
    }
    else { 
        ctx->activeBadEffect = EFFECT_RAINBOW; 
        ctx->badEffectTimer = 0; 
        ctx->sortingRow = 0; 
        set_game_comment("RAINBOW POWER!", 90); 
    }

    for (u16 i = 0; i < 200; i++) {
        if (ctx->board[i] == ITEM_ID_HEART) {
            ctx->board[i] = 1 + (random() % 7);
        }
    }

    SOUND_play(SND_GOOD_ITEM);
    ctx->commentTimer = 60;
    ctx->boardFlags |= GF_NEEDS_DRAW;
}



void triggerBadEffect() {
    if (ctx == NULL) return;

    u16 roll = (random() % 7) + 1; 
    ctx->activeBadEffect = (roll <= 6) ? roll : EFFECT_SHADOW_BOARD;

    switch(ctx->activeBadEffect) {
case EFFECT_FULLSPEED:
    ctx->badEffectTimer = 120; // Nur die Warnzeit
    ctx->activeBadEffect = EFFECT_FULLSPEED;
    //SOUND_play(SND_ALERT);
    break;

        case EFFECT_SAME_TILES: 
            ctx->badEffectTimer = DUR_SAME_TILES_SPAWNS;
            // Zufälligen Typ für den Fluch festlegen
            ctx->forcedPieceType = random() % 7; 
            set_game_comment("SPEED / SAME!", 90); 
            break;

        case EFFECT_NO_ROTATE:  
            ctx->badEffectTimer = DUR_NO_ROTATE_TICKS;
            set_game_comment("NO ROTATE!", 90); 
            break;

        case EFFECT_REVERSED:   
            ctx->badEffectTimer = DUR_REVERSED_TICKS;
            set_game_comment("REVERSED!", 90); 
            break;


case EFFECT_HOLD_LOCK:
    ctx->badEffectTimer = DUR_HOLD_LOCK_TICKS;
    // Wir merken uns den Typ NICHT extra, da holdType nur für die Anzeige ist.
    // Der eigentliche Stein-Typ bleibt in einer internen Variable oder wir 
    // verstecken ihn nur optisch:
    ctx->lastHoldType = ctx->holdType; // Backup des Steins
    ctx->holdType = -1;                // Unsichtbar machen
    ctx->flags &= ~GF_CAN_HOLD;        // Sperren
    set_game_comment("HOLD LOCKED!", 90);
    break;

        case EFFECT_HIDE_NEXT:  
            ctx->badEffectTimer = DUR_HIDE_NEXT_TICKS;
            set_game_comment("NEXT HIDDEN!", 90); 
            break;

        case EFFECT_SHADOW_BOARD: 
            ctx->badEffectTimer = DUR_SHADOW_TICKS;
            ctx->sortingRow = 0; 
            set_game_comment("DARK CURSE!", 90); 
            break;
    }

    SOUND_play(SND_BAD_ITEM);
    ctx->commentTimer = 60;
    // Erzwingt, dass sprite.c im nächsten Sync die Änderungen bemerkt
    ctx->lastActiveBadEffect = 99; 
}




void lockPiece() {
    bool lockedAbove = false;
    u16 i;

    // 1. TRANSFER: Schreibt die Steine ins Board.
    // Diese Funktion MUSS intern 'is_within_board' und 'set_board_tile' nutzen.
    transfer_piece_to_board(); 

    // 2. GAME OVER CHECK: Nutzen der Hilfsmakros
    for (i = 0; i < 4; i++) {
        s16 gy = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];
        s16 gx = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];

        // Wenn der Stein zwar platziert wurde, aber ein Teil davon 
        // laut 'is_within_board' außerhalb (oben) liegt -> Game Over.
        if (!is_within_board(gx, gy) && gy < 0) {
            lockedAbove = true;
            break; 
        }
    }

    SOUND_play(SND_PIECE_LOCK);

    if (lockedAbove) {
        SOUND_play(SND_GAME_OVER);
        play_game_over_animation();
    } else {
        // clearLines nutzt intern 'get_board_tile' mit Bitshifts
        if (clearLines() == 0) {
            spawnPiece();
        }
    }
}

void check_and_update_highscore(u32 score) {
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
    u16 linesCleared = 0;

    for (u16 y = 0; y < 20; y++) {
        bool full = true;
        u16 h = 0, s = 0;
        
        // Einmalige Berechnung des Zeilen-Offsets (y * 10)
        u16 rowOffset = (y << 3) + (y << 1);

        for (u16 x = 0; x < 10; x++) {
            // Direkter Zugriff auf das flache Array
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
            SET_LINE_PENDING(y); // Markiert die Zeile für die Animation

            // Bestimmung des Füll-Typs für die Animation
            u8 f;
            if (h == 0 && s == 0) f = 0;
            else if (h > s) { 
                f = ITEM_ID_HEART; 
                ctx->flags |= GF_HEART_TRIG; 
            }   
            else if (s > h) { 
                f = ITEM_ID_SKULL; 
                ctx->flags |= GF_SKULL_TRIG; 
            }   
            else f = TILE_ID_FLASH;

            // Zeile im Board mit dem Effekt-Tile füllen
            for (u16 x = 0; x < 10; x++) {
                ctx->board[rowOffset + x] = f;
            }
        }
    }

    if (linesCleared > 0) {
        apply_scoring(linesCleared);
        ctx->comboCount++;
        
        // Timer für die Lösch-Animation (z.B. Blinken)
        ctx->clearTimer = GET_TICKS(20); 
        
        // Sound-Variation basierend auf Combo
        SOUND_play(52 + ctx->comboCount);
        
        // View-Update triggern
        ctx->boardFlags |= GF_NEEDS_DRAW;
    } else {
        ctx->comboCount = 0;
    }

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
    if (ctx == NULL) return;

    // 1. EFFEKT-DEKREMENTIERUNG (Stückbasiert)
    if (ctx->activeBadEffect != EFFECT_NONE)
    {
        if (ctx->activeBadEffect == EFFECT_FULLSPEED)
        {
            if (ctx->badEffectTimer == 1) ctx->badEffectTimer = 0;
            if (ctx->badEffectTimer <= 0)
            {
                ctx->badEffectTimer--; 
                if (ctx->badEffectTimer <= -DUR_FULLSPEED_SPAWNS)
                {
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
                if (ctx->badEffectTimer <= 0)
                {
                    ctx->activeBadEffect = EFFECT_NONE;
                    ctx->badEffectTimer = 0;
                    ctx->lastActiveBadEffect = 99;
                    SOUND_play(SND_GOOD_ITEM);
                }
            }
        }
    }

    // 2. TYP-WAHL (Bag oder Random)
    ctx->type = ctx->nextType;

    if (config.randMode == 0) {
        ctx->nextType = ctx->bag[ctx->bagIndex++];
        if (ctx->bagIndex >= 7) refillBag();
    } else {
        ctx->nextType = random() % 7;
    }

    // Fluch-Überschreibung
    if (ctx->activeBadEffect == EFFECT_SAME_TILES) ctx->type = ctx->forcedPieceType;
    if (ctx->activeBadEffect == EFFECT_I_RAIN) ctx->type = 0; 

    // 3. POSITIONIERUNG & CLAMPING
    ctx->rotation = 0;
    ctx->pieceY = (ctx->type == 0) ? -1 : 0;

    // Berechnung der horizontalen Ausdehnung für das Clamping (Persistent X)
    s16 minRelX = 5, maxRelX = -5;
    for (u16 i = 0; i < 4; i++) {
        s16 px = PIECES[ctx->type][0][i][0];
        if (px < minRelX) minRelX = px;
        if (px > maxRelX) maxRelX = px;
    }

    // Sicherstellen, dass PieceX + minRelX >= 0 und PieceX + maxRelX <= 9
    if (ctx->pieceX < -minRelX) ctx->pieceX = -minRelX;
    if (ctx->pieceX > (9 - maxRelX)) ctx->pieceX = 9 - maxRelX;

    // 4. STATUS-RESETS
    ctx->flags |= GF_CAN_HOLD;
    ctx->moveTimer = 0; 
    handle_item_spawn_logic();

    // 5. GHOST & VIEW UPDATE
    if (GET_FLAG(config.flags, FLAG_SHADOW)) calculate_ghost_y();
    ctx->boardFlags |= GF_NEEDS_DRAW;

    // 6. GAME OVER CHECK
    if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation))
    {
        SOUND_play(SND_GAME_OVER);
        play_game_over_animation();
    }
}

bool handle_active_animations(GameContext* ctx) {
    if (ctx == NULL) return false;

    // 1. Line-Clear Blinken
    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            bool isVisual = (ctx->activeBadEffect == EFFECT_RAINBOW || ctx->activeBadEffect == EFFECT_SHADOW_BOARD);
            if (ctx->sortingRow == -1 || isVisual) spawnPiece();
        }
        ctx->boardFlags |= GF_NEEDS_DRAW;
        return true; 
    }

    // 2. Board-Animationen (Rainbow, Shadow, Sort)
    if (ctx->sortingRow != -1) {
        u16 y = ctx->sortingRow;
        bool isVisual = (ctx->activeBadEffect == EFFECT_RAINBOW || ctx->activeBadEffect == EFFECT_SHADOW_BOARD);

        if (ctx->activeBadEffect == EFFECT_RAINBOW) handle_rainbow_row(y);
        else if (ctx->activeBadEffect == EFFECT_SHADOW_BOARD) handle_shadow_row(y);
        else handle_sort_row(y);

        ctx->sortingRow++;
        ctx->boardFlags |= GF_NEEDS_DRAW;

        if (ctx->sortingRow >= 20) {
            ctx->sortingRow = -1;
            
            // FIX: Wenn ein visueller Effekt (Rainbow) endet, Zustand auf NONE setzen
            if (isVisual) {
                ctx->activeBadEffect = EFFECT_NONE;
                ctx->lastActiveBadEffect = 99; // Sprite-Update erzwingen
                SOUND_play(SND_GOOD_ITEM);
            } else {
                spawnPiece();
            }
        }
        if (!isVisual) return true; 
    }

    // 3. Transition: Shadow zu Rainbow
    if (ctx->activeBadEffect == EFFECT_SHADOW_BOARD && ctx->badEffectTimer > 0) {
        ctx->badEffectTimer--;
        if (ctx->badEffectTimer == 0) {
            ctx->activeBadEffect = EFFECT_RAINBOW;
            ctx->sortingRow = 0; 
            
            // WICHTIG: Cache invalidieren, da sich die IDs im Board massiv ändern
            // Falls tileCache global/extern ist:
            // memset(tileCache, 0xFF, 400); 
            
            set_game_comment("RAINBOW REBIRTH!", 90);
            SOUND_play(SND_GOOD_ITEM);
        }
    }

    return false; 
}

void finishLineClear() {
    for (s16 y = 19; y >= 0; y--) {
        if (GET_LINE_PENDING(y)) {
            // 1. Zeilen physisch verschieben
            if (y > 0) {
                // Kopiert alle Zeilen oberhalb (0 bis y-1) eine Position nach unten
                for (s16 row = y; row > 0; row--) {
                    memcpy(&ctx->board[row * 10], &ctx->board[(row - 1) * 10], 10);
                }
            }
            // Die neue oberste Zeile (Index 0) leeren
            memset(&ctx->board[0], 0, 10);

            // 2. Flags synchronisieren (Wichtig für Mehrfach-Linien)
            // Bit 10 = Zeile 0, Bit 29 = Zeile 19
            u32 currentBitPos = y + GF_PENDING_SHIFT;
            u32 maskAbove = (1UL << currentBitPos) - 1;
            
            // Isoliere Flags der Zeilen oberhalb (0 bis y-1)
            u32 pendingAbove = (ctx->boardFlags & maskAbove) & 0xFFFFFC00;
            // Isoliere Flags der Zeilen unterhalb (y+1 bis 19)
            u32 pendingBelow = (ctx->boardFlags & ~((1UL << (currentBitPos + 1)) - 1)) & 0xFFFFFC00;

            // Flags neu zusammensetzen:
            // - pendingBelow bleibt statisch
            // - pendingAbove wird um 1 Bit nach links geschoben (Index + 1)
            // - Das Flag der gerade gelöschten Zeile y wird überschrieben
            // - Das neue Flag für Zeile 0 wird automatisch 0
            ctx->boardFlags = (ctx->boardFlags & ~0xFFFFFC00) | pendingBelow | (pendingAbove << 1);

            // Re-Check der aktuellen Zeile y, da dort nun der Inhalt von y-1 liegt
            y++; 
        }
    }

    // Trigger für Item-Effekte
    if (ctx->flags & GF_HEART_TRIG) {
        triggerGoodEffect();
        ctx->flags &= ~GF_HEART_TRIG;
    }
    if (ctx->flags & GF_SKULL_TRIG) {
        triggerBadEffect();
        ctx->flags &= ~GF_SKULL_TRIG;
    }
}

void performHold() {
    // Prüft das Verhaltens-Bit GF_CAN_HOLD in ctx->flags
    if (!GET_FLAG(config.flags, FLAG_HOLD) || !(ctx->flags & GF_CAN_HOLD)) return;

    if (ctx->itemSlot < 4 && ctx->itemType == ITEM_ID_SKULL) { 
        triggerBadEffect(); 
        ctx->itemSlot = 255; 
    }

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

    // 3. Piece-Push: Der aktive Stein rückt mit dem Board nach oben
    ctx->pieceY--;

    // Sofortiger Game-Over Check, falls das Teil nun in statische Blöcke gedrückt wurde
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
    ctx->score = 0;
    ctx->linesTotal = 0;
    ctx->comboCount = 0;
    
    // Initialer Startwert für Persistent X
    ctx->pieceX = 3; 

    // 2. Level-Berechnung
    ctx->startLevel = (config.speedLevel == 2) ? 5 : 
                      (config.speedLevel == 3) ? 10 : 1;
    ctx->level = ctx->startLevel;

    // 3. Status & Effekte
    ctx->activeBadEffect = EFFECT_NONE;
    ctx->badEffectTimer = 0;
    ctx->sortingRow = -1;
    ctx->clearTimer = 0;
    ctx->moveTimer = 0;
    ctx->holdType = -1;
    
    if (GET_FLAG(config.flags, FLAG_HOLD)) ctx->flags |= GF_CAN_HOLD; 
    else ctx->flags &= ~GF_CAN_HOLD;

    ctx->flags &= ~(GF_HEART_TRIG | GF_SKULL_TRIG);
    ctx->boardFlags = GF_NEEDS_DRAW;

    // 4. Garbage Timer Initialisierung
    ctx->garbageTimer = 0;
    if (config.garbageFreq > 0) {
        u16 base = GARBAGE_INTERVALS[config.garbageFreq];
        ctx->garbageNextThreshold = GET_TICKS(base + (random() % 120) - 60);
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
    // y % 7 für die Farbwahl (1-7)
    u8 rowColor = (y % 7) + 1;
    
    u16 rowOffset = (y << 3) + (y << 1);

    for (u16 x = 0; x < 10; x++) {
        u16 index = rowOffset + x;
        u8 tile = ctx->board[index];

        // Prüfen auf belegte Tiles (ID 1-10)
        if (tile != 0 && tile <= 10) {
            ctx->board[index] = rowColor;
        }
    }
}


static void handle_shadow_row(u16 y) {
    // Zeilen-Offset berechnen: (y << 3) + (y << 1) entspricht y * 10
    u16 rowOffset = (y << 3) + (y << 1);

    for (u16 x = 0; x < 10; x++) {
        u16 index = rowOffset + x;
        u8 tile = ctx->board[index];

        // Nur farbige Tiles (1-9) in Schatten (8) umwandeln
        if (tile != 0 && tile < 10) {
            ctx->board[index] = 8;
        }
    }
}


static void handle_sort_row(u16 y) {
    u8 tempRow[10];
    u16 x, filled = 0;
    
    // Offset-Berechnung via Bitshift: y * 10
    u16 rowOffset = (y << 3) + (y << 1);

    // 1. Phase: Nicht-leere Tiles sammeln
    for (x = 0; x < 10; x++) {
        u8 tile = ctx->board[rowOffset + x];
        if (tile != 0) {
            tempRow[filled++] = tile;
        }
    }

    // 2. Phase: Gesammelte Tiles linksbündig zurückschreiben
    for (x = 0; x < filled; x++) {
        ctx->board[rowOffset + x] = tempRow[x];
    }

    // 3. Phase: Rest der Zeile mit Nullen füllen
    // Nutzt memset für effizientes Füllen auf dem 68k
    if (filled < 10) {
        memset(&ctx->board[rowOffset + filled], 0, 10 - filled);
    }
}


void trigger_line_items(u16 y) {
    // Einmalige Berechnung des Zeilen-Offsets: (y << 3) + (y << 1)
    u16 rowOffset = (y << 3) + (y << 1);

    for (u16 x = 0; x < 10; x++) {
        u8 tile = ctx->board[rowOffset + x];

        // Item-Checks
        if (tile == ITEM_ID_SKULL) {
            triggerBadEffect();
        } 
        else if (tile == ITEM_ID_HEART) {
            ctx->flags |= GF_HEART_TRIG;
        }
    }
}

void update_board_animations() {
    // Sicherheitscheck für gültige Indizes
    if (ctx->sortingRow == -1 || ctx->sortingRow > 19) return;

    u16 y = (u16)ctx->sortingRow;
    
    // Einmalige Berechnung des Zeilen-Offsets: (y << 3) + (y << 1)
    u16 rowOffset = (y << 3) + (y << 1);

    // Zeile vom flachen Puffer ins flache Board schreiben
    for (u16 x = 0; x < 10; x++) {
        u16 index = rowOffset + x;
        ctx->board[index] = sortBuffer[index];
    }

    // Optional: Logik zur horizontalen Sortierung/Bereinigung
    handle_sort_row(y);

    // Index für den nächsten Frame erhöhen
    ctx->sortingRow++;

    // Abschlussprüfung nach der letzten Reihe (19)
    if (ctx->sortingRow >= 20) {
        ctx->sortingRow = -1; // Animation beenden
        
        // Prüfung auf neu entstandene Linien oder neuen Stein spawnen
        if (clearLines() == 0) {
            spawnPiece();
        }
    }

    // Grafik-Update für diesen Frame markieren
    ctx->boardFlags |= GF_NEEDS_DRAW;
}

void handle_game_over() {
    SOUND_play(SND_GAME_OVER);
    finalize_game_session();
    view_animate_grayscale();
    
    for (u16 i = 0; i < 30; i++) SYS_doVBlankProcess();
    
    view_fade_out_frame();
    currentState = STATE_GAMEOVER;
}