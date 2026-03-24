#pragma once

#include <genesis.h>

#define BG_MODE_MENU  0
#define BG_MODE_SPACE 1
#define BG_MODE_RIISTAR 2

void menu_bg_init();
void menu_bg_set_mode(u8 mode);
void menu_bg_set_active(bool active);
void menu_bg_update();
// Geändert auf u8, um mit der .c Datei übereinzustimmen
void menu_bg_set_intensity(u8 intensity);

// Riistar-Drive: topRow = -1 wenn Board leer, sonst 0..19 (0 = oberste Reihe).
void menu_bg_riistar_set_stack_top(s16 topRow);
// pulseType: 1 = soft landing, 2 = soft drop landing, 3 = hard drop landing.
void menu_bg_riistar_pulse(u8 pulseType);