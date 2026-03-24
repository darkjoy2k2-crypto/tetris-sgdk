#include "menu_bg.h"
#include "bg.h"
#include "states/game/game_core.h"
#include <string.h>
#include "states/states.h"

#define TILE_MENU_BLOCK_BASE 510 
#define TILE_STAR_BASE       520
// SPR_init() reserviert standardmaessig 420 Tiles direkt vor dem Font-Bereich.
// Diese Reserve wird hier immer mit einkalkuliert, auch wenn SPR_init() noch nicht lief.
#define SPRITE_VRAM_RESERVE_TILES 420

#define BG_SPEED_BASE     FIX16(0.3)
#define BG_ACCEL          FIX16(0.02)
#define F16_0             FIX16(0)

#define RIISTAR_SPRING_K       FIX16(0.10)
#define RIISTAR_DAMPING        FIX16(0.88)
#define RIISTAR_PULSE_SOFT     FIX16(0.35)
#define RIISTAR_PULSE_SOFTDROP FIX16(0.75)
#define RIISTAR_PULSE_HARDDROP FIX16(1.35)

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
static fix16 riistar_group_scroll[5] = { F16_0, F16_0, F16_0, F16_0, F16_0 };
static s16 riistar_hscroll_lines[240];
static s16 riistar_map_row_scroll[32];
static fix16 riistar_vertical_center = F16_0;
static fix16 riistar_vertical_amplitude = F16_0;
static fix16 riistar_vertical_max = F16_0;
static fix16 riistar_y_offset = F16_0;
static fix16 riistar_y_velocity = F16_0;
static fix16 riistar_speed_scale = F16_0;
static s16 riistar_stack_top_row = -1;

static const u8 riistar_group_sizes[5] = { 6, 8, 8, 4, 6 };
static const fix16 riistar_group_speeds[5] = {
    FIX16(-1.60), FIX16(-1.20), FIX16(-0.85), FIX16(-0.55), FIX16(-0.35)
};

static const u16 target_palette[8] = {
    0x0000, 0x0440, 0x0400, 0x0024, 0x0044, 0x0040, 0x0404, 0x0004
};

static void draw_riistar_background() {
    u16 tileBase;
    u16 tileCount = riistar_bg_tileset.numTile;
    u16 safeUserMax;
    u16 mapW;
    u16 mapH;
    s16 maxScrollPx;
    u16 baseAttr;

    if (tileCount == 0) return;

    // Oberes Limit muss sowohl den aktuellen User-Max als auch den festen Sprite-Reservebereich beachten.
    safeUserMax = TILE_USER_MAX_INDEX;
    {
        u16 spriteReservedMax = (u16)(TILE_FONT_INDEX - SPRITE_VRAM_RESERVE_TILES - 1);
        if (spriteReservedMax < safeUserMax) safeUserMax = spriteReservedMax;
    }

    // Reserviere den oberen Bereich im sicheren User-Tile-Space, um Kollisionen mit BG_A zu vermeiden.
    if ((u16)(tileCount + 16) >= safeUserMax) {
        tileBase = TILE_USER_INDEX;
    } else {
        tileBase = safeUserMax - tileCount - 16;
        if (tileBase < TILE_USER_INDEX) tileBase = TILE_USER_INDEX;
    }

    VDP_clearPlane(BG_B, TRUE);

    mapW = riistar_bg_tilemap.w;
    mapH = riistar_bg_tilemap.h;
    if (mapW > 64) mapW = 64;
    if (mapH > 32) mapH = 32;
    maxScrollPx = (s16)(mapH << 3) - (IS_PAL_SYSTEM ? 240 : 224);
    if (maxScrollPx < 0) maxScrollPx = 0;
    riistar_vertical_center = FIX16(maxScrollPx / 2);
    riistar_vertical_amplitude = FIX16(maxScrollPx / 2);
    riistar_vertical_max = FIX16(maxScrollPx);
    riistar_y_offset = F16_0;
    riistar_y_velocity = F16_0;

    // Wichtig: derselbe IND/Tile-Base muss fuer TileSet-Upload und TileMap-Basetile genutzt werden.
    baseAttr = TILE_ATTR_FULL(PAL0, 0, 0, 0, tileBase);

    // Einfachster Testpfad: Tileset und Tilemap direkt per CPU schreiben.
    if (!VDP_loadTileSet(&riistar_bg_tileset, tileBase, CPU)) {
        return;
    }

    VDP_setTileMapEx(BG_B, &riistar_bg_tilemap, baseAttr, 0, 0, 0, 0, mapW, mapH, CPU);

    scroll_x = F16_0;
    sub_y = riistar_vertical_center;
    curr_dx = F16_0;
    curr_dy = F16_0;
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, F16_toInt(sub_y));
}

static void reset_riistar_scroll() {
    scroll_x = F16_0;
    sub_y = riistar_vertical_center;
    curr_dx = F16_0;
    curr_dy = F16_0;
    for (u16 i = 0; i < 5; i++) riistar_group_scroll[i] = F16_0;
    for (u16 y = 0; y < 32; y++) riistar_map_row_scroll[y] = 0;
    for (u16 y = 0; y < 240; y++) riistar_hscroll_lines[y] = 0;
    riistar_y_offset = F16_0;
    riistar_y_velocity = F16_0;
    riistar_speed_scale = F16_0;
    riistar_stack_top_row = -1;
}

static void update_riistar_scroll_lines(s16 verticalScrollPx) {
    const u16 screenHeight = IS_PAL_SYSTEM ? 240 : 224;
    s16 row = 31;
    for (u16 g = 0; g < 5; g++) {
        for (u16 r = 0; r < riistar_group_sizes[g] && row >= 0; r++) {
            riistar_map_row_scroll[row--] = F16_toInt(riistar_group_scroll[g]);
        }
    }

    for (u16 line = 0; line < screenHeight; line++) {
        s16 srcPixelY = verticalScrollPx + (s16)line;
        u16 srcRow;

        if (srcPixelY < 0) srcPixelY = 0;
        srcRow = (u16)(srcPixelY >> 3);
        if (srcRow > 31) srcRow = 31;

        riistar_hscroll_lines[line] = riistar_map_row_scroll[srcRow];
    }

    VDP_setHorizontalScrollLine(BG_B, 0, riistar_hscroll_lines, screenHeight, CPU);
}

void menu_bg_riistar_set_stack_top(s16 topRow)
{
    s16 fillHeight;
    const s16 maxFillForFullSpeed = (BOARD_HEIGHT - 2);

    if (topRow < -1) topRow = -1;
    if (topRow >= BOARD_HEIGHT) topRow = -1;

    riistar_stack_top_row = topRow;

    if (topRow < 0) {
        riistar_speed_scale = F16_0;
        return;
    }

    fillHeight = (s16)(BOARD_HEIGHT - topRow);
    if (fillHeight <= 0) {
        riistar_speed_scale = F16_0;
        return;
    }

    riistar_speed_scale = F16_div(FIX16(fillHeight), FIX16(maxFillForFullSpeed));
    if (riistar_speed_scale > FIX16(1.0)) riistar_speed_scale = FIX16(1.0);
    if (riistar_speed_scale < F16_0) riistar_speed_scale = F16_0;
}

void menu_bg_riistar_pulse(u8 pulseType)
{
    fix16 impulse = F16_0;

    if (bg_mode != BG_MODE_RIISTAR) return;
    if (!is_active) return;

    switch (pulseType) {
        case 1: impulse = RIISTAR_PULSE_SOFT; break;
        case 2: impulse = RIISTAR_PULSE_SOFTDROP; break;
        case 3: impulse = RIISTAR_PULSE_HARDDROP; break;
        default: return;
    }

    riistar_y_velocity += impulse;
}

// --- HILFSFUNKTIONEN ---

static void update_palette_fade() {
    if (bg_mode == BG_MODE_RIISTAR) {
        const u16* pal = riistar_bg_palette.data;

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
    scroll_x = F16_0; sub_y = F16_0;
    curr_dx = F16_0; curr_dy = F16_0;
    target_dx = F16_0; target_dy = F16_0;
    is_flashing = false; flash_level = F16_0;

    if (bg_mode == BG_MODE_RIISTAR) {
        VDP_setScrollingMode(HSCROLL_LINE, VSCROLL_PLANE);
        reset_riistar_scroll();
        VDP_setHorizontalScroll(BG_B, 0);
        VDP_setVerticalScroll(BG_B, F16_toInt(sub_y));
    } else {
        VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
        VDP_setHorizontalScroll(BG_B, 0);
        VDP_setVerticalScroll(BG_B, 0);
    }

    if (is_active) {
        if (bg_mode == BG_MODE_MENU) {
            fade_level = FIX16(1.0); target_dx = FIX16(0.2); target_dy = FIX16(0.1); draw_menu_shapes();
        } else if (bg_mode == BG_MODE_SPACE) {
            target_dx = F16_0; target_dy = STARFIELD_BASE_SP; update_star_tiles(1); draw_stars_dynamic(1);
        } else {
            target_dx = F16_0;
            target_dy = F16_0;
            fade_level = F16_0;
            is_fading = true;
            draw_riistar_background();
            update_riistar_scroll_lines(F16_toInt(sub_y));
        }
        update_palette_fade();
    }
}

void menu_bg_set_active(bool active) {
    // Wenn wir aktivieren wollen, aber das globale Flag dagegen spricht: Abbruch.
    if (active && !GET_FLAG(config.flags, FLAG_BG)) {
        is_active = false;
        is_fading = true; // Sorgt dafür, dass Reste ausgeblendet werden
        return;
    }

    if (active == is_active) return; 
    is_active = active;
    is_fading = true;
    scroll_x = F16_0;
    sub_y = F16_0;
    curr_dx = F16_0;
    curr_dy = F16_0;
    target_dx = F16_0;
    target_dy = F16_0;
    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, F16_toInt(sub_y));
    
    if (is_active) {
        if (bg_mode == BG_MODE_MENU) {
            target_dx = FIX16(0.2); 
            target_dy = FIX16(0.1); 
            draw_menu_shapes();
        } else if (bg_mode == BG_MODE_SPACE) {
            target_dx = F16_0; 
            target_dy = STARFIELD_BASE_SP; 
            update_star_tiles(1); 
            draw_stars_dynamic(1);
        } else {
            target_dx = F16_0;
            target_dy = F16_0;
            reset_riistar_scroll();
            draw_riistar_background();
            update_riistar_scroll_lines(F16_toInt(sub_y));
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
        } else if (bg_mode == BG_MODE_SPACE) {
            curr_dx = F16_0;
            if (curr_dy < target_dy) curr_dy += FIX16(0.05); else if (curr_dy > target_dy) curr_dy -= FIX16(0.05);
        } else {
            curr_dx = F16_0;
            for (u16 g = 0; g < 5; g++) {
                riistar_group_scroll[g] += F16_mul(riistar_group_speeds[g], riistar_speed_scale);
            }

            riistar_y_velocity += F16_mul((F16_0 - riistar_y_offset), RIISTAR_SPRING_K);
            riistar_y_velocity = F16_mul(riistar_y_velocity, RIISTAR_DAMPING);
            riistar_y_offset += riistar_y_velocity;

            sub_y = riistar_vertical_center + riistar_y_offset;
            if (sub_y < F16_0) {
                sub_y = F16_0;
                riistar_y_offset = sub_y - riistar_vertical_center;
                if (riistar_y_velocity < F16_0) riistar_y_velocity = F16_0;
            } else if (sub_y > riistar_vertical_max) {
                sub_y = riistar_vertical_max;
                riistar_y_offset = sub_y - riistar_vertical_center;
                if (riistar_y_velocity > F16_0) riistar_y_velocity = F16_0;
            }

            update_riistar_scroll_lines(F16_toInt(sub_y));
        }
        scroll_x += curr_dx; sub_y += curr_dy;
        if (bg_mode != BG_MODE_RIISTAR) {
            VDP_setHorizontalScroll(BG_B, F16_toInt(scroll_x));
        }
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