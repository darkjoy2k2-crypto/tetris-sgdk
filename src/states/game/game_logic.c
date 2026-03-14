#include <genesis.h>
#include <string.h>
#include "states/game/game_logic.h"
#include "states/game/game_core.h"
#include "states/game/game_view.h"
#include "menu_bg.h"
#include "states/states.h"
#include "sound_manager.h"
#include "sounds.h"
#include "states/game/game_view.h" 

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
static u8 sortBuffer[BOARD_WIDTH][BOARD_HEIGHT];

void triggerManualSort() {
    if (ctx == NULL || ctx->sortingRow != -1) return;

    // 1. Das gesamte Board (0 bis 19) in den Puffer
    for (u16 y = 0; y < 20; y++) {
        for (u16 x = 0; x < 10; x++) {
            sortBuffer[x][y] = ctx->board[x][y];
        }
    }

    // 2. Selection Sort: Wir füllen das Board von UNTEN (19) nach OBEN (0)
    for (s16 targetY = 19; targetY >= 0; targetY--) {
        u16 maxBlocksIndex = 0;
        u16 maxBlocksCount = 0;

        // Suche im verbleibenden oberen Teil die vollste Reihe
        for (u16 currentY = 0; currentY <= (u16)targetY; currentY++) {
            u16 currentCount = 0;
            for (u16 x = 0; x < 10; x++) {
                if (sortBuffer[x][currentY] != 0) currentCount++;
            }

            if (currentCount >= maxBlocksCount) {
                maxBlocksCount = currentCount;
                maxBlocksIndex = currentY;
            }
        }

        // Tausch der Reihen im Puffer
        for (u16 x = 0; x < 10; x++) {
            u8 temp = sortBuffer[x][targetY];
            sortBuffer[x][targetY] = sortBuffer[x][maxBlocksIndex];
            sortBuffer[x][maxBlocksIndex] = temp;
        }
    }

    // 3. Animation bei 0 starten (läuft dann bis 19 durch)
    ctx->sortingRow = 0; 
    SOUND_play(SND_TETRIS);
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

static void transfer_piece_to_board() {
    for (u16 i = 0; i < 4; i++) {
        s16 px = ctx->pieceX + PIECES[ctx->type][ctx->rotation][i][0];
        s16 py = ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1];
        if (py >= 0) {
            ctx->board[px][py] = (i == ctx->itemSlot) ? ctx->itemType : (ctx->type + 1);
        }
    }
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



static void set_game_comment(const char* text, u16 duration) {
    strncpy(ctx->lastComment, text, 20);
    ctx->lastComment[19] = '\0';
    ctx->commentTimer = duration;
}

void triggerBadEffect() {
    u16 roll = (random() % 7) + 1; 
    ctx->activeBadEffect = (roll <= 6) ? roll : EFFECT_SHADOW_BOARD;

    switch(ctx->activeBadEffect) {
        // Suche diesen Case:
        case EFFECT_FULLSPEED: 
ctx->badEffectTimer = GET_TICKS(5);
            // Füge das hier ein:
            set_game_comment("FULL SPEED!", 90); 
            break;
            case EFFECT_SAME_TILES: 
ctx->badEffectTimer = GET_TICKS(5);
            strncpy(ctx->lastComment, "SPEED / SAME!", 20); 
            break;
        case EFFECT_NO_ROTATE:  
ctx->badEffectTimer = GET_TICKS(180);
            strncpy(ctx->lastComment, "NO ROTATE!", 20); 
            break;
        case EFFECT_REVERSED:   
ctx->badEffectTimer = GET_TICKS(240);
            strncpy(ctx->lastComment, "REVERSED!", 20); 
            break;
        case EFFECT_HOLD_LOCK:  
ctx->badEffectTimer = GET_TICKS(300);
            ctx->holdType = -1; 
            ctx->canHold = false; 
            strncpy(ctx->lastComment, "HOLD LOCKED!", 20); 
            break;
        case EFFECT_HIDE_NEXT:  
ctx->badEffectTimer = GET_TICKS(300);
            strncpy(ctx->lastComment, "NEXT HIDDEN!", 20); 
            break;
        case EFFECT_SHADOW_BOARD: 
            ctx->activeBadEffect = EFFECT_SHADOW_BOARD;
            ctx->badEffectTimer = GET_TICKS(300); // Die 5 Sekunden "Licht aus"
            ctx->sortingRow = 0; // Startet die Verwandlung in Schatten-Farben
            set_game_comment("DARK CURSE!", 90); 
            break;
    }
    SOUND_play(SND_BAD_ITEM);
    ctx->commentTimer = 60;
    ctx->lastActiveBadEffect = 99;
}



void lockPiece() {
    bool lockedAbove = false;
    transfer_piece_to_board();
    for (u16 i = 0; i < 4; i++) {
        if (ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1] < 0) lockedAbove = true;
    }
    SOUND_play(SND_PIECE_LOCK);
    if (lockedAbove) {
        SOUND_play(SND_GAME_OVER);
        play_game_over_animation();
    } else {
        if (clearLines() == 0) {
            spawnPiece();
        }
    }
}


void triggerGoodEffect() {
    ctx->activeBadEffect = EFFECT_NONE;
    ctx->badEffectTimer = 0;
    ctx->lastActiveBadEffect = 99;

    u16 chance = random() % 6;
    if (chance == 0) {
        for (s16 y = BOARD_HEIGHT - 1; y > 0; y--) {
            for (u16 x = 0; x < BOARD_WIDTH; x++) ctx->board[x][y] = ctx->board[x][y - 1];
        }
        for (u16 x = 0; x < BOARD_WIDTH; x++) ctx->board[x][0] = 0;
        strncpy(ctx->lastComment, "HEAL & CLEAR!", 20);
    } 
    else if (chance == 1) { ctx->sortingRow = 0; strncpy(ctx->lastComment, "HEAL & SORT!", 20); }
    else if (chance == 2) {
        for (u16 y = 0; y < BOARD_HEIGHT; y++) {
            for (u16 x = 0; x < BOARD_WIDTH; x++) {
                if (ctx->board[x][y] == ITEM_ID_SKULL) {
                    ctx->board[x][y] = 1 + (random() % 7);
                }
            }
        }
        strncpy(ctx->lastComment, "SKULLS RECLAIMED!", 20);
    }
    else if (chance == 3) { ctx->activeBadEffect = EFFECT_I_RAIN; ctx->badEffectTimer = GET_TICKS(4); strncpy(ctx->lastComment, "I-BEAM RAIN!", 20); }
    else if (chance == 4) { ctx->activeBadEffect = EFFECT_FREEZE; ctx->badEffectTimer = GET_TICKS(600); strncpy(ctx->lastComment, "TIME FREEZE!", 20); }
    else { ctx->activeBadEffect = EFFECT_RAINBOW; ctx->sortingRow = 0; strncpy(ctx->lastComment, "RAINBOW POWER!", 20); }

    // Alle verbleibenden Herzen in bunte Steine verwandeln
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (ctx->board[x][y] == ITEM_ID_HEART) {
                ctx->board[x][y] = 1 + (random() % 7);
            }
        }
    }

    SOUND_play(SND_GOOD_ITEM);
    ctx->commentTimer = 60;
    ctx->needsBoardDraw = true;
}


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
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        bool full = true;
        u16 h = 0, s = 0;
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            u8 t = ctx->board[x][y];
            if (t == 0) { full = false; break; }
            if (t == ITEM_ID_HEART) h++;
            else if (t == ITEM_ID_SKULL) s++;
        }
        if (full) {
            linesCleared++;
            ctx->pendingLines[y] = true;
            u8 f;
            if (h == 0 && s == 0) f = 0;
            else if (h > s) { f = ITEM_ID_HEART; ctx->heartTriggered = true; }
            else if (s > h) { f = ITEM_ID_SKULL; ctx->skullTriggered = true; }
            else f = TILE_ID_FLASH;
            for (u16 x = 0; x < BOARD_WIDTH; x++) ctx->board[x][y] = f;
        }
    }
    if (linesCleared > 0) {
        apply_scoring(linesCleared);
        ctx->comboCount++;
        ctx->clearTimer = GET_TICKS(20);
        SOUND_play(52 + ctx->comboCount);
    } else {
        ctx->comboCount = 0;
    }
    return linesCleared;
}

// Die Game-Over Animation (Von oben nach unten Shadow-Pieces)
void play_game_over_animation() {
    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        bool rowChanged = false;
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (ctx->board[x][y] != 0) {
                ctx->board[x][y] = TILE_ID_GARBAGE; // ID 8 (Shadow)
                rowChanged = true;
            }
        }
        if (rowChanged) {
            ctx->needsBoardDraw = true;
            // WICHTIG: Falls der Fehler bleibt, nenne view_draw_board() hier 
            // exakt so, wie die Zeichen-Funktion in deiner game_view.c heißt!
            drawBoard(); 
            for(u16 i = 0; i < 3; i++) SYS_doVBlankProcess();
        }
    }
    for(u16 i = 0; i < 40; i++) SYS_doVBlankProcess();
    
    config.currentScore = ctx->score;
    currentState = STATE_GAMEOVER;
}

void spawnPiece() {
    if (ctx == NULL) return;

    // 1. Effekt-Timer Check
    if (ctx->badEffectTimer > 0 && (ctx->activeBadEffect <= 2 || ctx->activeBadEffect == EFFECT_I_RAIN)) {
        if (--ctx->badEffectTimer == 0) { ctx->activeBadEffect = EFFECT_NONE; ctx->lastActiveBadEffect = 99; }
    }

    // 2. Stein-Wahl
    if (ctx->activeBadEffect == EFFECT_I_RAIN) {
        ctx->type = 0;
    } else if (ctx->activeBadEffect != EFFECT_SAME_TILES) {
        ctx->type = ctx->nextType;
        if (config.randMode == 0) {
            ctx->nextType = ctx->bag[ctx->bagIndex++];
            if (ctx->bagIndex >= 7) refillBag();
        } else {
            ctx->nextType = random() % 7;
        }
    }

    ctx->rotation = 0;
    ctx->pieceX = 3;
    ctx->pieceY = (ctx->type == 0) ? -1 : 0;
    ctx->canHold = true;

    // 3. Item-Spawning (Hier wird die statische Funktion jetzt benutzt!)
    handle_item_spawn_logic();

    // 4. Ghost & Collision
if (GET_FLAG(config.flags, FLAG_SHADOW)) {
    calculate_ghost_y();
    ctx->needsBoardDraw = true; // Wichtig: Board muss neu gemalt werden
}
if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation)) {
        SOUND_play(SND_GAME_OVER);
        play_game_over_animation(); // Animation hier ebenfalls einfügen
    }
}

bool handle_active_animations(GameContext* ctx) {
    if (ctx == NULL) return false;

    // 1. Line-Clear Blinken (Pausiert das Spiel)
    if (ctx->clearTimer > 0) {
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
            if (ctx->sortingRow == -1) spawnPiece();
        }
        ctx->needsBoardDraw = true;
        return true; 
    }

    // 2. Board-Verwandlungen (Shadow oder Rainbow)
    if (ctx->sortingRow != -1) {
        u16 y = ctx->sortingRow;
        bool isVisual = (ctx->activeBadEffect == EFFECT_RAINBOW || ctx->activeBadEffect == EFFECT_SHADOW_BOARD);

        if (ctx->activeBadEffect == EFFECT_RAINBOW) handle_rainbow_row(y);
        else if (ctx->activeBadEffect == EFFECT_SHADOW_BOARD) handle_shadow_row(y);
        else handle_sort_row(y);

        ctx->sortingRow++;
        ctx->needsBoardDraw = true;

        if (ctx->sortingRow >= BOARD_HEIGHT) {
            ctx->sortingRow = -1;
            if (!isVisual) spawnPiece(); // Nur bei Sortierung pausieren
        }
        if (!isVisual) return true; // Nur bei Sortierung Steuerung blockieren
    }

    // 3. Die Kette: Wenn Schatten aktiv ist, Timer runterzählen
    if (ctx->activeBadEffect == EFFECT_SHADOW_BOARD && ctx->badEffectTimer > 0) {
        ctx->badEffectTimer--;
        
        // Wenn die 5 Sekunden um sind -> Umschalten auf Rainbow
        if (ctx->badEffectTimer == 0) {
            ctx->activeBadEffect = EFFECT_RAINBOW;
            ctx->sortingRow = 0; // Starte neue Animation: Licht kommt zurück
            set_game_comment("RAINBOW REBIRTH!", 90);
            SOUND_play(SND_GOOD_ITEM);
        }
    }

    return false; 
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

    // Effekte zünden
    if (ctx->heartTriggered) {
        triggerGoodEffect();
        ctx->heartTriggered = false;
    }
    if (ctx->skullTriggered) {
        triggerBadEffect();
        ctx->skullTriggered = false;
    }
}

void performHold() {
if (!GET_FLAG(config.flags, FLAG_HOLD) || !ctx->canHold) return;
    if (ctx->itemSlot < 4 && ctx->itemType == ITEM_ID_SKULL) { triggerBadEffect(); ctx->itemSlot = 255; }
    if (ctx->activeBadEffect == EFFECT_SAME_TILES) {
        if (ctx->holdType != -1) { ctx->holdType = -1; ctx->lastHoldType = -2; SOUND_play(SND_BAD_ITEM); }
        ctx->canHold = false; return; 
    }
    SOUND_play(SND_HOLD);
    if (ctx->holdType == -1) { ctx->holdType = ctx->type; spawnPiece(); } 
    else {
        s16 temp = ctx->type; ctx->type = ctx->holdType; ctx->holdType = temp;
        ctx->pieceX = 3; ctx->pieceY = 0; ctx->rotation = 0;
    }
    ctx->itemSlot = 255; ctx->canHold = false;
}

void addGarbageLine() {
    if (ctx == NULL) return;
    for (u16 y = 0; y < BOARD_HEIGHT - 1; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) ctx->board[x][y] = ctx->board[x][y + 1];
    }
    u8 randomColor = 1 + (random() % 7); 
    u16 holeX = random() % BOARD_WIDTH;
    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        ctx->board[x][BOARD_HEIGHT - 1] = (x == holeX) ? 0 : randomColor;
    }
    if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation)) {
        if (!checkCollision(ctx->pieceX, ctx->pieceY - 1, ctx->rotation)) ctx->pieceY--;
        else currentState = STATE_GAMEOVER;
    }
    if (GET_FLAG(config.flags, FLAG_SHADOW)) calculate_ghost_y();
    SOUND_play(SND_GARBAGE);
    ctx->needsBoardDraw = true;
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

    // Board leeren
    memset(ctx->board, 0, sizeof(ctx->board));
    
    // Status zurücksetzen
    ctx->score = 0;
    ctx->linesTotal = 0;
    ctx->level = ctx->startLevel;
    menu_bg_set_intensity(1);
    ctx->comboCount = 0;
    ctx->activeBadEffect = EFFECT_NONE;
    ctx->badEffectTimer = 0;
    ctx->sortingRow = -1;
    
    // Bag für neues Spiel mischen
    refillBag();
    ctx->nextType = ctx->bag[ctx->bagIndex++];
}

static void handle_rainbow_row(u16 y) {
    u8 rowColor = (y % 7) + 1;
    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        // Jetzt werden auch graue Schatten-Tiles (ID 8) wieder bunt
        if (ctx->board[x][y] != 0 && ctx->board[x][y] <= 10) {
            ctx->board[x][y] = rowColor;
        }
    }
}

static void handle_shadow_row(u16 y) {
    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        if (ctx->board[x][y] != 0 && ctx->board[x][y] < 10) {
            ctx->board[x][y] = 8; // Graue Tiles
        }
    }
}

static void handle_sort_row(u16 y) {
    u8 tempRow[BOARD_WIDTH];
    u16 filled = 0;
    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        if (ctx->board[x][y] != 0) tempRow[filled++] = ctx->board[x][y];
    }
    for (u16 x = 0; x < filled; x++) ctx->board[x][y] = tempRow[x];
    for (u16 x = filled; x < BOARD_WIDTH; x++) ctx->board[x][y] = 0;
}

void trigger_line_items(u16 y) {
    for (u16 x = 0; x < BOARD_WIDTH; x++) {
        if (ctx->board[x][y] == ITEM_ID_SKULL) triggerBadEffect();
        if (ctx->board[x][y] == ITEM_ID_HEART) ctx->heartTriggered = true;
    }
}

void update_board_animations() {
    // Sicherstellen, dass wir in einer gültigen Reihe sind
    if (ctx->sortingRow == -1 || ctx->sortingRow > 19) return;

    u16 y = ctx->sortingRow;

    // Wir schreiben die Zeile vom Puffer ins Board
    for (u16 x = 0; x < 10; x++) {
        ctx->board[x][y] = sortBuffer[x][y];
    }

    // Optional: Horizontal aufräumen
    handle_sort_row(y);

    // Erhöhe den Index für den nächsten Frame
    ctx->sortingRow++;

    // Erst wenn wir Reihe 19 fertig geschrieben haben, beenden wir
    if (ctx->sortingRow >= 20) {
        ctx->sortingRow = -1; // Animation Ende
        
        if (clearLines() == 0) {
            spawnPiece();
        }
    }

    ctx->needsBoardDraw = true;
}

void handle_game_over() {
    SOUND_play(SND_GAME_OVER);
    finalize_game_session();
    view_animate_grayscale();
    
    for (u16 i = 0; i < 30; i++) SYS_doVBlankProcess();
    
    view_fade_out_frame();
    currentState = STATE_GAMEOVER;
}