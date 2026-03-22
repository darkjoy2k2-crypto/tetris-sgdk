#include <genesis.h>
#include <string.h>
#include "states/game_select.h"
#include "states/states.h"
#include "states/game/game_conditions.h"
#include "sound_manager.h"
#include "menu_bg.h"
#include "fonts.h"
#include "gfx.h"

static SelectContext* ctx = NULL;
static char* optsDigits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

// --- Private Zeichen-Helfer ---

static void draw_menu_line(u16 row, char* label, u16 currentVal, char* options[], u16 numOptions, bool isSelected) {
    u16 y = 8 + (row * 2);
    u16 x = 4;

    VDP_setTextPalette(isSelected ? PAL1 : PAL3);
    VDP_drawText(isSelected ? ">" : " ", x, y);
    x += 2;

    VDP_drawText(label, x, y);
    x += 12;

    for (u16 i = 0; i < numOptions; i++) {
        if (i == currentVal) VDP_setTextPalette(PAL2);
        else VDP_setTextPalette(PAL3);

        VDP_drawText(options[i], x, y);
        x += strlen(options[i]) + 1;
    }
}

static void draw_name_entry(bool isSelected) {
    u16 y = 8;
    u16 x = 4;

    VDP_setTextPalette(isSelected ? PAL1 : PAL3);
    VDP_drawText(isSelected ? ">" : " ", x, y);
    x += 2;

    VDP_drawText("Player:", x, y);
    x += 12;

    for (u16 i = 0; i < 3; i++) {
        if (isSelected && ctx->nameCharIdx == i) VDP_setTextPalette(PAL2);
        else VDP_setTextPalette(PAL3);

        char letter[2] = { ctx->name[i], '\0' };
        VDP_drawText(letter, x, y);
        x += 2;
    }
}

// --- State System Funktionen ---

void select_init() {
    ctx = &sctx->select;

    menu_bg_set_mode(BG_MODE_MENU);
    menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));

    ctx->cursor = 0;
    strncpy(ctx->name, config.playerName, 3);
    ctx->name[3] = '\0';
    ctx->nameCharIdx = 0;
    
    ctx->randMode = config.randMode;
    ctx->speedLevel = (config.speedLevel > SPEED_LEVEL_MAX) ? SPEED_LEVEL_MAX : config.speedLevel;
    ctx->garbageFreq = (config.garbageFreq > GARBAGE_FREQ_MAX) ? GARBAGE_FREQ_MAX : config.garbageFreq;
    ctx->itemMode = config.itemMode;
    ctx->flags = config.flags;

    ctx->needsRedraw = true;
    KLog("SELECT: Init finished.");
}

void select_init_draw() {
    if (ctx == NULL) return;

    UI_init_fonts_and_palettes();   

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL1);
    VDP_drawText("--- GAME SETTINGS ---", 10, 4);
    VDP_setTextPalette(PAL3);
    VDP_drawText("START TO BEGIN", 13, 27);
}

void select_update() {
    if (ctx == NULL) return;

    if ((joyState & BUTTON_DOWN) && !(lastJoyState & BUTTON_DOWN)) {
        ctx->cursor = (ctx->cursor + 1) % 8;
        SOUND_play(SND_MOVE);
        ctx->needsRedraw = true;
    }
    if ((joyState & BUTTON_UP) && !(lastJoyState & BUTTON_UP)) {
        ctx->cursor = (ctx->cursor == 0) ? 7 : ctx->cursor - 1;
        SOUND_play(SND_MOVE);
        ctx->needsRedraw = true;
    }

    bool goRight  = (joyState & BUTTON_RIGHT) && !(lastJoyState & BUTTON_RIGHT);
    bool goLeft   = (joyState & BUTTON_LEFT)  && !(lastJoyState & BUTTON_LEFT);
    bool pressedA = (joyState & BUTTON_A)     && !(lastJoyState & BUTTON_A);

    if (goRight || goLeft || pressedA) {
        ctx->needsRedraw = true;
        
        if (ctx->cursor == 0) {
            char c = ctx->name[ctx->nameCharIdx];
            if (goRight) {
                c = (c == 'Z') ? 'A' : c + 1;
                SOUND_play(SND_ROTATE);
            } else if (goLeft) {
                c = (c == 'A') ? 'Z' : c - 1;
                SOUND_play(SND_ROTATE);
            }
            ctx->name[ctx->nameCharIdx] = c;
            
            if (pressedA) {
                ctx->nameCharIdx = (ctx->nameCharIdx + 1) % 3;
                SOUND_play(SND_MOVE);
            }
        } else {
            SOUND_play(SND_ROTATE);
            s16 dir = goLeft ? -1 : 1;
            switch(ctx->cursor) {
                case 1: ctx->randMode    = (ctx->randMode + dir + 2) % 2; break;
                case 2: ctx->speedLevel  = (ctx->speedLevel + dir + (SPEED_LEVEL_MAX + 1)) % (SPEED_LEVEL_MAX + 1); break;
                case 3: ctx->garbageFreq = (ctx->garbageFreq + dir + (GARBAGE_FREQ_MAX + 1)) % (GARBAGE_FREQ_MAX + 1); break;
                case 4: TOGGLE_FLAG(ctx->flags, FLAG_SHADOW); break; 
                case 5: TOGGLE_FLAG(ctx->flags, FLAG_HOLD);   break;
                case 6: TOGGLE_FLAG(ctx->flags, FLAG_NEXT);   break;
                case 7: ctx->itemMode    = (ctx->itemMode + dir + 4) % 4; break;
            }
        }
    }

    if ((joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
        KLog("SELECT: START pressed. Checking for changes...");

        // Vergleich Name
        bool nameChanged = (strcmp(config.playerName, ctx->name) != 0);        // Vergleich Flags (PAL-Flag ignorieren)
        // Vergleich Rest
        bool settingsChanged = (config.randMode != ctx->randMode) || 
                       (config.speedLevel != ctx->speedLevel) || 
                       (config.garbageFreq != ctx->garbageFreq) || 
                       (config.itemMode != ctx->itemMode) ||
                       (config.flags != ctx->flags);

        bool needsSave = (nameChanged || settingsChanged);

        // Werte übertragen
        strncpy(config.playerName, ctx->name, 3);
        config.playerName[3] = '\0';
        config.randMode    = ctx->randMode;
        config.speedLevel  = ctx->speedLevel;
        config.garbageFreq = ctx->garbageFreq;
        config.itemMode    = ctx->itemMode;
        config.flags       = ctx->flags;
        config.runtime.gameMode = GAME_MODE_FREEGAME;
        config.runtime.challengeLevelId = 255;
        config.runtime.challengeResult = CHALLENGE_RESULT_NONE;

        // Feed runtime gameplay conditions before switching state.
        // This keeps STATE_GAME independent from config-copy logic.
        game_conditions_set_from_select(ctx);

        if (needsSave) {
            KLog_U1("SELECT: Changes detected! Name:", nameChanged);
            KLog_U1("SELECT: Logic:", settingsChanged);
            
            config.sramop = SRAM_SAVE;
            config.preferredState = STATE_GAME;
            currentState = STATE_SAVE;
        } else {
            KLog("SELECT: No changes. Direct to game.");
            currentState = STATE_GAME;
        }
    }
}

void select_draw() {
    if (ctx == NULL || !ctx->needsRedraw) return;

    draw_name_entry(ctx->cursor == 0);

    char* optsRand[]   = {"Fair", "Chaos"};
    char* optsOnOff[]  = {"Off", "On"};
    char* optsItems[]  = {"None", "All", "Good", "Bad"};

    draw_menu_line(1, "Random:",   ctx->randMode,   optsRand,   2, (ctx->cursor == 1));
    draw_menu_line(2, "Speed:",    ctx->speedLevel,  optsDigits, 10, (ctx->cursor == 2));
    draw_menu_line(3, "Garbage:",  ctx->garbageFreq, optsDigits, 10, (ctx->cursor == 3));
    draw_menu_line(4, "Shadow:",   GET_FLAG(ctx->flags, FLAG_SHADOW) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 4));
    draw_menu_line(5, "Hold:",     GET_FLAG(ctx->flags, FLAG_HOLD)   ? 1 : 0, optsOnOff, 2, (ctx->cursor == 5));
    draw_menu_line(6, "Next:",     GET_FLAG(ctx->flags, FLAG_NEXT)   ? 1 : 0, optsOnOff, 2, (ctx->cursor == 6));
    draw_menu_line(7, "Items:",    ctx->itemMode,    optsItems,  4, (ctx->cursor == 7));
    
    ctx->needsRedraw = false;
}

void select_cleanup() {
    VDP_clearTextArea(0, 0, 40, 28);
    ctx = NULL;
    KLog("SELECT: Cleanup finished.");
}