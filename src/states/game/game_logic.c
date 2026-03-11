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


void triggerManualSort() {
    // Nur starten, wenn nicht gerade schon eine Sortierung läuft
    if (ctx == NULL || ctx->sortingRow != -1) return;

    ctx->sortingRow = 0; // Startet die Animation bei der obersten Zeile
    strncpy(ctx->lastComment, "SELECT SORT!", 20);
    ctx->commentTimer = 60;
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
    ctx->level = ctx->startLevel + (ctx->linesTotal / 10);
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
            ctx->badEffectTimer = 5; 
            // Füge das hier ein:
            set_game_comment("FULL SPEED!", 90); 
            break;
            case EFFECT_SAME_TILES: 
            ctx->badEffectTimer = 5; 
            strncpy(ctx->lastComment, "SPEED / SAME!", 20); 
            break;
        case EFFECT_NO_ROTATE:  
            ctx->badEffectTimer = 180; 
            strncpy(ctx->lastComment, "NO ROTATE!", 20); 
            break;
        case EFFECT_REVERSED:   
            ctx->badEffectTimer = 240; 
            strncpy(ctx->lastComment, "REVERSED!", 20); 
            break;
        case EFFECT_HOLD_LOCK:  
            ctx->badEffectTimer = 300; 
            ctx->holdType = -1; 
            ctx->canHold = false; 
            strncpy(ctx->lastComment, "HOLD LOCKED!", 20); 
            break;
        case EFFECT_HIDE_NEXT:  
            ctx->badEffectTimer = 300; 
            strncpy(ctx->lastComment, "NEXT HIDDEN!", 20); 
            break;
        case EFFECT_SHADOW_BOARD: 
            ctx->sortingRow = 0; 
            strncpy(ctx->lastComment, "DARK CURSE!", 20); 
            break;
    }
    SOUND_play(SND_BAD_ITEM);
    ctx->commentTimer = 60;
    ctx->lastActiveBadEffect = 99;
}



void lockPiece() {
    bool lockedAbove = false;
    transfer_piece_to_board(); // Hier wird die Funktion jetzt benutzt -> Warnung weg!
    
    for (u16 i = 0; i < 4; i++) {
        if (ctx->pieceY + PIECES[ctx->type][ctx->rotation][i][1] < 0) lockedAbove = true;
    }
    
    SOUND_play(SND_PIECE_LOCK);
    
    if (lockedAbove) {
        SOUND_play(SND_GAME_OVER);
        play_game_over_animation();
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
            for (u16 x = 0; x < BOARD_WIDTH; x++) if (ctx->board[x][y] == ITEM_ID_SKULL) ctx->board[x][y] = 0;
        }
        strncpy(ctx->lastComment, "SKULLS PURGED!", 20);
    }
    else if (chance == 3) { ctx->activeBadEffect = EFFECT_I_RAIN; ctx->badEffectTimer = 4; strncpy(ctx->lastComment, "I-BEAM RAIN!", 20); }
    else if (chance == 4) { ctx->activeBadEffect = EFFECT_FREEZE; ctx->badEffectTimer = 600; strncpy(ctx->lastComment, "TIME FREEZE!", 20); }
    else { ctx->activeBadEffect = EFFECT_RAINBOW; ctx->sortingRow = 0; strncpy(ctx->lastComment, "RAINBOW POWER!", 20); }

    for (u16 y = 0; y < BOARD_HEIGHT; y++) {
        for (u16 x = 0; x < BOARD_WIDTH; x++) if (ctx->board[x][y] == ITEM_ID_HEART) ctx->board[x][y] = 0;
    }
    SOUND_play(SND_GOOD_ITEM);
    ctx->commentTimer = 60;
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
            if (config.showShadow) calculate_ghost_y();
            
            return true; // Rotation erfolgreich
        }
    }
    return false; // Rotation blockiert
}




// In clearLines: Die Logik für das "Ghost"-Blinken (ID 0)
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
            if (h == 0 && s == 0) {
                f = 0; // Absolute Neutralität -> Ghost Tile (ID 0)
            } else if (h > s) {
                f = ITEM_ID_HEART; // 11
                ctx->heartTriggered = true;
            } else if (s > h) {
                f = ITEM_ID_SKULL; // 10
                ctx->skullTriggered = true;
            } else {
                f = TILE_ID_FLASH; // Gleichstand (z.B. 1:1) -> Flash (11)
            }

            for (u16 x = 0; x < BOARD_WIDTH; x++) ctx->board[x][y] = f;
        }
    }

    if (linesCleared > 0) {
        apply_scoring(linesCleared);
        ctx->comboCount++;
        ctx->clearTimer = 20;
        SOUND_play(52 + ctx->comboCount); // Sound nur einmal beim Löschen
    } else ctx->comboCount = 0;

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
    if (config.showShadow) calculate_ghost_y();

if (checkCollision(ctx->pieceX, ctx->pieceY, ctx->rotation)) {
        SOUND_play(SND_GAME_OVER);
        play_game_over_animation(); // Animation hier ebenfalls einfügen
    }
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
    if (!config.allowHold || !ctx->canHold) return;
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
        if (ctx->board[x][y] != 0 && ctx->board[x][y] < 10) {
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
    if (ctx->sortingRow == -1) return;

    u16 y = ctx->sortingRow;
    if (ctx->activeBadEffect == EFFECT_RAINBOW) {
        handle_rainbow_row(y);
    } else if (ctx->activeBadEffect == EFFECT_SHADOW_BOARD) {
        handle_shadow_row(y);
    } else {
        handle_sort_row(y);
    }

    ctx->sortingRow++;
    if (ctx->sortingRow >= BOARD_HEIGHT) {
        ctx->sortingRow = -1;
        if (ctx->activeBadEffect == EFFECT_RAINBOW || ctx->activeBadEffect == EFFECT_SHADOW_BOARD) {
            ctx->activeBadEffect = EFFECT_NONE;
        }
        spawnPiece();
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