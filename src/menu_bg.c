#include "menu_bg.h"
#include "game_core.h"

#define TILE_MENU_BLOCK_BASE 510 

// --- TUNING ---
#define BG_SPEED_BASE       FIX16(0.3)  // Grundgeschwindigkeit
#define BG_ACCEL            FIX16(0.02)  // Snappy transition fuer spuerbare Zufalligkeit
#define F16_0               FIX16(0)     // Konstante fuer Null-Checks laut Vorgabe

static fix16 scroll_x = F16_0;
static fix16 sub_y = F16_0;

static fix16 curr_dx = F16_0;
static fix16 curr_dy = F16_0;
static fix16 target_dx = F16_0;
static fix16 target_dy = F16_0;
static u16 change_timer = 0;

static bool is_active = false;
static fix16 fade_level = F16_0;
static bool is_fading = false;

static const u16 target_palette[8] = {
    0x0000, 0x0440, 0x0400, 0x0024, 0x0044, 0x0040, 0x0404, 0x0004
};

// --- Hilfsfunktionen ---

static void update_palette_fade() {
    for (u16 i = 0; i < 7; i++) {
        u16 target = target_palette[i + 1];
        u16 r = (target >> 1) & 0x7;
        u16 g = (target >> 5) & 0x7;
        u16 b = (target >> 9) & 0x7;

        u16 fr = F16_toInt(F16_mul(FIX16(r), fade_level));
        u16 fg = F16_toInt(F16_mul(FIX16(g), fade_level));
        u16 fb = F16_toInt(F16_mul(FIX16(b), fade_level));

        PAL_setColor(49 + i, (fb << 9) | (fg << 5) | (fr << 1));
    }
}

static void load_colored_tile(u16 colorIndex) {
    u32 tile[8];
    u32 c = colorIndex & 0xF;
    tile[0] = 0x00000000;
    u32 row = (c<<28)|(c<<24)|(c<<20)|(c<<16)|(c<<12)|(c<<8)|0x00;
    for(u16 i = 1; i < 8; i++) tile[i] = row;
    VDP_loadTileData(tile, TILE_MENU_BLOCK_BASE + (colorIndex - 1), 1, CPU);
}

static void draw_bg_shape(u16 x, u16 y) {
    u16 type = random() % 7;
    u16 rotation = random() % 4;
    for (u16 i = 0; i < 4; i++) {
        s16 px = (x + PIECES[type][rotation][i][0]) % 64;
        s16 py = (y + PIECES[type][rotation][i][1]) % 32;
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_MENU_BLOCK_BASE + type), px, py);
    }
}

// --- Hauptfunktionen ---

void menu_bg_init() {
    PAL_setColor(48, 0x0000); 
    for (u16 i = 1; i <= 7; i++) load_colored_tile(i);
    is_active = false;
    fade_level = F16_0;
    is_fading = false;
}

void menu_bg_set_active(bool active) {
    if (active == is_active) return; 
    is_active = active;
    is_fading = true;

    if (is_active) {
        // Initiale Drift-Werte
        target_dx = FIX16(0.2);
        target_dy = FIX16(0.1);
        change_timer = 60; // Schneller erster Richtungswechsel

        VDP_clearPlane(BG_B, TRUE);
        for (u16 zy = 0; zy < 4; zy++) {
            for (u16 zx = 0; zx < 8; zx++) {
                if ((random() % 10) < 4) {
                    u16 rx = (zx * 8) + (random() % 4);
                    u16 ry = (zy * 8) + (random() % 4);
                    draw_bg_shape(rx, ry);
                }
            }
        }
    }
}

void menu_bg_update() {
    if (is_active || fade_level > F16_0) {
        if (change_timer > 0) change_timer--;
        
        if (change_timer == 0 && is_active) {
            u16 angle = random() % 1024;
            target_dx = F16_mul(cosFix16(angle), BG_SPEED_BASE);
            target_dy = F16_mul(sinFix16(angle), BG_SPEED_BASE);
            change_timer = 240 + (random() % 300);
        }

        // Trägheits-Anpassung
        if (curr_dx < target_dx) curr_dx += BG_ACCEL;
        else if (curr_dx > target_dx) curr_dx -= BG_ACCEL;

        if (curr_dy < target_dy) curr_dy += BG_ACCEL;
        else if (curr_dy > target_dy) curr_dy -= BG_ACCEL;

        scroll_x += curr_dx;
        sub_y += curr_dy;
        
        VDP_setHorizontalScroll(BG_B, F16_toInt(scroll_x));
        VDP_setVerticalScroll(BG_B, F16_toInt(sub_y));
    }

    if (is_fading) {
        if (is_active) {
            fade_level += FIX16(0.05);
            if (fade_level >= FIX16(1.0)) { fade_level = FIX16(1.0); is_fading = false; }
        } else {
            fade_level -= FIX16(0.05);
            if (fade_level <= F16_0) { 
                fade_level = F16_0; 
                is_fading = false; 
                VDP_clearPlane(BG_B, TRUE); 
                // Reset der Scrollwerte bei Inaktivitaet
                scroll_x = F16_0;
                sub_y = F16_0;
            }
        }
        update_palette_fade();
    }
}