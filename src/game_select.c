#include <genesis.h>
#include "game_select.h"
#include "states.h"
#include "sound_manager.h"
#include <string.h>
#include "menu_bg.h"
#include "fonts.h"

typedef struct SelectContext {
    u16 cursor;
    char name[4];
    u16 nameCharIdx;
    u16 randMode;
    u16 speedLevel;
    u16 garbageFreq;
    bool showShadow;
    bool allowHold;
    bool showNext;
    bool needsRedraw;
} SelectContext;

static SelectContext* ctx = NULL;

static void draw_menu_line(u16 row, char* label, u16 currentVal, char* options[], u16 numOptions, bool isSelected) {
    u16 y = 8 + (row * 2);
    u16 x = 4;

    VDP_setTextPalette(PAL1);
    VDP_drawText(isSelected ? ">" : " ", x, y);
    x += 2;

    VDP_setTextPalette(PAL1);
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

    VDP_setTextPalette(PAL1);
    VDP_drawText(isSelected ? ">" : " ", x, y);
    x += 2;

    VDP_setTextPalette(PAL1);
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

void select_init() {
    ctx = MEM_alloc(sizeof(SelectContext));
    ctx->cursor = 0;
    strncpy(ctx->name, config.playerName, 3);
    ctx->name[3] = '\0';
    ctx->nameCharIdx = 0;
    ctx->randMode = config.randMode;
    ctx->speedLevel = config.speedLevel;
    ctx->garbageFreq = config.garbageFreq;
    ctx->showShadow = config.showShadow;
    ctx->allowHold = config.allowHold;
    ctx->showNext = config.showNext;

    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);

    PAL_setPalette(PAL2, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0x440000));
    PAL_setColor(37, RGB24_TO_VDPCOLOR(0x880000));
    PAL_setColor(38, RGB24_TO_VDPCOLOR(0xFF0000));
    PAL_setColor(39, RGB24_TO_VDPCOLOR(0xFF8888));

    PAL_setPalette(PAL1, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(21, RGB24_TO_VDPCOLOR(0x666600));
    PAL_setColor(22, RGB24_TO_VDPCOLOR(0xFFFF00));
    PAL_setColor(23, RGB24_TO_VDPCOLOR(0x666600));

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL1);
    VDP_drawText("--- GAME SETTINGS ---", 10, 4);
    VDP_setTextPalette(PAL3);
    VDP_drawText("START TO BEGIN", 13, 25);

    ctx->needsRedraw = true;
    menu_bg_set_active(true);
}

void select_update() {
    if (ctx == NULL) return;

    if ((joyState & BUTTON_DOWN) && !(lastJoyState & BUTTON_DOWN)) {
        ctx->cursor = (ctx->cursor + 1) % 7;
        SOUND_play(SND_MOVE);
        ctx->needsRedraw = true;
    }
    if ((joyState & BUTTON_UP) && !(lastJoyState & BUTTON_UP)) {
        ctx->cursor = (ctx->cursor == 0) ? 6 : ctx->cursor - 1;
        SOUND_play(SND_MOVE);
        ctx->needsRedraw = true;
    }

    bool goRight = (joyState & BUTTON_RIGHT) && !(lastJoyState & BUTTON_RIGHT);
    bool goLeft = (joyState & BUTTON_LEFT) && !(lastJoyState & BUTTON_LEFT);
    bool pressedA = (joyState & BUTTON_A) && !(lastJoyState & BUTTON_A);

    if (goRight || goLeft || pressedA) {
        ctx->needsRedraw = true;
        if (ctx->cursor == 0) {
            char c = ctx->name[ctx->nameCharIdx];
            if (goRight) c = (c == 'Z') ? 'A' : c + 1;
            else if (goLeft) c = (c == 'A') ? 'Z' : c - 1;
            ctx->name[ctx->nameCharIdx] = c;
            if (pressedA) {
                ctx->nameCharIdx = (ctx->nameCharIdx + 1) % 3;
                SOUND_play(SND_MOVE);
            } else {
                SOUND_play(SND_ROTATE);
            }
        } else {
            SOUND_play(SND_ROTATE);
            s16 dir = goLeft ? -1 : 1;
            switch(ctx->cursor) {
                case 1: ctx->randMode = (ctx->randMode + dir + 2) % 2; break;
                case 2: ctx->speedLevel = (ctx->speedLevel + dir + 4) % 4; break;
                case 3: ctx->garbageFreq = (ctx->garbageFreq + dir + 4) % 4; break;
                case 4: ctx->showShadow = !ctx->showShadow; break;
                case 5: ctx->allowHold = !ctx->allowHold; break;
                case 6: ctx->showNext = !ctx->showNext; break;
            }
        }
    }

    if (ctx->needsRedraw) {
        draw_name_entry(ctx->cursor == 0);
        char* optsRand[] = {"Fair", "Chaos"};
        char* optsLevels[] = {"None", "Slow", "Med", "Fast"};
        char* optsOnOff[] = {"Off", "On"};
        draw_menu_line(1, "Random:", ctx->randMode, optsRand, 2, (ctx->cursor == 1));
        draw_menu_line(2, "Speed:", ctx->speedLevel, optsLevels, 4, (ctx->cursor == 2));
        draw_menu_line(3, "Garbage:", ctx->garbageFreq, optsLevels, 4, (ctx->cursor == 3));
        draw_menu_line(4, "Shadow:", ctx->showShadow, optsOnOff, 2, (ctx->cursor == 4));
        draw_menu_line(5, "Hold:", ctx->allowHold, optsOnOff, 2, (ctx->cursor == 5));
        draw_menu_line(6, "Next:", ctx->showNext, optsOnOff, 2, (ctx->cursor == 6));
        ctx->needsRedraw = false;
    }

    if ((joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
        strncpy(config.playerName, ctx->name, 3);
        config.playerName[3] = '\0';
        config.randMode = ctx->randMode;
        config.speedLevel = ctx->speedLevel;
        config.garbageFreq = ctx->garbageFreq;
        config.showShadow = ctx->showShadow;
        config.allowHold = ctx->allowHold;
        config.showNext = ctx->showNext;
        currentState = STATE_GAME;
    }
}

void select_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearTextArea(0, 0, 40, 28);
}