#include "sprite.h"
#include "sprites.h"
#include <string.h>
#include "states/game/game_core.h"

// Definition des Arrays (Größe muss mit Header übereinstimmen)
GameSprite gameSprites[4]; 

/**
 * Initialisiert die Sprite-Engine und die 4 Basis-Sprites.
 */
void sprites_init() {
    if (SPR_isInitialized()) {
        SPR_end(); 
    }
    SPR_init();

    for(u16 i = 0; i < 4; i++) {
        memset(&gameSprites[i], 0, sizeof(GameSprite));
        
        // Initial als No-Rotate laden, wird dynamisch via SPR_setDefinition gewechselt
        gameSprites[i].vdpSprite = SPR_addSprite(&anim_norotate, -128, -128, TILE_ATTR(PAL2, 0, 0, 0));
        
        if (gameSprites[i].vdpSprite == NULL) {
            continue; 
        }
        
        gameSprites[i].type = SPRITE_TYPE_NOROTATE;
        gameSprites[i].animDir = 1;
    }
    
    // Initial-Offsets für die Zentrierung auf den Tetromino-Blöcken
    gameSprites[INDEX_PIECE].offsetX  = -8;  gameSprites[INDEX_PIECE].offsetY  = -8;
    gameSprites[INDEX_SHADOW].offsetX = -8;  gameSprites[INDEX_SHADOW].offsetY = 0;
    gameSprites[INDEX_NEXT].offsetX   = 0; gameSprites[INDEX_NEXT].offsetY   = 8;
    gameSprites[INDEX_HOLD].offsetX   = 0; gameSprites[INDEX_HOLD].offsetY   = -8;
}

/**
 * Hilfsfunktion zum Ressourcenwechsel ohne unnötige VDP-Uploads.
 */
static void _update_sprite_resource(GameSprite* gs, u8 targetType) {
    if (gs->type == targetType) return;
    
    gs->type = targetType;
    if (targetType == SPRITE_TYPE_SKULL) {
        SPR_setDefinition(gs->vdpSprite, &anim_skull);
    } else if (targetType == SPRITE_TYPE_SPIRAL) {
        // NEU: Spirale laden
        SPR_setDefinition(gs->vdpSprite, &anim_spiral);
    } else {
        SPR_setDefinition(gs->vdpSprite, &anim_norotate);
    }
}

/**
 * Synchronisiert die Sprites mit der Spiellogik.
 * Setzt Grafik, Priorität und Tiefe basierend auf dem aktiven Effekt.
 */
/**
 * Interne Hilfsfunktion zur Zustandssteuerung ohne Koordinaten-Übergabe
 */
static void _setup_sprite(u8 idx, u8 type, bool prio, u8 depth, bool visible) {
    GameSprite* gs = &gameSprites[idx];
    if (visible) gs->attr |= SPRITE_ATTR_VISIBLE; else gs->attr &= ~SPRITE_ATTR_VISIBLE;
    
    if (visible) {
        _update_sprite_resource(gs, type);
        SPR_setPriority(gs->vdpSprite, prio);
        SPR_setDepth(gs->vdpSprite, depth);
    }
}

void sprites_sync_game(Vect2D_s16 piecePos, Vect2D_s16 shadowPos, u8 activeEffect) {
    if (ctx == NULL) return;

// 1. RELATIVER VERSATZ (Innerhalb des 4x4 Gitters)
    // Wir holen die Grid-Koordinaten des Item-Slots
    s16 gridX = PIECES[ctx->type][ctx->rotation][ctx->itemSlot][0];
    s16 gridY = PIECES[ctx->type][ctx->rotation][ctx->itemSlot][1];

    // Umrechnung in Pixel (1 Tile = 8 Pixel)
    s16 bx = gridX << 3;
    s16 by = gridY << 3;

    // 2. ABSOLUTE POSITIONIERUNG
    // piecePos ist bereits (RENDER_X + pieceX) * 8
    gameSprites[INDEX_PIECE].x  = piecePos.x;
    gameSprites[INDEX_PIECE].y  = piecePos.y;

    gameSprites[INDEX_SHADOW].x = shadowPos.x ;
    gameSprites[INDEX_SHADOW].y = shadowPos.y;




    // UI-Sprites (Bitshift << 3 für Tile -> Pixel Umrechnung)
    gameSprites[INDEX_NEXT].x = UI_X_NEXT << 3; 
    gameSprites[INDEX_NEXT].y = UI_Y_NEXT << 3;

    gameSprites[INDEX_HOLD].x = UI_X_HOLD << 3;  
    gameSprites[INDEX_HOLD].y = UI_Y_HOLD << 3;

    // 2. Zustands-Konfiguration aller Slots
    switch (activeEffect) {
        case EFFECT_NO_ROTATE:
            _setup_sprite(INDEX_PIECE,  SPRITE_TYPE_NOROTATE, PRIO_HIGH, DEPTH_FOREGROUND, TRUE);
            _setup_sprite(INDEX_SHADOW,   0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT,   0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD,   0, 0, 0, FALSE);
            break;


        case EFFECT_REVERSED:
            _setup_sprite(INDEX_PIECE,  SPRITE_TYPE_SPIRAL, PRIO_LOW,  DEPTH_BACKGROUND, TRUE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT,   0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD,   0, 0, 0, FALSE);
            break;
            break;

        case EFFECT_HIDE_NEXT:
            _setup_sprite(INDEX_PIECE,  0, 0, 0, FALSE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT,   SPRITE_TYPE_SKULL, PRIO_HIGH, DEPTH_FOREGROUND, TRUE);
            _setup_sprite(INDEX_HOLD,   0, 0, 0, FALSE);
            break;

        case EFFECT_HOLD_LOCK:
            _setup_sprite(INDEX_PIECE,  0, 0, 0, FALSE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT,   0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD,   SPRITE_TYPE_SKULL, PRIO_HIGH, DEPTH_FOREGROUND, TRUE);
            break;
        case EFFECT_FULLSPEED:
        case EFFECT_SAME_TILES:
        default: // EFFECT_NONE oder Effekte ohne Sprite-Bedarf
            _setup_sprite(INDEX_PIECE,  0, 0, 0, FALSE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT,   0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD,   0, 0, 0, FALSE);
            break;
    }
}



/**
 * Verarbeitet Animationen und Hardware-Updates.
 */
void sprites_update() {
    for(u16 i = 0; i < 4; i++) {
        GameSprite* gs = &gameSprites[i];
        if (gs->vdpSprite == NULL) continue;

        if (!(gs->attr & SPRITE_ATTR_VISIBLE)) {
            SPR_setPosition(gs->vdpSprite, -128, -128); 
            continue;
        }

// Animations-Logik innerhalb der Schleife von sprites_update()
        gs->animTimer++;
        
        if (gs->type == SPRITE_TYPE_SKULL) {
            if (gs->animTimer >= GET_TICKS(SKULL_NEXTFRAME)) {
                gs->frame += gs->animDir;
                if (gs->frame >= 7) gs->animDir = -1;
                else if (gs->frame <= 0) { 
                    gs->animDir = 1; 
                    gs->attr ^= SPRITE_ATTR_FLIPX; 
                }
                gs->animTimer = 0;
            }
        } 
        else if (gs->type == SPRITE_TYPE_SPIRAL) {
            // NEU: Spirale mit 4 Bildern
            if (gs->animTimer >= GET_TICKS(SPIRAL_NEXTFRAME)) {
                gs->frame = (gs->frame + 1) & 3; // Loop durch 0, 1, 2, 3
                gs->animTimer = 0;
            }
        }
        else { // SPRITE_TYPE_NOROTATE
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

        // Hardware-Register schreiben
        SPR_setAnim(gs->vdpSprite, gs->animation);
        SPR_setFrame(gs->vdpSprite, gs->frame);
        SPR_setHFlip(gs->vdpSprite, (gs->attr & SPRITE_ATTR_FLIPX));
        SPR_setPosition(gs->vdpSprite, gs->x + gs->offsetX, gs->y + gs->offsetY);
    }
    SPR_update();
}

/**
 * Schaltet Sichtbarkeit manuell (für UI/Spezialeffekte).
 */
void sprites_set_visible(u8 index, bool visible) {
    if (index < 4) {
        if (visible) gameSprites[index].attr |= SPRITE_ATTR_VISIBLE;
        else gameSprites[index].attr &= ~SPRITE_ATTR_VISIBLE;
    }
}