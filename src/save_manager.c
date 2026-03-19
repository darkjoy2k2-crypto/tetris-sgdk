#include "save_manager.h"
#include "states/states.h"
#include <string.h>

extern GlobalConfig config;

void save_options() {
    SRAM_enable(); 
    u8 *ptr = (u8*)&config;
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
    // Da wir hier interne Funktionen rufen, die selbst Enabled/Disabled nutzen,
    // ist das doppelte Schalten harmlos, sorgt aber für Kapselung.
    save_options();
    save_highscores();
    SRAM_disable();
}

void save_load() {
    SRAM_enable();
    u8 *ptr = (u8*)&config;
    u16 size_opt = (u16)((u32)&config.highscores - (u32)&config);
    for (u16 i = 0; i < size_opt; i++) {
        ptr[i] = SRAM_readByte(ADDR_OPTIONS + i);
    }
    u8 *h_ptr = (u8*)&config.highscores;
    u16 size_h = (u16)sizeof(config.highscores);
    for (u16 i = 0; i < size_h; i++) {
        h_ptr[i] = SRAM_readByte(ADDR_HIGHSCORES + i);
    }
    SRAM_disable();
}

void save_init() {
    // Nur kurz einschalten zum Prüfen
    SRAM_enable();
    u32 magic = SRAM_readLong(ADDR_MAGIC);
    u32 version = SRAM_readLong(ADDR_VERSION);
    SRAM_disable();

    if (magic == SAVE_MAGIC && version == SAVE_VERSION) {
        save_load();
    } else {
        // save_clear ruft save_execute auf, welches den SRAM wieder einschaltet
        save_clear();
    }
}

void save_clear() {
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