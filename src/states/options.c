#include <genesis.h>
#include <string.h>
#include "gfx.h"
#include "states/options.h"
#include "states/states.h"
#include "sound_manager.h"
#include "menu_bg.h"
#include "fonts.h"
#include "save_manager.h"

static OptionsContext* ctx = NULL;

static void draw_option_line(u16 row, char* label, u16 currentVal, char* options[], u16 numOptions, bool isSelected) {
    u16 y = 10 + (row << 1); 
    u16 x = 9; 
    VDP_setTextPalette(isSelected ? PAL1 : PAL3);
    VDP_drawText(isSelected ? ">" : " ", x - 2, y);
    VDP_drawText(label, x, y);
    x += 14;
    for (u16 i = 0; i < numOptions; i++) {
        VDP_setTextPalette((i == currentVal) ? PAL2 : PAL3);
        VDP_drawText(options[i], x, y);
        x += strlen(options[i]) + 2;
    }
}

void options_init() {
    ctx = &sctx->options;
    ctx->cursor = 0;
    ctx->subCursor = 0; 
    ctx->flags = config.flags;
    ctx->thresholdLR = config.thresholdLR;
    ctx->thresholdSD = config.thresholdSD;
    ctx->needsRedraw = true;
    menu_bg_set_mode(BG_MODE_MENU);
    menu_bg_set_active(GET_FLAG(ctx->flags, FLAG_BG));
    SYS_showFrameLoad(GET_FLAG(ctx->flags, FLAG_DEBUG));
}

void options_init_draw() {
    if (ctx == NULL) return;
    UI_init_fonts_and_palettes();
    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL1);
    VDP_drawText("--- OPTIONS ---", 12, 6);
    VDP_setTextPalette(PAL3);
    VDP_drawText("B TO SAVE & RETURN", 11, 25);
}

void options_update() {
    if (ctx == NULL) return;

    if ((joyState & BUTTON_DOWN) && !(lastJoyState & BUTTON_DOWN)) {
        ctx->cursor = (ctx->cursor + 1) % 6;
        SOUND_play(SND_MOVE);
        ctx->needsRedraw = true;
    }
    if ((joyState & BUTTON_UP) && !(lastJoyState & BUTTON_UP)) {
        ctx->cursor = (ctx->cursor == 0) ? 5 : ctx->cursor - 1;
        SOUND_play(SND_MOVE);
        ctx->needsRedraw = true;
    }

    bool goRight  = (joyState & BUTTON_RIGHT) && !(lastJoyState & BUTTON_RIGHT);
    bool goLeft   = (joyState & BUTTON_LEFT)  && !(lastJoyState & BUTTON_LEFT);
    bool pressedA = (joyState & BUTTON_A)     && !(lastJoyState & BUTTON_A);

    if (goRight || goLeft || pressedA) {
        ctx->needsRedraw = true;
        if (ctx->cursor == 2) { 
            if (pressedA) { ctx->subCursor = (ctx->subCursor + 1) % 2; SOUND_play(SND_MOVE); }
            else {
                SOUND_play(SND_ROTATE);
                if (ctx->subCursor == 0) {
                    if (goRight && ctx->thresholdLR > 2) ctx->thresholdLR--;
                    else if (goLeft && ctx->thresholdLR < 20) ctx->thresholdLR++;
                } else {
                    if (goRight && ctx->thresholdSD > 1) ctx->thresholdSD--;
                    else if (goLeft && ctx->thresholdSD < 10) ctx->thresholdSD++;
                }
            }
        } else if (ctx->cursor == 5) {
            if (pressedA || goLeft || goRight) {
                if (goLeft || (pressedA && ctx->subCursor == 1)) ctx->subCursor = 0;
                else if (goRight || (pressedA && ctx->subCursor == 0)) ctx->subCursor = 1;

                if (pressedA) {
                    if (ctx->subCursor == 0) { // RELOAD
                        save_load();
                        options_init(); // Re-Init lokaler Context
                        SOUND_play(SND_RESET);
                    } else { // RESET DEFAULTS
                        ctx->thresholdLR = 10; ctx->thresholdSD = 2;
                        ctx->flags = (FLAG_SHADOW | FLAG_HOLD | FLAG_NEXT | FLAG_SOUND | FLAG_MUSIC | FLAG_BG);
                        if (config.flags & FLAG_IS_PAL) ctx->flags |= FLAG_IS_PAL;
                        SYS_showFrameLoad(FALSE);
                        menu_bg_set_active(GET_FLAG(ctx->flags, FLAG_BG));
                        if (GET_FLAG(ctx->flags, FLAG_MUSIC)) SOUND_playMusic(); else XGM_stopPlay();
                        SOUND_play(SND_RESET);
                    }
                } else { SOUND_play(SND_MOVE); }
            }
        } else {
            SOUND_play(SND_ROTATE);
            switch(ctx->cursor) {
                case 0: TOGGLE_FLAG(ctx->flags, FLAG_MUSIC); if (GET_FLAG(ctx->flags, FLAG_MUSIC)) SOUND_playMusic(); else XGM_stopPlay(); break;
                case 1: TOGGLE_FLAG(ctx->flags, FLAG_SOUND); break;
                case 3: TOGGLE_FLAG(ctx->flags, FLAG_BG); menu_bg_set_active(GET_FLAG(ctx->flags, FLAG_BG)); break;
                case 4: TOGGLE_FLAG(ctx->flags, FLAG_DEBUG); SYS_showFrameLoad(GET_FLAG(ctx->flags, FLAG_DEBUG) ? TRUE : FALSE); break;
            }
        }
    }

    if ((joyState & (BUTTON_START | BUTTON_B)) && !(lastJoyState & (BUTTON_START | BUTTON_B))) {
        config.flags = (ctx->flags & ~FLAG_IS_PAL) | (config.flags & FLAG_IS_PAL);
        config.thresholdLR = ctx->thresholdLR;
        config.thresholdSD = ctx->thresholdSD;
        save_options();
        currentState = STATE_TITLE;
    }
}

void options_draw() {
    if (ctx == NULL || !ctx->needsRedraw) return;
    char* optsOnOff[] = {"OFF", "ON "};
    char valLR[4], valSD[4];
    uintToStr(ctx->thresholdLR, valLR, 2);
    uintToStr(ctx->thresholdSD, valSD, 2);

    draw_option_line(0, "MUSIC:", GET_FLAG(ctx->flags, FLAG_MUSIC) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 0));
    draw_option_line(1, "SOUND:", GET_FLAG(ctx->flags, FLAG_SOUND) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 1));

    u16 ySens = 14, xSens = 9;
    bool isSensSelected = (ctx->cursor == 2);
    VDP_setTextPalette(isSensSelected ? PAL1 : PAL3);
    VDP_drawText(isSensSelected ? ">" : " ", xSens - 2, ySens);
    VDP_drawText("SENSIBILITY:", xSens, ySens);
    VDP_setTextPalette((isSensSelected && ctx->subCursor == 0) ? PAL2 : PAL3);
    VDP_drawText("<->", xSens + 14, ySens); VDP_drawText(valLR, xSens + 18, ySens);
    VDP_setTextPalette((isSensSelected && ctx->subCursor == 1) ? PAL2 : PAL3);
    VDP_drawText("v", xSens + 22, ySens); VDP_drawText(valSD, xSens + 24, ySens);

    draw_option_line(3, "BACKGRD:", GET_FLAG(ctx->flags, FLAG_BG) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 3));
    draw_option_line(4, "DEBUG   :", GET_FLAG(ctx->flags, FLAG_DEBUG) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 4));

    VDP_setTextPalette(PAL3);
    VDP_drawText("SYSTEM:", 9, 21);
    VDP_drawText(GET_FLAG(config.flags, FLAG_IS_PAL) ? "PAL " : "NTSC", 23, 21);

    bool isResetRow = (ctx->cursor == 5);
    VDP_setTextPalette(isResetRow ? PAL1 : PAL3);
    VDP_drawText(isResetRow ? ">" : " ", 7, 23);
    VDP_setTextPalette((isResetRow && ctx->subCursor == 0) ? PAL2 : PAL3);
    VDP_drawText("RELOAD OPTIONS", 9, 23);
    VDP_setTextPalette((isResetRow && ctx->subCursor == 1) ? PAL2 : PAL3);
    VDP_drawText("RESET DEFAULTS", 25, 23);

    ctx->needsRedraw = false;
}

void options_cleanup() {
    VDP_clearTextArea(0, 0, 40, 28);
    ctx = NULL;
}