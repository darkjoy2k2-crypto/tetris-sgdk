#include <genesis.h>
#include "states/states.h"
#include "states/title.h"
#include "menu_bg.h"
#include "sound_manager.h"
#include "gfx.h"
#include "text_manager.h"

// Der lokale Proxy-Pointer (Brücke zur Union in states.h)
static TitleContext* ctx = NULL;

// Hilfsfunktion für die Menü-Darstellung
static void draw_title_menu_line(u16 row, char* label, bool isSelected) {
    u16 y = 18 + (row * 2); 
    u16 x = 15;

    if (isSelected) {
        VDP_setTextPalette(PAL1); // Gold/Gelb
        VDP_drawText("> ", x - 2, y);
    } else {
        VDP_setTextPalette(PAL3); // Weiß/Grau
        VDP_drawText("  ", x - 2, y);
    }
    
    VDP_drawText(label, x, y);
}

void title_init() {
    ctx = &sctx->title;

    // Menu Hintergrund explizit für diesen State konfigurieren
    menu_bg_init(); 
    menu_bg_set_mode(BG_MODE_MENU);

    ctx->phase = PHASE_BLINK;
    ctx->cursor = 0;
    ctx->blinkTimer = 0;
    ctx->textVisible = true;
    ctx->lastTextVisible = false; 
    ctx->idleTimer = 0;
    ctx->needsRedraw = true;
}

void title_init_draw() {
    ctx = &sctx->title;
    if (ctx == NULL) return;

    UI_init_fonts_and_palettes();
    VDP_clearTextArea(0, 0, 40, 28);
    text_manager_init();
}

void title_update() {
    if (ctx == NULL) return;

    if (ctx->phase == PHASE_BLINK) {
        text_manager_set_enabled(TRUE);

        ctx->blinkTimer++;
        if (ctx->blinkTimer >= 30) {
            ctx->blinkTimer = 0;
            ctx->textVisible = !ctx->textVisible;
        }

        if (text_manager_is_finished()) {
            currentState = STATE_HIGHSCORE;
        }

        if (joyState != 0 && lastJoyState == 0) {
            ctx->phase = PHASE_MENU;
            ctx->needsRedraw = true;
            ctx->textVisible = false;
            text_manager_set_enabled(FALSE);
            SOUND_play(SND_MENU_SELECT);
            VDP_clearTextArea(10, 20, 20, 1);
        }

    } else {
        text_manager_set_enabled(FALSE);

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

        if ((joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
            SOUND_play(SND_MENU_SELECT);
            switch(ctx->cursor) {
                case 0: currentState = STATE_CHALLENGE; break;
                case 1: currentState = STATE_SELECT; break;
                case 2: currentState = STATE_VS; break;
                case 3: currentState = STATE_OPTIONS; break;
                case 4: currentState = STATE_SOUNDTEST; break;
                case 5: currentState = STATE_GFXTEST; break;
            }
        }

        if ((joyState & BUTTON_B) && !(lastJoyState & BUTTON_B)) {
            ctx->phase = PHASE_BLINK;
            ctx->needsRedraw = true;
            ctx->cursor = 0;
            VDP_clearTextArea(8, 18, 24, 10);
        }
    }

    text_manager_update();
}

void title_draw() {
    if (ctx == NULL) return;

    if (ctx->phase == PHASE_BLINK) {
        if (ctx->textVisible != ctx->lastTextVisible) {
            VDP_setTextPalette(PAL3);
            if (ctx->textVisible) {
                VDP_drawText("PRESS START TO PLAY", 10, 20);
            } else {
                VDP_clearTextArea(10, 20, 20, 1);
            }
            ctx->lastTextVisible = ctx->textVisible;
        }
    } else if (ctx->needsRedraw) {
        draw_title_menu_line(0, "CHALLENGE",  (ctx->cursor == 0));
        draw_title_menu_line(1, "FREE GAME",  (ctx->cursor == 1));
        draw_title_menu_line(2, "VS STATE",   (ctx->cursor == 2));
        draw_title_menu_line(3, "OPTIONS",    (ctx->cursor == 3));
        draw_title_menu_line(4, "SOUND TEST", (ctx->cursor == 4));
        draw_title_menu_line(5, "GFX TEST",   (ctx->cursor == 5));
        ctx->needsRedraw = false;
    }
}

void title_cleanup() {
    text_manager_glyphs_visible(FALSE);
    text_manager_cleanup();
    ctx = NULL;
}