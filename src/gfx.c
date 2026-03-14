#include "gfx.h"

const u32 tile_skull[8] = {
    0x00666600, //   XXXX  
    0x06666660, //  XXXXXX 
    0x66066066, // XX XX XX (Augenhöhlen)
    0x66066066, // XX XX XX
    0x06666660, //  XXXXXX
    0x00666600, //   XXXX
    0x00600600, //   X  X  (Zähne/Kiefer)
    0x00666600  //   XXXX
};

const u32 tile_heart[8] = {
    0x0CC00CC0, //  RR  RR 
    0xCCCCCCCC, // RRRRRRRR
    0xCCCCCCCC, // RRRRRRRR
    0xCCCCCCCC, // RRRRRRRR
    0x0CCCCCC0, //  RRRRRR 
    0x00CCCC00, //   RRRR  
    0x000CC000, //    RR   
    0x00000000  //          
};

void gfx_load_tiles(u16 offset) {
    PAL_setColor(32 + 7, RGB24_TO_VDPCOLOR(0x000044)); 
    
    PAL_setColor(40, RGB24_TO_VDPCOLOR(0x0000FF)); 
    PAL_setColor(41, RGB24_TO_VDPCOLOR(0xFFFF00)); 
    PAL_setColor(42, RGB24_TO_VDPCOLOR(0xFF00FF)); 
    PAL_setColor(43, RGB24_TO_VDPCOLOR(0x00FF00)); 
    PAL_setColor(44, RGB24_TO_VDPCOLOR(0xFF0000)); 
    PAL_setColor(45, RGB24_TO_VDPCOLOR(0x5555FF)); 
    PAL_setColor(46, RGB24_TO_VDPCOLOR(0xFFA500)); 
    
    PAL_setColor(47, RGB24_TO_VDPCOLOR(0x444444)); 
    PAL_setColor(32 + 5, RGB24_TO_VDPCOLOR(0x222222)); 
    PAL_setColor(32 + 6, RGB24_TO_VDPCOLOR(0xFFFFFF)); 

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

VDP_setTextPalette(PAL1);
}
