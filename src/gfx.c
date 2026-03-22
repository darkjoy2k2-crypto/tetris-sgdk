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
        0x77777777, 0x00000007, 0x00000007, 0x00000007,
        0x00000007, 0x00000007, 0x00000007, 0x00000007
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

    u32 g = 15; 
    u32 ghost_mid = (g << 28) | (0 << 24) | (0 << 20) | (0 << 16) | 
                    (0 << 12) | (0 << 8)  | (0 << 4)  | g;
    u32 ghost_tile[8] = {
        0xFFFFFFFF, ghost_mid, ghost_mid, ghost_mid, 
        ghost_mid, ghost_mid, ghost_mid, 0xFFFFFFFF
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
    PAL_setColor(16 + 5, RGB24_TO_VDPCOLOR(0x666600)); // Schatten/Dunkel
    PAL_setColor(16 + 6, RGB24_TO_VDPCOLOR(0xFFFF00)); // Hauptfarbe Gelb
    PAL_setColor(16 + 7, RGB24_TO_VDPCOLOR(0x666600));

    // 4. PAL2: Warnung/Selektion (Rot)
    PAL_setPalette(PAL2, PAL_FONT_CLEAR.data, CPU);
    PAL_setColor(32 + 1, RGB24_TO_VDPCOLOR(0x440000)); // Sehr Dunkel
    PAL_setColor(32 + 5, RGB24_TO_VDPCOLOR(0x880000)); // Mittel
    PAL_setColor(32 + 6, RGB24_TO_VDPCOLOR(0xFF0000)); // Hellrot
    PAL_setColor(32 + 7, RGB24_TO_VDPCOLOR(0xFF8888)); // Rosa/Highlight
// JETZT: Wir überschreiben Index 1 und 7 in PAL2 mit den Board-Farben
    // Wir holen uns die Farben direkt aus PAL0, damit sie identisch sind!
    u16 colorCyan = PAL_getColor(1); // Farbe von Cyan (I-Piece) aus PAL0
    u16 colorRed  = PAL_getColor(7); // Farbe von Rot (Z-Piece) aus PAL0

    PAL_setColor(32 + 1, colorCyan); // Setzt Cyan auf Index 1 von PAL2
    PAL_setColor(32 + 7, colorRed);  // Setzt Rot auf Index 7 von PAL2


    
VDP_setTextPalette(PAL1);
}
