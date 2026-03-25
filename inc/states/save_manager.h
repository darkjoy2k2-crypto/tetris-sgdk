#ifndef _SAVE_MANAGER_H_
#define _SAVE_MANAGER_H_

#include <genesis.h>

#define SAVE_MAGIC    0x54455452  // "TETR"
#define SAVE_VERSION  0x00000123

#define ADDR_MAGIC       0x00
#define ADDR_VERSION     0x04
#define ADDR_OPTIONS     0x10
#define ADDR_HIGHSCORES  0x80

// Standard Save-Funktionen
void save_init();
void save_load();
void save_options();
void save_highscores();
void save_execute();
void save_clear();

// State-Funktionen für STATE_SAVING
void saving_init();
void saving_update();
void saving_init_draw();
void saving_draw();
void saving_cleanup();

#endif