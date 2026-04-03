#pragma once

#include <genesis.h>

#define BG_MODE_NONE    0
#define BG_MODE_MENU    1
#define BG_MODE_SPACE   2
#define BG_MODE_RIISTAR 3
#define BG_MODE_CLUB    4

/* --- Interne Tile-Indizes ------------------------------------------------ */
#define TILE_MENU_BLOCK_BASE       510
#define TILE_STAR_BASE             520
/** SPR_init() reserviert 420 Tiles direkt vor dem Font-Bereich. */
#define SPRITE_VRAM_RESERVE_TILES  420

/* --- Scroll-Parameter ---------------------------------------------------- */
#define BG_SPEED_BASE     FIX16(0.3)
#define BG_ACCEL          FIX16(0.02)
#define F16_0             FIX16(0)

/* --- Riistar-Konstanten --------------------------------------------------- */
#define RIISTAR_SPRING_K       FIX16(0.10)
#define RIISTAR_DAMPING        FIX16(0.88)
#define RIISTAR_PULSE_SOFT     FIX16(0.35)
#define RIISTAR_PULSE_SOFTDROP FIX16(1.20)
#define RIISTAR_PULSE_HARDDROP FIX16(2.20)

/* --- Club-Konstanten ----------------------------------------------------- */
#define CLUB_SCROLL_STEP     FIX16(0.5)
#define CLUB_SCROLL_MAX_X    1536
#define CLUB_SCROLL_Y_OFFSET (8)

/* --- Farb-Konstanten ------------------------------------------------------ */
#define COL_GRAY        0x0444
#define COL_YELLOW      0x0440
#define COL_RED         0x0004
#define STARFIELD_BASE_SP    FIX16(-1.2)
#define MENU_BG_COLOR        (((3) << 9) | ((2) << 5) | ((2) << 1))

/* --- Interne Zustandstypen ----------------------------------------------- */

typedef struct MenuBgMenuState {
    fix16 target_dx;
    fix16 target_dy;
    u16 change_timer;
} MenuBgMenuState;

typedef struct MenuBgSpaceState {
    u8 current_intensity;
    bool is_flashing;
    u8 _pad;
    fix16 flash_level;
} MenuBgSpaceState;

typedef struct MenuBgRiistarState {
    fix16 group_scroll[5];
    s16 hscroll_lines[240];
    s16 map_row_scroll[32];
    fix16 vertical_center;
    fix16 vertical_amplitude;
    fix16 vertical_max;
    fix16 speed_scale;
    s16 stack_top_row;
} MenuBgRiistarState;

typedef struct MenuBgClubState {
    Map* map;
    fix32 pos_x;
    fix32 scroll_diff;
    fix16 vertical_center;
    fix16 vertical_max;
    fix16 vertical_offset;
    s16 debug_scroll_px;
} MenuBgClubState;

typedef union MenuBgModeState {
    MenuBgMenuState menu;
    MenuBgSpaceState space;
    MenuBgRiistarState riistar;
    MenuBgClubState club;
} MenuBgModeState;

typedef struct MenuBgState {
    const Palette* bg_dynamic_palette;  /* u32 — muss 4-Byte-Grenze halten            */
    MenuBgModeState mode;               /* union: intern fix32/ptr → 4-Byte-Alignment  */
    fix16 scroll_x;
    fix16 sub_y;
    fix16 curr_dx;
    fix16 curr_dy;
    fix16 target_dx;                    /* shared über MENU/SPACE (Drift-Ziel)         */
    fix16 target_dy;
    fix16 fade_level;
    fix16 y_offset;
    fix16 y_velocity;
    u8 bg_mode;
    u8 pending_bg_mode;
    bool is_active;
    bool is_fading;
    bool has_pending_mode_switch;
    bool palette_frozen;
    bool bg_has_line_parallax;
    u8 _pad;
} MenuBgState;

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