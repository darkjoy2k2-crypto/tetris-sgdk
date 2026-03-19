#include "states/save_manager.h"
#include "states/states.h"
#include "menu_bg.h"
#include <string.h>
#include "bg.h"


// --- TEIL 1: SRAM HARDWARE-ZUGRIFF ---

void save_options() {
    SRAM_enable(); 
    u8 *ptr = (u8*)&config;
    // Größe berechnen: Alles vor dem Highscore-Array
    u16 size = (u16)((u32)&config.highscores - (u32)&config);
    for (u16 i = 0; i < size; i++) {
        SRAM_writeByte(ADDR_OPTIONS + i, ptr[i]);
    }
    SRAM_disable();
}

void save_highscores() {
    SRAM_enable();
    u8 *ptr = (u8*)&config.highscores;
    u16 size = (u16)sizeof(config.highscores);
    for (u16 i = 0; i < size; i++) {
        SRAM_writeByte(ADDR_HIGHSCORES + i, ptr[i]);
    }
    SRAM_disable();
}

void save_execute() {
    SRAM_enable();
    SRAM_writeLong(ADDR_MAGIC, SAVE_MAGIC);
    SRAM_writeLong(ADDR_VERSION, SAVE_VERSION);
    SRAM_disable();

    // Teilfunktionen nutzen ihre eigenen Enable/Disable Zyklen
    save_options();
    save_highscores();
}

void save_load() {
    SRAM_enableRO();
    
    // 1. Optionen laden
    u8 *ptr = (u8*)&config;
    // WICHTIG: Die Größe wird nur bis zum Anfang der Highscores berechnet
    u16 size_opt = (u16)((u32)&config.highscores - (u32)&config);
    for (u16 i = 0; i < size_opt; i++) {
        ptr[i] = SRAM_readByte(ADDR_OPTIONS + i);
    }

    // 2. Highscores laden
    u8 *h_ptr = (u8*)&config.highscores;
    u16 size_h = (u16)sizeof(config.highscores);
    for (u16 i = 0; i < size_h; i++) {
        h_ptr[i] = SRAM_readByte(ADDR_HIGHSCORES + i);
    }
    
    SRAM_disable();

    // preferredState wird NICHT geladen und bleibt auf dem aktuellen RAM-Wert
    // Wir setzen ihn hier höchstens auf einen Sicherheits-Default, falls nötig:
    // config.preferredState = STATE_NONE; 
}

void save_init() {
    SRAM_enableRO();
    u32 magic = SRAM_readLong(ADDR_MAGIC);
    u32 version = SRAM_readLong(ADDR_VERSION);
    SRAM_disable();

    if (magic == SAVE_MAGIC && version == SAVE_VERSION) {
        save_load();
    } else {
        save_clear();
    }
}

void save_clear() {
    // RAM-Defaults setzen
    config.currentScore = 0;
    strncpy(config.playerName, "ABC", 4);
    config.randMode = 0;
    config.speedLevel = 1;
    config.garbageFreq = 0;
    config.itemMode = 1;
    config.flags = FLAG_SHADOW | FLAG_HOLD | FLAG_NEXT | FLAG_MUSIC | FLAG_SOUND | FLAG_BG;
    config.thresholdLR = 10;
    config.thresholdSD = 2;

    char* names[] = {"PET", "SGK", "CPU", "VDP", "ACE", "SKY", "DAN", "EVA", "MAX", "JOE"};
    for(u16 i = 0; i < 10; i++) {
        strncpy(config.highscores[i].name, names[i], 3);
        config.highscores[i].name[3] = '\0';
        config.highscores[i].score = (10 - i) * 1000;
        config.highscores[i].isNew = 0;
    }
    
    save_execute();
}

// --- TEIL 2: STATE-LOGIK (STATE_SAVE) ---

void saving_init() {
    SaveContext *ctx = &sctx->save;
    ctx->timer = 0;
    ctx->textVisible = TRUE;

    menu_bg_set_mode(BG_MODE_MENU);
    
    // Palette schwarz für Fade-In Start
    u16 target_pal[16];
    memcpy(target_pal, game_bg.palette->data, 16 * 2);
    
    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL1); // Gold/Gelb laut UI_init
    VDP_drawText("SAVING DATA...", 13, 12);
    VDP_drawText("DO NOT TURN OFF CONSOLE", 8, 14);

    // Fade In (1/2 Sekunde = 30 Frames)
    PAL_fadeInPalette(PAL2, game_bg.palette->data, GET_TICKS(30),true);
}

void saving_update() {
    SaveContext *ctx = &sctx->save;
    ctx->timer++;

    // Blinken-Phase (insgesamt 2 Sekunden, startet nach Fade-In)
    // Blinkt 4x (alle 15 Frames Toggle -> 30 Frames pro Zyklus)
        if (ctx->timer % 15 == 0) {
            ctx->textVisible = !ctx->textVisible;
            if (ctx->textVisible) {
                VDP_drawText("SAVING DATA...", 13, 12);
            } else {
                VDP_clearText(13, 12, 14);
            }
        }

    // In der 3. Sekunde (Frame 180) wird gespeichert
    if (ctx->timer == 60) {
        // Text sicherheitshalber wieder anzeigen
        save_execute();
    }

    // In der 4. Sekunde (Frame 240) Fade-Out (1/2 Sekunde)
    if (ctx->timer == 90) {
        PAL_fadeOut(0, 63, 30, TRUE);
    }

    // Nach Abschluss des Fades zum Ranking
    if (ctx->timer > 120) {
        if (config.preferredState == STATE_NONE){
        currentState = STATE_TITLE;
        }            
        else{
        currentState = config.preferredState;
           config.preferredState = STATE_NONE;
        }
        

    }
}

void saving_init_draw() {
    // Platzhalter für Handler-Kompatibilität
}

void saving_draw() {
    // Platzhalter für Handler-Kompatibilität
}

void saving_cleanup() {
    VDP_clearTextArea(0, 0, 40, 28);
}