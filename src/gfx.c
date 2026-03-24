#include "gfx.h"

const u32 tile_skull[8] = {
    0x07666670, //  XXXX  (border)
    0x76666667, // XXXXXX (border + fill)
    0x76066067, // XX XX XX (eyes + border)
    0x76066067, // XX XX XX (eyes + border)
    0x76666667, // XXXXXX (border + fill)
    0x07666670, //  XXXX  (border)
    0x07600607, //  X  X  (teeth + border)
    0x07777770  //  border
};

const u32 tile_heart[8] = {
    0x0CC00CC0, //  RR  RR (no border needed - symmetric top)
    0x7CCCCCC7, // RRRRRRRR (border + fill)
    0x7CCCCCC7, // RRRRRRRR (border + fill)
    0x7CCCCCC7, // RRRRRRRR (border + fill)
    0x7CCCCCC7, //  RRRRRR (border + fill)
    0x07CCCC70, //   RRRR  (border)
    0x0007CC00, //    RR   (border)
    0x00000000  //          
};

void gfx_load_tiles(u16 offset) {

PAL_setColor(37, RGB24_TO_VDPCOLOR(0x222222)); // PAL2 COL5 ANTHRAZIT (Block-Schatten/Sprite-Teil)
PAL_setColor(38, RGB24_TO_VDPCOLOR(0xFFFFFF)); // PAL2 COL6 WEISS (Block-Highlight/Sprite-Teil)
PAL_setColor(39, RGB24_TO_VDPCOLOR(0x000044)); // PAL2 COL7 DUNKELBLAU (Hintergrund/Schatten)
PAL_setColor(40, RGB24_TO_VDPCOLOR(0x0000FF)); // PAL2 COL8 BLAU (Tetromino I / Kristall)
PAL_setColor(41, RGB24_TO_VDPCOLOR(0xFFFF00)); // PAL2 COL9 GELB (Tetromino O / Kristall)
PAL_setColor(42, RGB24_TO_VDPCOLOR(0xFF00FF)); // PAL2 COL10 MAGENTA (Tetromino T / Kristall)
PAL_setColor(43, RGB24_TO_VDPCOLOR(0x00FF00)); // PAL2 COL11 GRÜN (Tetromino S / Kristall)
PAL_setColor(44, RGB24_TO_VDPCOLOR(0xFF0000)); // PAL2 COL12 ROT (Tetromino Z / Kristall)
PAL_setColor(45, RGB24_TO_VDPCOLOR(0x5555FF)); // PAL2 COL13 HELLBLAU (Tetromino J / Kristall)
PAL_setColor(46, RGB24_TO_VDPCOLOR(0xFFA500)); // PAL2 COL14 ORANGE (Tetromino L / Kristall)
PAL_setColor(47, RGB24_TO_VDPCOLOR(0x444444)); // PAL2 COL15 DUNKELGRAU (Ghost-Piece Umrandung)


    const u32 empty_tile[8] = {
        0x01010101,
        0x10101010,
        0x01010101,
        0x10101010,
        0x01010101,
        0x10101010,
        0x01010101,
        0x10101010
    };

    



    VDP_loadTileData(empty_tile, offset, 1, CPU);

    for (u8 i = 0; i < 7; i++) {
        u8 c = i + 8;
        u32 row_top = 0x66666666;
        u32 row_mid = (6 << 28) | (c << 24) | (c << 20) | (c << 16) | 
                      (c << 12) | (c << 8)  | (c << 4)  | 5;
        u32 row_bottom = 0x55555555;

        u32 crystal_tile[8] = {
            row_top, row_mid, row_mid, row_mid, 
            row_mid, row_mid, row_mid, row_bottom
        };
        VDP_loadTileData(crystal_tile, offset + 1 + i, 1, CPU);
    }

    // Schachbrett-Muster: Transparent (0) und Rot (12) abwechselnd,
    // jede Reihe gegenueber der vorherigen versetzt.
    u32 ghost_tile[8] = {
        0x0C0C0C0C,
        0xC0C0C0C0,
        0x0C0C0C0C,
        0xC0C0C0C0,
        0x0C0C0C0C,
        0xC0C0C0C0,
        0x0C0C0C0C,
        0xC0C0C0C0
    };
    VDP_loadTileData(ghost_tile, offset + 8, 1, CPU);
}

#include <genesis.h>
#include "fonts.h"

void UI_init_fonts_and_palettes() {
    // 1. Font laden (einmalig für VDP)
    VDP_loadFont(&TS_FONT_CLEAR, CPU);
    VDP_setTextPalette(PAL3); // Standardmäßig auf Weiß setzen

    // 2. PAL3: Standard Text (Weiß/Grau)
    PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);

    // 3. PAL1: Highlight (Gold/Gelb)
    PAL_setPalette(PAL1, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0x332200)); // sehr dunkel
    PAL_setColor(16 + 5, RGB24_TO_VDPCOLOR(0x886600)); // mittel
    PAL_setColor(16 + 6, RGB24_TO_VDPCOLOR(0xFFD533)); // hell
    PAL_setColor(16 + 7, RGB24_TO_VDPCOLOR(0xFFF1A0)); // highlight

    // 4. PAL2: Warnung/Selektion (Rot)
    PAL_setPalette(PAL2, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(32 + 1, RGB24_TO_VDPCOLOR(0x330000)); // sehr dunkel
    PAL_setColor(32 + 5, RGB24_TO_VDPCOLOR(0x880000)); // mittel
    PAL_setColor(32 + 6, RGB24_TO_VDPCOLOR(0xFF2222)); // hellrot
    PAL_setColor(32 + 7, RGB24_TO_VDPCOLOR(0xFF9A9A)); // highlight


VDP_setTextPalette(PAL1);
}
