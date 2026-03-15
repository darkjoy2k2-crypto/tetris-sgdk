#include "sprite.h"
#include "sprites.h"
#include "states/states.h"
#include "gfx.h"

GameSprite gameSprites[10];

void sprites_init() {
    SPR_init();
    for(u16 i = 0; i < 10; i++) {
        memset(&gameSprites[i], 0, sizeof(GameSprite));
        gameSprites[i].vdpSprite = SPR_addSprite(&anim_norotate, -128, -128, TILE_ATTR(PAL2, 0, 0, 0));
        // Alle Sprites starten ohne SPRITE_ATTR_VISIBLE (Bit 0)
    }
    
    // Index 0: Aktives Tetromino
    gameSprites[0].attr = SPRITE_FLAG_TETROMINO;
    gameSprites[0].offsetX = -8;
    gameSprites[0].offsetY = -8;

    // Index 1: Schatten
    gameSprites[1].attr = SPRITE_FLAG_SHADOW;
    gameSprites[1].offsetX = -8;
    gameSprites[1].offsetY = -8;

    // Index 2: Next Piece
    gameSprites[2].attr = SPRITE_FLAG_NEXT;

    // Index 3: Hold Piece
    gameSprites[3].attr = SPRITE_FLAG_HOLD;
}


void sprites_update() {
    for(u16 i = 0; i < 10; i++) {
        GameSprite* gs = &gameSprites[i];
        
        if (!(gs->attr & SPRITE_ATTR_VISIBLE)) {
            SPR_setPosition(gs->vdpSprite, -128, -128); 
            continue;
        }

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