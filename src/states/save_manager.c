#include "states/save_manager.h"
#include "states/states.h"
#include "menu_bg.h"
#include <string.h>
#include "bg.h"

/* 0x00200000..0x002001FF => 512 bytes total SRAM window in ROM header. */
typedef char serializable_fits_sram_window[
    (ADDR_OPTIONS + sizeof(Serializable) <= 0x200) ? 1 : -1
];

// --- TEIL 1: SRAM HARDWARE-ZUGRIFF ---

void save_execute() {
    KLog("SRAM_EXECUTE: Start...");
    KLog_U2("RAM_CHECK: LRinit=", config.thresholdLRInitial, " LRepeat=", config.thresholdLRRepeat);
    KLog_U1("RAM_CHECK: SD=", config.thresholdSD);
    KLog_U1("RAM_CHECK: flags=", config.flags);

    SRAM_enable();
    
    // Header schreiben
    SRAM_writeLong(ADDR_MAGIC, SAVE_MAGIC);
    SRAM_writeLong(ADDR_VERSION, SAVE_VERSION);
    
    // Pointer auf den gepackten Block
    u8 *ptr = (u8*)&config.serializable;
    u16 size = sizeof(Serializable);
    
    KLog_U1("SRAM_HW: Writing serializable block, size: ", size);
    
    // Da wir 'packed' nutzen, können wir die Struktur Byte für Byte spiegeln
    for (u16 i = 0; i < size; i++) {
        SRAM_writeByte(ADDR_OPTIONS + i, ptr[i]);
    }
    
    SRAM_disable();
    KLog("SRAM_HW: Disabled. SRAM_EXECUTE: Finished.");
}

bool save_verify() {
    KLog("SRAM_VERIFY: Starting Integrity Check...");
    SRAM_enable();
    
    u32 magic = SRAM_readLong(ADDR_MAGIC);
    u32 version = SRAM_readLong(ADDR_VERSION);
    
    if (magic != SAVE_MAGIC || version != SAVE_VERSION) {
        KLog_U1("SRAM_VERIFY: Header Mismatch! Magic found: ", magic);
        SRAM_disable();
        return FALSE;
    }

    u8 *ptr = (u8*)&config.serializable;
    u16 size = sizeof(Serializable);
    
    for (u16 i = 0; i < size; i++) {
        u8 sramByte = SRAM_readByte(ADDR_OPTIONS + i);
        if (ptr[i] != sramByte) {
            KLog_U1("SRAM_VERIFY: Difference at index ", i);
            SRAM_disable();
            return FALSE;
        }
    }
    
    SRAM_disable();
    KLog("SRAM_VERIFY: Success. SRAM matches RAM.");
    return TRUE;
}

void save_load() {
    KLog("SRAM_LOAD: Start reading...");
    SRAM_enable();
    
    u8 *ptr = (u8*)&config.serializable;
    u16 size = sizeof(Serializable);
    
    for (u16 i = 0; i < size; i++) {
        ptr[i] = SRAM_readByte(ADDR_OPTIONS + i);
    }
    
    SRAM_disable();
    KLog("SRAM_LOAD: Finished.");
    KLog_U2("SRAM_LOAD_RESULT: LRinit=", config.thresholdLRInitial, " LRepeat=", config.thresholdLRRepeat);
    KLog_U1("SRAM_LOAD_RESULT: SD=", config.thresholdSD);
}

void save_clear() {
    KLog("SRAM_CLEAR: Initializing RAM with Defaults...");
    
    config.serializable.currentScore = 0;
    strncpy(config.serializable.playerName, "ABC", 3);
    config.serializable.playerName[3] = '\0';
    
    config.serializable.randMode = 0;
    config.serializable.speedLevel = 3;
    config.serializable.garbageFreq = 3;
    config.serializable.itemMode = 1;
    config.serializable.flags = FLAG_SHADOW | FLAG_HOLD | FLAG_NEXT | FLAG_MUSIC | FLAG_SOUND | FLAG_BG;
    
    // Werkseinstellungen Sensibilität
    config.serializable.thresholdLRInitial = 6;
    config.serializable.thresholdLRRepeat = 2;
    config.serializable.thresholdSD = 3;
    
    /* Initialize Challenge Mode (column 0 unlocked, all else locked) */
    memset(config.serializable.challenge_unlocked, 0, sizeof(config.serializable.challenge_unlocked));
    memset(config.serializable.challenge_cleared, 0, sizeof(config.serializable.challenge_cleared));

    char* names[] = {"PET", "SGK", "CPU", "VDP", "ACE", "SKY", "DAN", "EVA", "MAX", "JOE"};
    for(u16 i = 0; i < 10; i++) {
        strncpy(config.serializable.highscores[i].name, names[i], 3);
        config.serializable.highscores[i].name[3] = '\0';
        config.serializable.highscores[i].score = (u32)((10 - i) * 1000);
        config.serializable.highscores[i].isNew = 0;
    }
    
    save_execute();
}

void save_init() {
    KLog("SRAM_INIT: Check Hardware Header...");
    SRAM_enable();
    u32 magic = SRAM_readLong(ADDR_MAGIC);
    u32 version = SRAM_readLong(ADDR_VERSION);
    SRAM_disable();

    if (magic == SAVE_MAGIC && version == SAVE_VERSION) {
        save_load();
    } else {
        KLog("SRAM_INIT: Fresh SRAM detected. Formatting...");
        save_clear();
    }
}

// --- TEIL 2: STATE-LOGIK (STATE_SAVE) ---

void saving_init() {
    SaveContext *ctx = &sctx->save;
    ctx->timer = 0;
    ctx->textVisible = TRUE;
    ctx->errorOccurred = FALSE;

    VDP_clearTextArea(0, 0, 40, 28);
    // Sicherstellen, dass das Menü-System weiß, welcher Hintergrund aktiv ist
    menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));
    
    PAL_fadeInPalette(PAL1, game_bg.palette->data, 30, TRUE);
}

void saving_init_draw() {
    VDP_drawText("DO NOT TURN OFF CONSOLE!", 8, 12);
}

void saving_update() {
    SaveContext *ctx = &sctx->save;

    if (ctx->errorOccurred) {
        if (joyState & (BUTTON_A | BUTTON_B | BUTTON_START)) {
            config.sramop = SRAM_NONE;
            currentState = STATE_TITLE;
        }
        return;
    }

    ctx->timer++;

    // Verzögerung, damit der User den Text lesen kann
    if (ctx->timer == 60) {
        if (config.sramop == SRAM_SAVE) {
            save_execute();
            if (!save_verify()) {
                ctx->errorOccurred = TRUE;
                VDP_clearTextArea(0, 14, 40, 4);
                VDP_setTextPalette(PAL0);
                VDP_drawText("SAVE CORRUPTED!", 12, 14);
                VDP_drawText("PRESS BUTTON TO CONTINUE", 8, 16);
            }
        } 
        else if (config.sramop == SRAM_LOAD) {
            save_load();
        }
        else if (config.sramop == SRAM_INIT) {
            save_init();
        }
    }

    if (!ctx->errorOccurred) {
        if (ctx->timer == 90) {
            PAL_fadeOut(0, 63, 20, TRUE);
        }

        if (ctx->timer > 115) {
            config.sramop = SRAM_NONE;
            // Falls preferredState gesetzt wurde (z.B. zurück zu Options), dahin wechseln
            if (config.preferredState != STATE_NONE) {
                currentState = config.preferredState;
                config.preferredState = STATE_NONE;
            } else {
                currentState = STATE_TITLE;
            }
        }
    }
}

void saving_draw() {
    SaveContext *ctx = &sctx->save;
    if (ctx->errorOccurred) return;

    char* msg = "WORKING...";
    u16 x = 15;

    if (config.sramop == SRAM_INIT) { msg = "CHECKING DATA..."; x = 12; }
    else if (config.sramop == SRAM_LOAD) { msg = "LOADING DATA...";  x = 13; }
    else if (config.sramop == SRAM_SAVE) { msg = "SAVING DATA...";   x = 14; }

    if (ctx->timer % 20 == 0) {
        ctx->textVisible = !ctx->textVisible;
        if (ctx->textVisible) VDP_drawText(msg, x, 14);
        else VDP_clearText(x, 14, 16);
    }
}

void saving_cleanup() {
    VDP_clearTextArea(0, 0, 40, 28);
}