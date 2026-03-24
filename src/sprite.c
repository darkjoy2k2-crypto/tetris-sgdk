#include "sprite.h"
#include "sprites.h"
#include <string.h>
#include "states/game/game_core.h"
#include "states/states.h"

// Definition des Arrays (Größe muss mit Header übereinstimmen)
GameSprite gameSprites[4];

// Dust-Partikel Slots
DustParticle dustParticles[DUST_SLOT_COUNT];

#define PARTICLE_KIND_DUST      0
#define PARTICLE_KIND_EXPLOSION 1

static s16 _rand_jitter(s16 radius) {
    return (s16)(random() % (u16)((radius << 1) + 1)) - radius;
}

static DustParticle* _acquire_free_particle_slot() {
    for (u8 di = 0; di < DUST_SLOT_COUNT; di++) {
        if (!dustParticles[di].active && dustParticles[di].vdpSprite != NULL) {
            return &dustParticles[di];
        }
    }
    return NULL;
}

static u8 _count_free_particle_slots() {
    u8 freeCount = 0;
    for (u8 di = 0; di < DUST_SLOT_COUNT; di++) {
        if (!dustParticles[di].active && dustParticles[di].vdpSprite != NULL) {
            freeCount++;
        }
    }
    return freeCount;
}


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

    // Dust-Partikel-Sprites initialisieren (4 reservierte Slots)
    for (u8 di = 0; di < DUST_SLOT_COUNT; di++) {
        memset(&dustParticles[di], 0, sizeof(DustParticle));
        dustParticles[di].vdpSprite = SPR_addSprite(&anim_dust, -128, -128, TILE_ATTR(PAL2, 0, 0, 0));
        dustParticles[di].active = FALSE;
        dustParticles[di].frameTickLimit = DUST_FRAME_TICKS;
        dustParticles[di].kind = PARTICLE_KIND_DUST;
        if (dustParticles[di].vdpSprite != NULL) {
            SPR_setPriority(dustParticles[di].vdpSprite, PRIO_LOW);
            SPR_setDepth(dustParticles[di].vdpSprite, DEPTH_DEFAULT);
        }
    }
}

/**
 * Hilfsfunktion zum Ressourcenwechsel ohne unnötige VDP-Uploads.
 */
static void _update_sprite_resource(GameSprite* gs, u8 targetType) {
    if (gs->type == targetType) return;
    
    gs->type = targetType;
    gs->frame = 0;
    gs->animation = 0;
    gs->animTimer = 0;
    gs->stateTimer = 0;
    gs->animDir = 1;
    gs->attr &= ~SPRITE_ATTR_FLIPX;

    if (targetType == SPRITE_TYPE_SKULL) {
        SPR_setDefinition(gs->vdpSprite, &anim_skull);
    } else if (targetType == SPRITE_TYPE_SPIRAL) {
        // Spirale laden
        SPR_setDefinition(gs->vdpSprite, &anim_spiral);
    } else if (targetType == SPRITE_TYPE_SPEED) {
        // Geschwindigkeit laden
        SPR_setDefinition(gs->vdpSprite, &anim_speed);
    } else {
        SPR_setDefinition(gs->vdpSprite, &anim_norotate);
    }
}

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

static Vect2D_s16 _get_center_offset(u8 type, u8 rotation) {
    s16 sumX = 0, sumY = 0;
    for (u16 i = 0; i < 4; i++) {
        sumX += PIECES[type][rotation][i][0];
        sumY += PIECES[type][rotation][i][1];
    }

    Vect2D_s16 offset;
    offset.x = (sumX << 1) + 4;
    offset.y = (sumY << 1) + 4;

    // Sonderkorrektur für L/J-Winkel in Grundposition (Rotation 0)
    // Wenn X spiegelverkehrt wirkt, ziehen wir hier die 4 Pixel ab.
    if ((type == 5 || type == 6) && rotation == 0) {
        offset.x -= 4;
    }

    return offset;
}

/**
 * Startet eine Dust-Animation an der angegebenen Pixelposition.
 * Findet den ersten freien Slot; sind alle 4 belegt, wird der Effekt ignoriert.
 */
void sprites_trigger_dust(s16 x, s16 y, bool riseUp) {
    DustParticle* dp = _acquire_free_particle_slot();
    if (dp == NULL) return;

    dp->active = TRUE;
    dp->frame  = 0;
    dp->frameTick = 0;
    dp->frameTickLimit = DUST_FRAME_TICKS;
    dp->startDelay = 0;
    dp->kind = PARTICLE_KIND_DUST;
    dp->risePerFrame = riseUp ? DUST_RISE_DROP : 0;
    dp->startX = x;
    dp->startY = y - DUST_OFFSET;

    if (dp->vdpSprite != NULL) {
        SPR_setDefinition(dp->vdpSprite, &anim_dust);
    }
}

void sprites_trigger_line_clear_explosions(u32 clearingLineMask) {
    u8 clearedLines[BOARD_HEIGHT];
    u8 clearedCount = 0;
    u8 freeSlots;
    u8 explosionTarget;
    const s16 boardLeftPx = (RENDER_X << 3);
    const s16 boardWidthPx = (BOARD_WIDTH << 3);

    for (u8 y = 0; y < BOARD_HEIGHT; y++) {
        if (clearingLineMask & (1UL << y)) {
            clearedLines[clearedCount++] = y;
        }
    }

    if (clearedCount == 0) return;

    freeSlots = _count_free_particle_slots();
    if (freeSlots == 0) return;

    explosionTarget = (u8)(clearedCount * EXPLOSIONS_PER_CLEAR);
    if (explosionTarget > freeSlots) explosionTarget = freeSlots;

    for (u8 i = 0; i < explosionTarget; i++) {
        DustParticle* dp = _acquire_free_particle_slot();
        if (dp == NULL) return;

        u8 lineIndex = (u8)(random() % clearedCount);
        u8 lineY = clearedLines[lineIndex];
        s16 baseX = boardLeftPx + (s16)(((i + 1) * boardWidthPx) / (explosionTarget + 1));
        s16 baseY = ((RENDER_Y + lineY) << 3) + 4;

        dp->active = TRUE;
        dp->kind = PARTICLE_KIND_EXPLOSION;
        dp->frame = 0;
        dp->frameTick = 0;
        dp->frameTickLimit = EXPLOSION_FRAME_TICKS;
        dp->startDelay = (u8)(random() % (EXPLOSION_DELAY_MAX + 1));
        dp->risePerFrame = 0;
        dp->startX = baseX + _rand_jitter(EXPLOSION_JITTER);
        dp->startY = baseY + _rand_jitter(EXPLOSION_JITTER);

        if (dp->vdpSprite != NULL) {
            SPR_setDefinition(dp->vdpSprite, &anim_explosion);
            SPR_setPriority(dp->vdpSprite, PRIO_HIGH);
            SPR_setDepth(dp->vdpSprite, DEPTH_DEFAULT);
        }
    }
}

void sprites_trigger_explosion_at_board_cell(u16 boardX, u16 boardY, u8 delayMax) {
    DustParticle* dp = _acquire_free_particle_slot();
    if (dp == NULL) return;

    dp->active = TRUE;
    dp->kind = PARTICLE_KIND_EXPLOSION;
    dp->frame = 0;
    dp->frameTick = 0;
    dp->frameTickLimit = EXPLOSION_FRAME_TICKS;
    dp->startDelay = (delayMax > 0) ? (u8)(random() % (delayMax + 1)) : 0;
    dp->risePerFrame = 0;
    dp->startX = (s16)(((RENDER_X + boardX) << 3) + 4 + _rand_jitter(EXPLOSION_JITTER));
    dp->startY = (s16)(((RENDER_Y + boardY) << 3) + 4 + _rand_jitter(EXPLOSION_JITTER));

    if (dp->vdpSprite != NULL) {
        SPR_setDefinition(dp->vdpSprite, &anim_explosion);
        SPR_setPriority(dp->vdpSprite, PRIO_HIGH);
        SPR_setDepth(dp->vdpSprite, DEPTH_DEFAULT);
    }
}

void sprites_sync_game(Vect2D_s16 piecePos, Vect2D_s16 shadowPos, u8 activeEffect) {
    if (sctx == NULL) return;

// 1. DYNAMISCHE ZENTRIERUNG BERECHNEN
    Vect2D_s16 center = _get_center_offset(sctx->game.type, sctx->game.rotation);
    
    // X nach rechts (+), Y nach oben (-)
    // Wir nehmen das mathematische Zentrum und verschieben das Sprite-Mapping
    s16 dynOffsetX = center.x - 16;  // X korrigiert nach rechts
    s16 dynOffsetY = center.y - 16; // Y Feinjustierung (etwas höher als zuvor)

    // 2. POSITIONIERUNG & OFFSETS ÜBERNEHMEN
    gameSprites[INDEX_PIECE].x = piecePos.x;
    gameSprites[INDEX_PIECE].y = piecePos.y;
    gameSprites[INDEX_PIECE].offsetX = dynOffsetX;
    gameSprites[INDEX_PIECE].offsetY = dynOffsetY;

    gameSprites[INDEX_SHADOW].x = shadowPos.x;
    gameSprites[INDEX_SHADOW].y = shadowPos.y;
    gameSprites[INDEX_SHADOW].offsetX = dynOffsetX;
    gameSprites[INDEX_SHADOW].offsetY = dynOffsetY;

    gameSprites[INDEX_NEXT].x = UI_X_NEXT << 3; 
    gameSprites[INDEX_NEXT].y = UI_Y_NEXT << 3;
    gameSprites[INDEX_HOLD].x = UI_X_HOLD << 3;  
    gameSprites[INDEX_HOLD].y = UI_Y_HOLD << 3;

    switch (activeEffect) {
        case EFFECT_HIDE_NEXT:
            _setup_sprite(INDEX_PIECE, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT, SPRITE_TYPE_SKULL, 1, 0, TRUE);
            _setup_sprite(INDEX_HOLD, 0, 0, 0, FALSE);
            break;
        case EFFECT_HOLD_LOCK:
            _setup_sprite(INDEX_PIECE, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD, SPRITE_TYPE_SKULL, 1, 0, TRUE);
            break;
        case EFFECT_NO_ROTATE:
            _setup_sprite(INDEX_PIECE, SPRITE_TYPE_NOROTATE, 1, 0, TRUE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD, 0, 0, 0, FALSE);
            break;
        case EFFECT_REVERSED:
            _setup_sprite(INDEX_PIECE, SPRITE_TYPE_SPIRAL, 0, 10, TRUE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD, 0, 0, 0, FALSE);
            break;
        case EFFECT_FULLSPEED:
            _setup_sprite(INDEX_PIECE, SPRITE_TYPE_SPEED, PRIO_LOW, DEPTH_BACKGROUND, TRUE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD, 0, 0, 0, FALSE);
            break;
        default:
            _setup_sprite(INDEX_PIECE, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_NEXT, 0, 0, 0, FALSE);
            _setup_sprite(INDEX_HOLD, 0, 0, 0, FALSE);
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

        // Animations-Logik innerhalb der Schleife
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
            // Spirale mit 4 Bildern
            if (gs->animTimer >= GET_TICKS(SPIRAL_NEXTFRAME)) {
                gs->frame = (gs->frame + 1) & 3; // Loop durch 0, 1, 2, 3
                gs->animTimer = 0;
            }
        }
        else if (gs->type == SPRITE_TYPE_SPEED) {
            if (gs->animTimer >= GET_TICKS(SPEED_NEXTFRAME)) {
                gs->frame = (gs->frame + 1) & 3;
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
        
        // Finaler Positions-Aufruf inkl. dynamischem Offset
        SPR_setPosition(gs->vdpSprite, gs->x + gs->offsetX, gs->y + gs->offsetY);
    }

    // Dust-/Explosion-Partikel (geteilte Slots)
    for (u8 di = 0; di < DUST_SLOT_COUNT; di++) {
        DustParticle* dp = &dustParticles[di];
        if (dp->vdpSprite == NULL) continue;

        if (!dp->active) {
            SPR_setPosition(dp->vdpSprite, -128, -128);
            continue;
        }

        if (dp->startDelay > 0) {
            dp->startDelay--;
            SPR_setPosition(dp->vdpSprite, -128, -128);
            continue;
        }

        SPR_setFrame(dp->vdpSprite, dp->frame);

        if (dp->kind == PARTICLE_KIND_DUST) {
            s16 dustX = dp->startX - DUST_OFFSET;
            s16 dustY = dp->startY - (s16)(dp->frame * dp->risePerFrame);

            if (dustX < DUST_BOARD_MIN_X) dustX = DUST_BOARD_MIN_X;
            if (dustX > DUST_BOARD_MAX_X) dustX = DUST_BOARD_MAX_X;

            if (dustY < DUST_BOARD_MIN_Y) {
                dp->active = FALSE;
                SPR_setPosition(dp->vdpSprite, -128, -128);
                continue;
            }

            SPR_setPosition(dp->vdpSprite, dustX, dustY);
        } else {
            SPR_setPosition(dp->vdpSprite, dp->startX - DUST_OFFSET, dp->startY - DUST_OFFSET);
        }

        dp->frameTick++;
        if (dp->frameTick >= dp->frameTickLimit) {
            dp->frameTick = 0;
            dp->frame++;
            if (dp->kind == PARTICLE_KIND_DUST) {
                if (dp->frame >= DUST_TOTAL_FRAMES) dp->active = FALSE;
            } else {
                if (dp->frame >= EXPLOSION_TOTAL_FRAMES) dp->active = FALSE;
            }

            if (!dp->active) {
                SPR_setPosition(dp->vdpSprite, -128, -128);
                dp->active = FALSE;
            }
        }
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

void sprites_cleanup() {
    SPR_reset();
}