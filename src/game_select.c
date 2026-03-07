#include <genesis.h>
#include "game_select.h"
#include "states.h"
#include "sound_manager.h"
#include <string.h>

typedef struct SelectContext{
    u16 cursor;         // 0: Name, 1: Random, 2: Speed, 3: Garbage, 4: Shadow, 5: Hold, 6: Next
    char name[4];       
    u16 nameCharIdx;    
    u16 randMode;
    u16 speedLevel;
    u16 garbageFreq;
    bool showShadow;
    bool allowHold;
    bool showNext;
} SelectContext;

static SelectContext* ctx = NULL;

// Hilfsfunktion: Zeichnet eine Standard-Menüzeile
static void draw_menu_line(u16 row, char* label, u16 currentVal, char* options[], u16 numOptions, bool isSelected) {
    u16 y = 8 + (row * 2);
    u16 x = 4;

    // Cursor (Gelb)
    VDP_setTextPalette(isSelected ? PAL1 : PAL0);
    VDP_drawText(isSelected ? ">" : " ", x, y);
    x += 2;

    // Label (Immer Gelb)
    VDP_setTextPalette(PAL1); 
    VDP_drawText(label, x, y);
    x += 12; 

    // Optionen
    for (u16 i = 0; i < numOptions; i++) {
        // Gewählte Option in ROT (PAL2), andere in WEISS (PAL0)
        if (i == currentVal) VDP_setTextPalette(PAL2);
        else VDP_setTextPalette(PAL0);

        VDP_drawText(options[i], x, y);
        x += strlen(options[i]) + 1; 
    }
}

// Hilfsfunktion: Zeichnet die Namenseingabe
static void draw_name_entry(bool isSelected) {
    u16 y = 8;
    u16 x = 4;

    VDP_setTextPalette(isSelected ? PAL1 : PAL0);
    VDP_drawText(isSelected ? ">" : " ", x, y);
    x += 2;

    VDP_setTextPalette(PAL1);
    VDP_drawText("Player:", x, y);
    x += 12;

    for (u16 i = 0; i < 3; i++) {
        // Aktueller Buchstabe in ROT
        if (isSelected && ctx->nameCharIdx == i) VDP_setTextPalette(PAL2);
        else VDP_setTextPalette(PAL0);

        char letter[2] = { ctx->name[i], '\0' };
        VDP_drawText(letter, x, y);
        x += 2;
    }
}

void select_init() {
    ctx = MEM_alloc(sizeof(SelectContext));
    ctx->cursor = 0;
    strncpy(ctx->name, "JOE", 3);
    ctx->name[3] = '\0';
    ctx->nameCharIdx = 0;
    
    ctx->randMode = 0;
    ctx->speedLevel = 2;
    ctx->garbageFreq = 1;
    ctx->showShadow = true;
    ctx->allowHold = true;
    ctx->showNext = true;

    // Farben setzen (SGDK 2.x)
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF)); // PAL0: Weiß
    PAL_setColor(31, RGB24_TO_VDPCOLOR(0xFFFF00)); // PAL1: Gelb
    PAL_setColor(47, RGB24_TO_VDPCOLOR(0xFF0000)); // PAL2: Rot

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL1);
    VDP_drawText("--- GAME SETTINGS ---", 10, 4);
    
    VDP_setTextPalette(PAL0);
    VDP_drawText("START TO BEGIN", 13, 25);
}

void select_update() {
    if (ctx == NULL) return;

    // Navigation
    if ((joyState & BUTTON_DOWN) && !(lastJoyState & BUTTON_DOWN)) {
        ctx->cursor = (ctx->cursor + 1) % 7;
        SOUND_play(SND_MOVE);
    }
    if ((joyState & BUTTON_UP) && !(lastJoyState & BUTTON_UP)) {
        ctx->cursor = (ctx->cursor == 0) ? 6 : ctx->cursor - 1;
        SOUND_play(SND_MOVE);
    }

    bool goRight = (joyState & BUTTON_RIGHT) && !(lastJoyState & BUTTON_RIGHT);
    bool goLeft = (joyState & BUTTON_LEFT) && !(lastJoyState & BUTTON_LEFT);
    bool pressedA = (joyState & BUTTON_A) && !(lastJoyState & BUTTON_A);

    if (ctx->cursor == 0) {
        // Logik für Namenseingabe
        if (goRight || goLeft) {
            char c = ctx->name[ctx->nameCharIdx];
            if (goRight) c = (c == 'Z') ? 'A' : c + 1;
            else c = (c == 'A') ? 'Z' : c - 1;
            ctx->name[ctx->nameCharIdx] = c;
            SOUND_play(SND_ROTATE);
        }
        if (pressedA) {
            ctx->nameCharIdx = (ctx->nameCharIdx + 1) % 3;
            SOUND_play(SND_MOVE);
        }
    } else {
        // Logik für restliche Optionen
        if (goRight || goLeft || pressedA) {
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

    // Zeichnen
    draw_name_entry(ctx->cursor == 0);

    char* optsRand[] = {"Fair", "Chaos"};
    char* optsLevels[] = {"None", "Slow", "Med", "Fast"};
    char* optsOnOff[] = {"Off", "On"};

    draw_menu_line(1, "Random:",   ctx->randMode,   optsRand,   2, (ctx->cursor == 1));
    draw_menu_line(2, "Speed:",    ctx->speedLevel, optsLevels, 4, (ctx->cursor == 2));
    draw_menu_line(3, "Garbage:",  ctx->garbageFreq,optsLevels, 4, (ctx->cursor == 3));
    draw_menu_line(4, "Shadow:",   ctx->showShadow, optsOnOff,  2, (ctx->cursor == 4));
    draw_menu_line(5, "Hold:",     ctx->allowHold,  optsOnOff,  2, (ctx->cursor == 5));
    draw_menu_line(6, "Next:",     ctx->showNext,   optsOnOff,  2, (ctx->cursor == 6));

    // Start-Knopf: Speichern und Los!
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
    VDP_setTextPalette(PAL0);
    VDP_clearTextArea(0, 0, 40, 28);
}