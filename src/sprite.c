#include "sprite.h"
#include "sprites.h"
#include "states/states.h"

#include "gfx.h" // Für die Ressourcen-Referenz

GameSprite gameSprites[10];

void sprites_init() {
    SPR_init();
    for(u16 i = 0; i < 10; i++) {
        memset(&gameSprites[i], 0, sizeof(GameSprite));
        // Erzeuge SGDK Sprite-Instanz
        gameSprites[i].vdpSprite = SPR_addSprite(&anim_norotate, -32, -32, TILE_ATTR(PAL0, 0, 0, 0)); //SPRITEPALETTE
        gameSprites[i].attr = 0; // Standard: Invisible
    }
}
void sprites_update() {
    for(u16 i = 0; i < 10; i++) {
        GameSprite* gs = &gameSprites[i];
        
        // Sichtbarkeit prüfen
        if (!(gs->attr & 1)) {
            // Aus dem Sichtfeld schieben
            SPR_setPosition(gs->vdpSprite, -128, -128); 
            continue;
        }

        // 1. Frame-Rotation innerhalb der Reihe (0, 1, 2, 3)
        gs->animTimer++;
        if (gs->animTimer >= GET_TICKS(30)) {
            gs->frame = (gs->frame + 1) % 4; 
            gs->animTimer = 0;
        }

        // 2. Wechsel der Animations-Reihe (0 oder 1)
        gs->stateTimer++;
        if (gs->stateTimer >= GET_TICKS(60)) {
            gs->animation = (gs->animation == 0) ? 1 : 0;
            gs->stateTimer = 0;
        }

        // --- DER ENTSCHEIDENDE TEIL ---
        // Wähle die Reihe (0 = oben, 1 = unten)
        SPR_setAnim(gs->vdpSprite, gs->animation);
        // Wähle das Bild in der Reihe (0, 1, 2, 3)
        SPR_setFrame(gs->vdpSprite, gs->frame);
        
        SPR_setPosition(gs->vdpSprite, gs->x, gs->y);
    }
    
    // VDP-Daten abschicken
    SPR_update();
}

void sprites_set_visible(u8 index, bool visible) {
    if (index < 10) {
        if (visible) gameSprites[index].attr |= 1;
        else gameSprites[index].attr &= ~1;
    }
}