#include <genesis.h>
#include "states/game_select.h"
#include "states/states.h"
#include "sound_manager.h"
#include <string.h>
#include "menu_bg.h"
#include "fonts.h"
#include "gfx.h"

// Der lokale Kontext für diesen State
typedef struct SelectContext {
    u16 cursor;
    char name[4];
    u16 nameCharIdx;
    u16 randMode;
    u16 speedLevel;
    u16 garbageFreq;
    u16 itemMode;
    u16 flags;          // Temporäre Flags für das Menü
    bool needsRedraw;
} SelectContext;

static SelectContext* ctx = NULL;

// --- Private Zeichen-Helfer ---

static void draw_menu_line(u16 row, char* label, u16 currentVal, char* options[], u16 numOptions, bool isSelected) {
    u16 y = 8 + (row * 2);
    u16 x = 4;

    // Standard-Palette für das Label (Gelb/Gold bei Selektion)
    VDP_setTextPalette(isSelected ? PAL1 : PAL3);
    VDP_drawText(isSelected ? ">" : " ", x, y);
    x += 2;

    VDP_drawText(label, x, y);
    x += 12;

    // Optionen zeichnen
    for (u16 i = 0; i < numOptions; i++) {
        // Selektierte Option leuchtet (PAL2 - Rot oder Highlight)
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
        // Aktueller Buchstabe in der Bearbeitung wird hervorgehoben
        if (isSelected && ctx->nameCharIdx == i) VDP_setTextPalette(PAL2);
        else VDP_setTextPalette(PAL3);

        char letter[2] = { ctx->name[i], '\0' };
        VDP_drawText(letter, x, y);
        x += 2;
    }
}

// --- State System Funktionen ---

void select_init() {
    // Speicher reservieren und nullen
    ctx = MEM_alloc(sizeof(SelectContext));
    memset(ctx, 0, sizeof(SelectContext));

    // Aktuelle Konfiguration in den lokalen Kontext spiegeln
    ctx->cursor = 0;
    strncpy(ctx->name, config.playerName, 3);
    ctx->name[3] = '\0';
    ctx->nameCharIdx = 0;
    
    ctx->randMode = config.randMode;
    ctx->speedLevel = config.speedLevel;
    ctx->garbageFreq = config.garbageFreq;
    ctx->itemMode = config.itemMode;
    
    // Kopie der Flags für die Bearbeitung im Menü
    ctx->flags = config.flags;

    ctx->needsRedraw = true;
    menu_bg_set_active(true);
}

void select_init_draw() {
    if (ctx == NULL) return;

    // Zentrale Farb- und Font-Initialisierung aufrufen
    UI_init_fonts_and_palettes();   

    // Statische UI-Elemente
    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL1);
    VDP_drawText("--- GAME SETTINGS ---", 10, 4);
    VDP_setTextPalette(PAL3);
    VDP_drawText("START TO BEGIN", 13, 27);
}

void select_update() {
    if (ctx == NULL) return;

    // 1. Vertikale Navigation
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

    // 2. Werte-Manipulation (Links/Rechts oder A)
    bool goRight  = (joyState & BUTTON_RIGHT) && !(lastJoyState & BUTTON_RIGHT);
    bool goLeft   = (joyState & BUTTON_LEFT)  && !(lastJoyState & BUTTON_LEFT);
    bool pressedA = (joyState & BUTTON_A)     && !(lastJoyState & BUTTON_A);

    if (goRight || goLeft || pressedA) {
        ctx->needsRedraw = true;
        
        if (ctx->cursor == 0) {
            // --- Namenseingabe ---
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
            // --- Menü-Optionen ---
            SOUND_play(SND_ROTATE);
            s16 dir = goLeft ? -1 : 1;
            switch(ctx->cursor) {
                case 1: ctx->randMode    = (ctx->randMode + dir + 2) % 2; break;
                case 2: ctx->speedLevel  = (ctx->speedLevel + dir + 4) % 4; break;
                case 3: ctx->garbageFreq = (ctx->garbageFreq + dir + 4) % 4; break;
                
                // Bit-Flags umschalten (Nutzt die neuen flexiblen Makros)
                case 4: TOGGLE_FLAG(ctx->flags, FLAG_SHADOW); break; 
                case 5: TOGGLE_FLAG(ctx->flags, FLAG_HOLD);   break;
                case 6: TOGGLE_FLAG(ctx->flags, FLAG_NEXT);   break;
                
                case 7: ctx->itemMode    = (ctx->itemMode + dir + 4) % 4; break;
            }
        }
    }

    // 3. Übernahme in die globale Config und Start des Spiels
    if ((joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
        strncpy(config.playerName, ctx->name, 3);
        config.playerName[3] = '\0';
        
        config.randMode    = ctx->randMode;
        config.speedLevel  = ctx->speedLevel;
        config.garbageFreq = ctx->garbageFreq;
        config.itemMode    = ctx->itemMode;
        
        // Flags übertragen: Alle Gameplay-Flags vom Kontext, 
        // aber das FLAG_IS_PAL bleibt fest im globalen config.flags
        config.flags = (ctx->flags & ~FLAG_IS_PAL) | (config.flags & FLAG_IS_PAL);

        currentState = STATE_GAME;
    }
}

void select_draw() {
    if (ctx == NULL || !ctx->needsRedraw) return;

    // UI zeichnen
    draw_name_entry(ctx->cursor == 0);

    char* optsRand[]   = {"Fair", "Chaos"};
    char* optsLevels[] = {"None", "Slow", "Med", "Fast"};
    char* optsOnOff[]  = {"Off", "On"};
    char* optsItems[]  = {"None", "All", "Good", "Bad"};

    draw_menu_line(1, "Random:",   ctx->randMode,   optsRand,   2, (ctx->cursor == 1));
    draw_menu_line(2, "Speed:",    ctx->speedLevel,  optsLevels, 4, (ctx->cursor == 2));
    draw_menu_line(3, "Garbage:",  ctx->garbageFreq, optsLevels, 4, (ctx->cursor == 3));
    
    // Abfrage der Flags für die Anzeige (GET_FLAG Makro)
    draw_menu_line(4, "Shadow:",   GET_FLAG(ctx->flags, FLAG_SHADOW) ? 1 : 0, optsOnOff, 2, (ctx->cursor == 4));
    draw_menu_line(5, "Hold:",     GET_FLAG(ctx->flags, FLAG_HOLD)   ? 1 : 0, optsOnOff, 2, (ctx->cursor == 5));
    draw_menu_line(6, "Next:",     GET_FLAG(ctx->flags, FLAG_NEXT)   ? 1 : 0, optsOnOff, 2, (ctx->cursor == 6));
    
    draw_menu_line(7, "Items:",    ctx->itemMode,    optsItems,  4, (ctx->cursor == 7));
    
    ctx->needsRedraw = false;
}

void select_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearTextArea(0, 0, 40, 28);
}