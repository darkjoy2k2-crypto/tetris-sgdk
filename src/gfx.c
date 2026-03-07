#include "gfx.h"

void gfx_init() {
    // 1. Farben in Palette 1 (Indizes 17-31)
    // Wir behalten die kräftigen Grundfarben
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x00FFFF)); // 1: Cyan (I)
    PAL_setColor(18, RGB24_TO_VDPCOLOR(0xFFFF00)); // 2: Gelb (O)
    PAL_setColor(19, RGB24_TO_VDPCOLOR(0xFF00FF)); // 3: Lila (T)
    PAL_setColor(20, RGB24_TO_VDPCOLOR(0x00FF00)); // 4: Grün (S)
    PAL_setColor(21, RGB24_TO_VDPCOLOR(0xFF0000)); // 5: Rot (Z)
    PAL_setColor(22, RGB24_TO_VDPCOLOR(0x5555FF)); // 6: Blau (J)
    PAL_setColor(23, RGB24_TO_VDPCOLOR(0xFFA500)); // 7: Orange (L)
    
    PAL_setColor(24, RGB24_TO_VDPCOLOR(0x444444)); // 8: Dunkleres Grau (Ghost)
    
    // Wir nutzen Index 25 für einen sanften Schatten (Dunklere Nuance der Steine)
    PAL_setColor(25, RGB24_TO_VDPCOLOR(0x222222)); // 9: Tiefer Schatten
    
    // Index 26 ist unser "Licht"
    PAL_setColor(26, RGB24_TO_VDPCOLOR(0xFFFFFF)); // 10 (A): Reinweiß (Lichtkante)

    // 2. Leer-Tile laden (Hintergrund-Punkt für die Orientierung)
    const u32 empty_tile[8] = {0,0,0,0x00033000,0x00033000,0,0,0};
    VDP_loadTileData(empty_tile, TILE_EMPTY_INDEX, 1, CPU);

    // 3. 7 Block-Tiles mit 3D-Bevel generieren (Ohne schwarzen Rand)
    // A = Weiß (Licht), 9 = Schatten, c = Grundfarbe
    for (u8 i = 0; i < 7; i++) {
        u8 c = i + 1; // Grundfarbe
        
        // Zeile 0: Obere Lichtkante (Komplett Weiß)
        u32 row_top = 0xAAAAAAAA; 
        
        // Zeile 1-6: Lichtkante links (A), Farbe in der Mitte (c), Schatten rechts (9)
        u32 row_mid = (10 << 28) | (c << 24) | (c << 20) | (c << 16) | 
                      (c << 12) | (c << 8)  | (c << 4)  | 9; 

        // Zeile 7: Untere Schattenkante
        u32 row_bottom = 0x99999999;

        u32 crystal_tile[8] = {
            row_top,    // Oben: Licht
            row_mid,    // Mitte: Licht links, Farbe, Schatten rechts
            row_mid,
            row_mid,
            row_mid,
            row_mid,
            row_mid,
            row_bottom  // Unten: Schatten
        };
        
        VDP_loadTileData(crystal_tile, TILE_BLOCK_BASE + i, 1, CPU);
    }

    // 4. Ghost-Tile (Schattenstein) 
    // Er bekommt nur eine dezente Lichtkante oben links, um "immateriell" zu wirken
    u32 g = 8; // Ghost-Grau
    u32 ghost_top = 0x88888888;
    u32 ghost_mid = 0x80000008;
    u32 ghost_tile[8] = {ghost_top, ghost_mid, ghost_mid, ghost_mid, 
                         ghost_mid, ghost_mid, ghost_mid, ghost_top};
    VDP_loadTileData(ghost_tile, TILE_GHOST_INDEX, 1, CPU);
}