#include <genesis.h>
#include <string.h>
#include "gfx.h"
#include "states/options.h"
#include "states/states.h"
#include "sound_manager.h"
#include "menu_bg.h"
#include "fonts.h"

static OptionsContext* ctx = NULL;

// Hilfsfunktion für das Zeichnen der Zeilen
static void draw_option_line(u16 row, char* label, u16 currentVal, char* options[], u16 numOptions, bool isSelected) {
    u16 y = 10 + (row << 1); // Bitshift statt Multiplikation
    u16 x = 6;

    VDP_setTextPalette(isSelected ? PAL1 : PAL3);
    VDP_drawText(isSelected ? ">" : " ", x - 2, y);
    VDP_drawText(label, x, y);

    x += 14;

    for (u16 i = 0; i < numOptions; i++) {
        if (i == currentVal) VDP_setTextPalette(PAL2); // Highlight
        else VDP_setTextPalette(PAL3);

        VDP_drawText(options[i], x, y);
        x += strlen(options[i]) + 2;
    }
}

void options_init() {
    // Brücke zur globalen Union
    ctx = &sctx->options;

    // Initialisierung der Werte
    ctx->cursor = 0;
    ctx->flags = config.flags;
    ctx->needsRedraw = true;

    // Hintergrund-System für Options-Screen konfigurieren
    menu_bg_set_mode(BG_MODE_MENU);
    menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));
}

void options_init_draw() {
    if (ctx == NULL) return;

    // Fonts und Paletten sicherstellen
    UI_init_fonts_and_palettes();

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL1);
    VDP_drawText("--- OPTIONS ---", 12, 6);
    
    VDP_setTextPalette(PAL3);
    VDP_drawText("B TO RETURN", 14, 25);
}

void options_update() {
    if (ctx == NULL) return;

    // 1. Navigation
    if ((joyState & BUTTON_DOWN) && !(lastJoyState & BUTTON_DOWN)) {
        ctx->cursor = (ctx->cursor + 1) % 4;
        SOUND_play(SND_MOVE);
        ctx->needsRedraw = true;
    }
    if ((joyState & BUTTON_UP) && !(lastJoyState & BUTTON_UP)) {
        ctx->cursor = (ctx->cursor == 0) ? 3 : ctx->cursor - 1;
        SOUND_play(SND_MOVE);
        ctx->needsRedraw = true;
    }

    // 2. Manipulation
    bool goRight  = (joyState & BUTTON_RIGHT) && !(lastJoyState & BUTTON_RIGHT);
    bool goLeft   = (joyState & BUTTON_LEFT)  && !(lastJoyState & BUTTON_LEFT);
    bool pressedA = (joyState & BUTTON_A)     && !(lastJoyState & BUTTON_A);

    if (goRight || goLeft || pressedA) {
        ctx->needsRedraw = true;
        SOUND_play(SND_ROTATE);

        switch(ctx->cursor) {
            case 0: // MUSIC
                TOGGLE_FLAG(ctx->flags, FLAG_MUSIC);
                if (GET_FLAG(ctx->flags, FLAG_MUSIC)) {
                    SOUND_playMusic();
                } else {
                    // Falls SOUND_stopMusic fehlt, nutze XGM_stopPlay()
                    XGM_stopPlay(); 
                }
                break;

            case 1: // SOUND
                TOGGLE_FLAG(ctx->flags, FLAG_SOUND);
                break;

            case 2: // BACKGROUND
                TOGGLE_FLAG(ctx->flags, FLAG_BG);
                // Live-Feedback für den Hintergrund
                menu_bg_set_active(GET_FLAG(ctx->flags, FLAG_BG));
                break;

            case 3: // SYSTEM
                break;
        }
    }

    // Zurück zum Titel und Übernahme der Flags
    if ((joyState & (BUTTON_START | BUTTON_B)) && !(lastJoyState & (BUTTON_START | BUTTON_B))) {
        config.flags = (ctx->flags & ~FLAG_IS_PAL) | (config.flags & FLAG_IS_PAL);
        currentState = STATE_TITLE;
    }
}

void options_draw() {
    if (ctx == NULL || !ctx->needsRedraw) return;

    char* optsOnOff[] = {"OFF", "ON "};
    char* optsSystem[] = {"NTSC", "PAL "};

    draw_option_line(0, "MUSIC:", GET_FLAG(ctx->flags, FLAG_MUSIC) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 0));
    draw_option_line(1, "SOUND:", GET_FLAG(ctx->flags, FLAG_SOUND) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 1));
    draw_option_line(2, "BACKGRD:", GET_FLAG(ctx->flags, FLAG_BG) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 2));
    draw_option_line(3, "SYSTEM:", GET_FLAG(config.flags, FLAG_IS_PAL) ? 1 : 0, optsSystem, 2, (ctx->cursor == 3));

    ctx->needsRedraw = false;
}

void options_cleanup() {
    VDP_clearTextArea(0, 0, 40, 28);
    ctx = NULL;
}