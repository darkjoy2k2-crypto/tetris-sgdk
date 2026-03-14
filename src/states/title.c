#include <genesis.h>
#include "states/title.h"
#include "states/states.h"
#include "menu_bg.h"
#include "sound_manager.h"
#include "gfx.h"

typedef enum {
    PHASE_BLINK,
    PHASE_MENU
} TitlePhase;

typedef struct {
    TitlePhase phase;
    u16 cursor;
    u16 blinkTimer;
    bool textVisible;
    bool lastTextVisible;
    u16 idleTimer;
    bool needsRedraw;
} TitleContext;

static TitleContext* ctx = NULL;

// Hilfsfunktion mit goldener/gelber Highlight-Farbe (PAL1 aus deinem Select-Screen)
static void draw_title_menu_line(u16 row, char* label, bool isSelected) {
    u16 y = 18 + (row * 2); 
    u16 x = 15;

    if (isSelected) {
        VDP_setTextPalette(PAL1); // Deine Gold/Gelb Palette
        VDP_drawText("> ", x - 2, y);
    } else {
        VDP_setTextPalette(PAL3); // Standard Weiß/Grau
        VDP_drawText("  ", x - 2, y);
    }
    
    VDP_drawText(label, x, y);
}

void title_init() {
menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));
    // Sicherstellen, dass der Speicher sauber ist
    if (ctx != NULL) MEM_free(ctx);
    ctx = MEM_alloc(sizeof(TitleContext));
    
    // EXPLIZITE INITIALISIERUNG gegen Kaltstart-Fehler
    ctx->phase = PHASE_BLINK;
    ctx->cursor = 0;
    ctx->blinkTimer = 0;
    ctx->textVisible = true;
    ctx->lastTextVisible = false; 
    ctx->idleTimer = 0;
    ctx->needsRedraw = true;
}

void title_init_draw() {
    UI_init_fonts_and_palettes();
    if (ctx == NULL) return;
    VDP_clearTextArea(0, 0, 40, 28);
    
    VDP_setTextPalette(PAL1);
    VDP_drawText("TETRIS VIBE SGDK", 11, 6);
    VDP_setTextPalette(PAL3);
    VDP_drawText("SEE WHAT AI CAN DO FOR YOU", 7, 8);
}

void title_update() {
    if (ctx == NULL) return;

    if (ctx->phase == PHASE_BLINK) {
        ctx->blinkTimer++;
        if (ctx->blinkTimer >= 30) {
            ctx->blinkTimer = 0;
            ctx->textVisible = !ctx->textVisible;
        }

        if (joyState == 0) {
            ctx->idleTimer++;
            if (ctx->idleTimer >= 420) currentState = STATE_HIGHSCORE;
        } else {
            ctx->idleTimer = 0;
        }

        // Wechsel ins Menü
if (joyState != 0 && lastJoyState == 0) {
    ctx->phase = PHASE_MENU;
    ctx->needsRedraw = true;
    ctx->textVisible = false; 
    SOUND_play(SND_MENU_SELECT);
    VDP_clearTextArea(10, 20, 20, 1);
}

    } else {
        // --- MENÜ LOGIK ---
        if ((joyState & BUTTON_DOWN) && !(lastJoyState & BUTTON_DOWN)) {
            ctx->cursor = (ctx->cursor + 1) % 3;
            SOUND_play(SND_MOVE);
            ctx->needsRedraw = true;
        }
        if ((joyState & BUTTON_UP) && !(lastJoyState & BUTTON_UP)) {
            ctx->cursor = (ctx->cursor == 0) ? 2 : ctx->cursor - 1;
            SOUND_play(SND_MOVE);
            ctx->needsRedraw = true;
        }

        if ((joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
            SOUND_play(SND_MENU_SELECT);
            switch(ctx->cursor) {
                case 0: currentState = STATE_SELECT; break;
                case 1: currentState = STATE_OPTIONS; break;
                case 2: currentState = STATE_SOUNDTEST; break;
            }
        }
        
        if ((joyState & BUTTON_B) && !(lastJoyState & BUTTON_B)) {
            ctx->phase = PHASE_BLINK;
            ctx->needsRedraw = true;
            ctx->cursor = 0;
            VDP_clearTextArea(8, 18, 24, 8);
            // Sicherstellen, dass BG-Status korrekt bleibt
            menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));
        }
    }
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
        // Menü mit Gold-Highlight zeichnen
        draw_title_menu_line(0, "GAME START", (ctx->cursor == 0));
        draw_title_menu_line(1, "OPTIONS",    (ctx->cursor == 1));
        draw_title_menu_line(2, "SOUND TEST", (ctx->cursor == 2));
        ctx->needsRedraw = false;
    }
}

void title_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
}