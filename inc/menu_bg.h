#pragma once

#include <genesis.h>

#define BG_MODE_MENU  0
#define BG_MODE_SPACE 1

void menu_bg_init();
void menu_bg_set_mode(u8 mode);
void menu_bg_set_active(bool active);
void menu_bg_update();
// Geändert auf u8, um mit der .c Datei übereinzustimmen
void menu_bg_set_intensity(u8 intensity);