#include "sprite.h"
#include "sprites.h" // Ressourcen (anim_skull, anim_norotate)
#include <string.h>

// Definition des Arrays
GameSprite gameSprites[10];

void sprites_init() {
    // 1. Sprite-Engine sauber zurücksetzen statt nur neu init
    if (SPR_isInitialized()) {
        SPR_end(); 
    }
    SPR_init();

    for(u16 i = 0; i < 10; i++) {
        memset(&gameSprites[i], 0, sizeof(GameSprite));
        
        // 2. Fehlerprüfung: SPR_addSprite kann NULL zurückgeben
        gameSprites[i].vdpSprite = SPR_addSprite(&anim_norotate, -128, -128, TILE_ATTR(PAL2, 0, 0, 0));
        
        if (gameSprites[i].vdpSprite == NULL) {
            // Hier kritischer Fehler: VDP Speicher voll!
            continue; 
        }
        
        gameSprites[i].type = SPRITE_TYPE_NOROTATE;
        gameSprites[i].animDir = 1;
    }
    

    
    // Initial-Offsets für Tetromino & Shadow
    gameSprites[0].offsetX = -8; gameSprites[0].offsetY = -8;
    gameSprites[1].offsetX = -8; gameSprites[1].offsetY = -8;

    // Index 2: Next Piece Offset
    gameSprites[2].offsetX = -8;
    gameSprites[2].offsetY = -8;

    // Index 3: Hold Piece Offset
    gameSprites[3].offsetX = -8;
    gameSprites[3].offsetY = -8;
}

void sprites_update() {
    for(u16 i = 0; i < 10; i++) {
        GameSprite* gs = &gameSprites[i];
        
        // FAKTISCHER FIX: Wenn kein VDP-Sprite existiert, überspringen
        if (gs->vdpSprite == NULL) continue;

        if (!(gs->attr & SPRITE_ATTR_VISIBLE)) {
            SPR_setPosition(gs->vdpSprite, -128, -128); 
            continue;
        }



        if (gs->type == SPRITE_TYPE_SKULL) {
            gs->animTimer++;
            if (gs->animTimer >= GET_TICKS(SKULL_NEXTFRAME)) {
                gs->frame += gs->animDir;
                if (gs->frame >= 7) gs->animDir = -1;
                else if (gs->frame <= 0) { 
                    gs->animDir = 1; 
                    gs->attr ^= SPRITE_ATTR_FLIPX; 
                }
                gs->animTimer = 0;
            }
        } else {
            // Norotate Logic
            gs->animTimer++;
            if (gs->animTimer >= GET_TICKS(NOROT_NEXTFRAME)) {
                gs->frame = (gs->frame + 1) & 3; 
                gs->animTimer = 0;
            }
            gs->stateTimer++;
            if (gs->stateTimer >= GET_TICKS(NOROT_NETXANIM)) {
                gs->animation = (gs->animation == 0) ? 1 : 0;
                gs->stateTimer = 0;
            }
        }

        SPR_setAnim(gs->vdpSprite, gs->animation);
        SPR_setFrame(gs->vdpSprite, gs->frame);
        SPR_setHFlip(gs->vdpSprite, (gs->attr & SPRITE_ATTR_FLIPX));
        SPR_setVFlip(gs->vdpSprite, (gs->attr & SPRITE_ATTR_FLIPY));
        SPR_setPosition(gs->vdpSprite, gs->x, gs->y);
    }
    SPR_update();
}

void sprites_set_visible(u8 index, bool visible) {
    if (index < 10) {
        if (visible) gameSprites[index].attr |= SPRITE_ATTR_VISIBLE;
        else gameSprites[index].attr &= ~SPRITE_ATTR_VISIBLE;
    }
}