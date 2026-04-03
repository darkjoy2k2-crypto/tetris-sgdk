#include "menu_bg.h"
#include "bg.h"
#include "states/game/game_core.h"
#include "menubg/none_mode.h"
#include "menubg/space_mode.h"
#include "menubg/riistar_mode.h"
#include "menubg/club_mode.h"
#include <string.h>
#include "states/states.h"

static const u16 target_palette[8] = {
    MENU_BG_COLOR,
    (((7) << 9) | ((6) << 5) | ((2) << 1)),
    (((2) << 9) | ((7) << 5) | ((7) << 1)),
    (((7) << 9) | ((3) << 5) | ((7) << 1)),
    (((3) << 9) | ((7) << 5) | ((3) << 1)),
    (((2) << 9) | ((3) << 5) | ((7) << 1)),
    (((7) << 9) | ((4) << 5) | ((3) << 1)),
    (((2) << 9) | ((2) << 5) | ((7) << 1))
};

static MenuBgState menuBg = {
    .scroll_x = F16_0,
    .sub_y = F16_0,
    .curr_dx = F16_0,
    .curr_dy = F16_0,
    .is_active = FALSE,
    .bg_mode = BG_MODE_NONE,
    .pending_bg_mode = BG_MODE_NONE,
    .fade_level = F16_0,
    .is_fading = FALSE,
    .has_pending_mode_switch = FALSE,
    .palette_frozen = FALSE,
    .target_dx = F16_0,
    .target_dy = F16_0,
    .bg_has_line_parallax = TRUE,
    .bg_dynamic_palette = &riistar_bg_palette,
    .y_offset = F16_0,
    .y_velocity = F16_0,
};

#define scroll_x                 (menuBg.scroll_x)
#define sub_y                    (menuBg.sub_y)
#define curr_dx                  (menuBg.curr_dx)
#define curr_dy                  (menuBg.curr_dy)
#define target_dx                (menuBg.target_dx)
#define target_dy                (menuBg.target_dy)
#define change_timer             (menuBg.mode.menu.change_timer)
#define is_active                (menuBg.is_active)
#define bg_mode                  (menuBg.bg_mode)
#define pending_bg_mode          (menuBg.pending_bg_mode)
#define fade_level               (menuBg.fade_level)
#define is_fading                (menuBg.is_fading)
#define is_flashing              (menuBg.mode.space.is_flashing)
#define has_pending_mode_switch  (menuBg.has_pending_mode_switch)
#define palette_frozen           (menuBg.palette_frozen)
#define current_intensity        (menuBg.mode.space.current_intensity)
#define flash_level              (menuBg.mode.space.flash_level)
#define bg_has_line_parallax     (menuBg.bg_has_line_parallax)
#define bg_dynamic_palette       (menuBg.bg_dynamic_palette)
#define riistar_y_offset         (menuBg.y_offset)
#define riistar_y_velocity       (menuBg.y_velocity)

#define club_map                 (menuBg.mode.club.map)
static void update_palette_fade(void);
static void draw_menu_shapes(void);

u16 menu_bg_get_base_color(void) {
    return MENU_BG_COLOR;
}

void menu_bg_set_palette_frozen(bool frozen) {
    palette_frozen = frozen;
}

static void apply_current_mode_state(void) {
    Map* oldClubMap = menuBg.mode.club.map;

    if ((bg_mode != BG_MODE_CLUB) && (oldClubMap != NULL)) {
        MAP_release(oldClubMap);
    }

    // Union immer vor Mode-Init leeren — verhindert Garbage aus vorherigem Mode
    memset(&menuBg.mode, 0, sizeof(menuBg.mode));
    
    scroll_x = F16_0;
    sub_y = F16_0;
    curr_dx = F16_0;
    curr_dy = F16_0;

    if (bg_mode == BG_MODE_RIISTAR || bg_mode == BG_MODE_CLUB) {
        if (bg_mode == BG_MODE_RIISTAR) {
            bg_has_line_parallax = TRUE;
            bg_dynamic_palette = &riistar_bg_palette;
            menubg_riistar_apply(&menuBg);
        } else {
            bg_has_line_parallax = FALSE;
            bg_dynamic_palette = &club_bg_palette;
            menubg_club_apply(&menuBg);
        }
    } else {
        VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
        VDP_setHorizontalScroll(BG_B, 0);
        VDP_setVerticalScroll(BG_B, 0);

        if (bg_mode == BG_MODE_MENU) {
            target_dx = FIX16(0.2);
            target_dy = FIX16(0.1);
            change_timer = 0;
            draw_menu_shapes();
        } else {
            menubg_space_apply(&menuBg);
        }
    }

    update_palette_fade();
}

void menu_bg_riistar_set_stack_top(s16 topRow)
{
    menubg_riistar_set_stack_top(&menuBg, topRow);
}

void menu_bg_riistar_pulse(u8 pulseType)
{
    menubg_riistar_pulse(&menuBg, pulseType, bg_mode, is_active);
}

// --- HILFSFUNKTIONEN ---

static void update_palette_fade() {
    if (palette_frozen) return;

    if (bg_mode == BG_MODE_CLUB) {
        return;
    }

    if (bg_mode == BG_MODE_RIISTAR) {
        const u16* pal = bg_dynamic_palette->data;

        for (u16 i = 0; i < 16; i++) {
            u16 target = pal[i];
            u16 r = (target >> 1) & 0x7;
            u16 g = (target >> 5) & 0x7;
            u16 b = (target >> 9) & 0x7;
            u16 fr = F16_toInt(F16_mul(FIX16(r), fade_level));
            u16 fg = F16_toInt(F16_mul(FIX16(g), fade_level));
            u16 fb = F16_toInt(F16_mul(FIX16(b), fade_level));

            if (fr > 7) fr = 7;
            if (fg > 7) fg = 7;
            if (fb > 7) fb = 7;

            PAL_setColor(i, (fb << 9) | (fg << 5) | (fr << 1));
        }
        return;
    }

    for (u16 i = 0; i < 7; i++) {
        u16 target = COL_GRAY;
        
        if (bg_mode == BG_MODE_SPACE) {
            // Kanal-Logik nach Vorgabe
            if (i == 0) { // Kanal 1
                if (current_intensity >= 8)      target = COL_RED;
                else                             target = COL_GRAY;
            } 
            else if (i == 1) { // Kanal 2
                if (current_intensity >= 9)      target = COL_RED;
                else if (current_intensity >= 5) target = COL_YELLOW;
                else                             target = COL_GRAY;
            } 
            else if (i == 2) { // Kanal 3
                if (current_intensity >= 10)     target = COL_RED;
                else if (current_intensity >= 7) target = COL_RED;
                else if (current_intensity >= 6) target = COL_YELLOW;
                else                             target = COL_GRAY;
            }
        } else {
            target = target_palette[i + 1];
        }
        
        if (bg_mode == BG_MODE_SPACE && is_flashing) {
            // Weiß-Flash Logik: Skaliert von Grau zu Weiß (7,7,7)
            u16 v = F16_toInt(F16_mul(FIX16(7), flash_level)); 
            if (v > 7) v = 7;
            PAL_setColor(1 + i, (v << 9) | (v << 5) | (v << 1));
            PAL_setColor(0, (v << 9) | (v << 5) | (v << 1));
        } else {
            u16 r = (target >> 1) & 0x7, g = (target >> 5) & 0x7, b = (target >> 9) & 0x7;
            u16 fr = F16_toInt(F16_mul(FIX16(r), fade_level));
            u16 fg = F16_toInt(F16_mul(FIX16(g), fade_level));
            u16 fb = F16_toInt(F16_mul(FIX16(b), fade_level));
            if (fr > 7) fr = 7; 
            if (fg > 7) fg = 7; 
            if (fb > 7) fb = 7;
            PAL_setColor(1 + i, (fb << 9) | (fg << 5) | (fr << 1));
            if (i == 0) {
                if (bg_mode == BG_MODE_MENU) PAL_setColor(0, MENU_BG_COLOR);
                else PAL_setColor(0, 0x0000);
            }
        }
    }
}

static void draw_menu_shapes(void) {
    VDP_clearPlane(BG_B, TRUE);
    for (u16 zy = 0; zy < 5; zy++) {
        for (u16 zx = 0; zx < 10; zx++) {
            if ((random() % 10) < 7) {
                u16 type = random() % 7, rot = random() % 4;
                u16 rx = (zx * 6) + (random() % 3), ry = (zy * 6) + (random() % 3);
                for (u16 i = 0; i < 4; i++) {
                    s16 px = (rx + PIECES[type][rot][i][0]) % 64;
                    s16 py = (ry + PIECES[type][rot][i][1]) % 32;
                    VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_MENU_BLOCK_BASE + type), px, py);
                }
            }
        }
    }
}

void menu_bg_init() {
    PAL_setColor(0, MENU_BG_COLOR); 
    for (u16 i = 1; i <= 7; i++) {
        u32 tile[8]; u32 c = i & 0xF;
        tile[0] = 0x00000000;
        u32 row = (c<<28)|(c<<24)|(c<<20)|(c<<16)|(c<<12)|(c<<8)|0x00;
        for(u16 j = 1; j < 8; j++) tile[j] = row;
        VDP_loadTileData(tile, TILE_MENU_BLOCK_BASE + (i - 1), 1, CPU);
    }
    menubg_space_init_tiles(1);
}

void menu_bg_set_mode(u8 mode) {
    if (!has_pending_mode_switch && (bg_mode == mode)) return;

    if (has_pending_mode_switch) {
        // Laufende Transition: nur Ziel aktualisieren oder abbrechen.
        if (mode == bg_mode) {
            has_pending_mode_switch = false;
            pending_bg_mode = BG_MODE_NONE;
            is_active = (bg_mode != BG_MODE_NONE);
        } else {
            pending_bg_mode = mode;
        }
        return;
    }

    // Start aus OFF/NONE in aktiven Modus sofort sichtbar machen.
    // Der uebergeordnete Fullscreen-Fade in main.c uebernimmt die Transition.
    if (bg_mode == BG_MODE_NONE) {
        bg_mode = mode;
        if (bg_mode == BG_MODE_NONE) return;
        is_active = true;
        fade_level = FIX16(1.0);
        is_fading = false;
        apply_current_mode_state();
        return;
    }

    // Wechsel aus aktivem Modus: immer ueber Fade-Out, danach ggf. Fade-In.
    pending_bg_mode = mode;
    has_pending_mode_switch = true;
    is_active = false;
    is_fading = true;
}

void menu_bg_set_mode_instant(u8 mode) {
    has_pending_mode_switch = false;
    pending_bg_mode = BG_MODE_NONE;

    if (bg_mode == BG_MODE_CLUB) {
        menubg_club_cleanup(&menuBg);
    }

    bg_mode = mode;

    if (bg_mode == BG_MODE_NONE) {
        is_active = false;
        is_fading = false;
        fade_level = F16_0;
        menubg_none_apply();
        return;
    }

    is_active = true;
    is_fading = false;
    fade_level = FIX16(1.0);
    apply_current_mode_state();
}

void menu_bg_set_active(bool active) {
    // Legacy wrapper: avoid using this in states.
    menu_bg_set_mode(active ? BG_MODE_MENU : BG_MODE_NONE);
}

void menu_bg_set_intensity(u8 level) {
    if (bg_mode != BG_MODE_SPACE) return;
    menubg_space_set_intensity(&menuBg, level, is_active);
    update_palette_fade();
}

void menu_bg_update() {
    bool flashing_active = (bg_mode == BG_MODE_SPACE) && is_flashing;

    if (is_active || fade_level > F16_0 || flashing_active) {
        if (bg_mode == BG_MODE_MENU) {
            if (change_timer > 0) change_timer--;
            if (change_timer == 0 && is_active) {
                u16 angle = random() % 1024;
                target_dx = F16_mul(cosFix16(angle), BG_SPEED_BASE);
                target_dy = F16_mul(sinFix16(angle), BG_SPEED_BASE);
                change_timer = 240 + (random() % 300);
            }
            if (curr_dx < target_dx) curr_dx += BG_ACCEL; else if (curr_dx > target_dx) curr_dx -= BG_ACCEL;
            if (curr_dy < target_dy) curr_dy += BG_ACCEL; else if (curr_dy > target_dy) curr_dy -= BG_ACCEL;
        } else if (bg_mode == BG_MODE_SPACE) {
            menubg_space_update(&menuBg);
        } else if (bg_mode == BG_MODE_CLUB) {
            menubg_club_update(&menuBg);
        } else {
            menubg_riistar_update(&menuBg);
        }
        scroll_x += curr_dx; sub_y += curr_dy;
        if (bg_mode != BG_MODE_RIISTAR && bg_mode != BG_MODE_CLUB) {
            VDP_setHorizontalScroll(BG_B, F16_toInt(scroll_x));
        }
        if (bg_mode != BG_MODE_CLUB) VDP_setVerticalScroll(BG_B, F16_toInt(sub_y));

        if (bg_mode == BG_MODE_SPACE && is_flashing) {
            flash_level -= FIX16(0.04); 
            if (flash_level <= F16_0) { 
                is_flashing = false; 
                flash_level = F16_0; 
                fade_level = FIX16(1.0);
            }
            update_palette_fade();
        } else if (is_fading) {
            if (is_active) {
                if (fade_level < FIX16(1.0)) fade_level += FIX16(0.05); else fade_level = FIX16(1.0);
            } else {
                fade_level -= FIX16(0.05);
                if (fade_level <= F16_0) {
                    fade_level = F16_0;
                    if (has_pending_mode_switch) {
                        has_pending_mode_switch = false;
                        bg_mode = pending_bg_mode;
                        pending_bg_mode = BG_MODE_NONE;

                        if (bg_mode == BG_MODE_NONE) {
                            is_active = false;
                            is_fading = false;
                            VDP_clearPlane(BG_B, TRUE);
                        } else {
                            is_active = true;
                            is_fading = true;
                            fade_level = F16_0;
                            apply_current_mode_state();
                        }
                    } else {
                        is_fading = false;
                        VDP_clearPlane(BG_B, TRUE);
                    }
                }
            }
            update_palette_fade();
        }
    }
}