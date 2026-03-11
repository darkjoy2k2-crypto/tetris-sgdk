#include "menu_bg.h"
#include "states/game/game_core.h"
#include <string.h>

#define TILE_MENU_BLOCK_BASE 510 
#define TILE_STAR_BASE       520

#define BG_SPEED_BASE     FIX16(0.3)
#define BG_ACCEL          FIX16(0.02)
#define F16_0             FIX16(0)

#define COL_GRAY   0x0444 
#define COL_YELLOW 0x0440 
#define COL_RED    0x0004 

#define STARFIELD_BASE_SP    FIX16(-1.2) 

static fix16 scroll_x = F16_0, sub_y = F16_0;
static fix16 curr_dx = F16_0, curr_dy = F16_0;
static fix16 target_dx = F16_0, target_dy = F16_0;
static u16 change_timer = 0;
static bool is_active = false;
static u8 bg_mode = BG_MODE_MENU; 
static fix16 fade_level = F16_0;
static bool is_fading = false, is_flashing = false;
static u8 current_intensity = 1;
static fix16 flash_level = F16_0;

static const u16 target_palette[8] = {
    0x0000, 0x0440, 0x0400, 0x0024, 0x0044, 0x0040, 0x0404, 0x0004
};

// --- HILFSFUNKTIONEN ---

static void update_palette_fade() {
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
        
        if (is_flashing && bg_mode == BG_MODE_SPACE) {
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
            if (i == 0) PAL_setColor(0, 0x0000); 
        }
    }
}

static void update_star_tiles(u8 intensity) {
    u32 tiles[16];
    u16 length = 1 + (intensity * 2);
    if (length > 16) length = 16;
    u16 num_tiles = (length > 8) ? 2 : 1;

    for (u16 t = 0; t < 3; t++) { 
        u32 p = t + 1; 
        u32 pattern = (p << 12); // Zentrierter 1px Punkt
        memset(tiles, 0, sizeof(tiles));
        for(u16 i = 0; i < length; i++) tiles[i] = pattern;
        VDP_loadTileData(tiles, TILE_STAR_BASE + (t * 2), num_tiles, CPU);
    }
}

static void draw_stars_dynamic(u8 intensity) {
    VDP_clearPlane(BG_B, TRUE);
    u16 count = 45 + (intensity * 12); 
    for (u16 i = 0; i < count; i++) {
        u16 rx = random() % 64, ry = random() % 32;
        u16 channel = i % 3; 
        u16 tile_idx = TILE_STAR_BASE + (channel * 2);
        
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, tile_idx), rx, ry);
        if (intensity > 3 && (intensity * 2) > 8) {
             VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, tile_idx + 1), rx, (ry + 1) % 32);
        }
    }
}

void draw_menu_shapes() {
    VDP_clearPlane(BG_B, TRUE);
    for (u16 zy = 0; zy < 4; zy++) {
        for (u16 zx = 0; zx < 8; zx++) {
            if ((random() % 10) < 4) {
                u16 type = random() % 7, rot = random() % 4;
                u16 rx = (zx * 8) + (random() % 4), ry = (zy * 8) + (random() % 4);
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
    PAL_setColor(0, 0x0000); 
    for (u16 i = 1; i <= 7; i++) {
        u32 tile[8]; u32 c = i & 0xF;
        tile[0] = 0x00000000;
        u32 row = (c<<28)|(c<<24)|(c<<20)|(c<<16)|(c<<12)|(c<<8)|0x00;
        for(u16 j = 1; j < 8; j++) tile[j] = row;
        VDP_loadTileData(tile, TILE_MENU_BLOCK_BASE + (i - 1), 1, CPU);
    }
    update_star_tiles(1);
}

void menu_bg_set_mode(u8 mode) {
    if (bg_mode == mode) return;
    bg_mode = mode;
    curr_dx = F16_0; curr_dy = F16_0;
    is_flashing = false; flash_level = F16_0;
    if (is_active) {
        if (bg_mode == BG_MODE_MENU) {
            fade_level = FIX16(1.0); target_dx = FIX16(0.2); target_dy = FIX16(0.1); draw_menu_shapes();
        } else {
            target_dx = F16_0; target_dy = STARFIELD_BASE_SP; update_star_tiles(1); draw_stars_dynamic(1);
        }
        update_palette_fade();
    }
}

void menu_bg_set_active(bool active) {
    if (active == is_active) return; 
    is_active = active;
    is_fading = true;
    if (is_active) {
        if (bg_mode == BG_MODE_MENU) {
            target_dx = FIX16(0.2); 
            target_dy = FIX16(0.1); 
            draw_menu_shapes();
        } else {
            target_dx = F16_0; 
            target_dy = STARFIELD_BASE_SP; 
            update_star_tiles(1); 
            draw_stars_dynamic(1);
        }
    }
}

void menu_bg_set_intensity(u8 level) {
    if (bg_mode != BG_MODE_SPACE) return;
    if (level < 1) level = 1; 
    if (level > 10) level = 10;

    // Trigger Flash bei Level-Up / Reset
    if (level == 1 && current_intensity > 5) {
        is_flashing = true;
        flash_level = FIX16(1.0);
    }

    current_intensity = level;
    fix16 speed_factor = (level < 7) ? FIX16(-0.2) : FIX16(-0.35);
    target_dy = STARFIELD_BASE_SP + F16_mul(FIX16(level - 1), speed_factor);
    
    update_star_tiles(level);
    draw_stars_dynamic(level);
    if (is_active && !is_flashing) fade_level = FIX16(1.0);
    update_palette_fade();
}

void menu_bg_update() {
    if (is_active || fade_level > F16_0 || is_flashing) {
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
        } else {
            curr_dx = F16_0;
            if (curr_dy < target_dy) curr_dy += FIX16(0.05); else if (curr_dy > target_dy) curr_dy -= FIX16(0.05);
        }
        scroll_x += curr_dx; sub_y += curr_dy;
        VDP_setHorizontalScroll(BG_B, F16_toInt(scroll_x));
        VDP_setVerticalScroll(BG_B, F16_toInt(sub_y));

        if (is_flashing) {
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
                if (fade_level <= F16_0) { fade_level = F16_0; is_fading = false; VDP_clearPlane(BG_B, TRUE); }
            }
            update_palette_fade();
        }
    }
}