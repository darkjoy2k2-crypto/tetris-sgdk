#pragma once

#include <genesis.h>

#define BG_MODE_NONE    0
#define BG_MODE_MENU    1
#define BG_MODE_SPACE   2
#define BG_MODE_RIISTAR 3
#define BG_MODE_CLUB    4

void menu_bg_init();
void menu_bg_set_mode(u8 mode);
void menu_bg_set_mode_instant(u8 mode);
// Legacy wrapper, avoid in new code (use menu_bg_set_mode with BG_MODE_* instead).
void menu_bg_set_active(bool active);
void menu_bg_update();
u16 menu_bg_get_base_color(void);
void menu_bg_set_palette_frozen(bool frozen);
// Geändert auf u8, um mit der .c Datei übereinzustimmen
void menu_bg_set_intensity(u8 intensity);

// Riistar-Drive: topRow = -1 wenn Board leer, sonst 0..19 (0 = oberste Reihe).
void menu_bg_riistar_set_stack_top(s16 topRow);
// pulseType: 1 = soft landing, 2 = soft drop landing, 3 = hard drop landing.
void menu_bg_riistar_pulse(u8 pulseType);