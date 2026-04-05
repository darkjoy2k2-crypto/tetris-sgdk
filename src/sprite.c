#include "sprite.h"
#include "sprites.h"
#include <string.h>
#include "states/game/game_core.h"
#include "states/states.h"

// Global switch to disable all random particle start delays.
// FALSE keeps original effect timing behavior.
static const bool g_disableRandomParticleDelays = FALSE;

// Definition des Arrays (Gr????e muss mit Header ??bereinstimmen)
GameSprite gameSprites[4];

// Dust-Partikel Slots
DustParticle dustParticles[DUST_SLOT_COUNT];

typedef struct TitleTextSprite {
    Sprite* vdpSprite;
    const SpriteDefinition* def;
    u8 frame;
    s16 x;
    s16 y;
    bool visible;
} TitleTextSprite;

static TitleTextSprite titleTextSprites[TITLE_TEXT_MAX_CHARS];
static u8 titleTextLength = 0;
static bool titleTextEnabled = FALSE;

#define TITLE_TEXT_SCREEN_W      320
#define TITLE_TEXT_SCREEN_H_NTSC 224
#define TITLE_TEXT_SCREEN_H_PAL  240

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

static s16 _title_text_glyph_extent(const SpriteDefinition* def) {
    if (def == &alpha_16) return 16;
    if (def == &alpha_48) return 48;
    return 32;
}

static bool _title_text_is_on_screen(const TitleTextSprite* ts) {
    s16 extent;
    s16 screenH;

    if (ts == NULL || ts->def == NULL) return FALSE;

    extent = _title_text_glyph_extent(ts->def);
    screenH = (s16)(IS_PAL_SYSTEM ? TITLE_TEXT_SCREEN_H_PAL : TITLE_TEXT_SCREEN_H_NTSC);

    if (ts->x <= -extent || ts->x >= TITLE_TEXT_SCREEN_W) return FALSE;
    if (ts->y <= -extent || ts->y >= screenH) return FALSE;

    return TRUE;
}

static bool _map_char_to_glyph(char c, const SpriteDefinition** def, u8* frame) {
    switch (c) {
        case 'I': case 'i': *def = &alpha_16; *frame = 0; return TRUE;
        case '!': *def = &alpha_16; *frame = 1; return TRUE;
        case '.': *def = &alpha_16; *frame = 2; return TRUE;
        case ';': *def = &alpha_16; *frame = 2; return TRUE;

        case 'M': case 'm': *def = &alpha_48; *frame = 0; return TRUE;
        case 'W': case 'w': *def = &alpha_48; *frame = 1; return TRUE;

        case 'A': case 'a': *def = &alpha_32; *frame = 0; return TRUE;
        case 'B': case 'b': *def = &alpha_32; *frame = 1; return TRUE;
        case 'C': case 'c': *def = &alpha_32; *frame = 2; return TRUE;
        case 'D': case 'd': *def = &alpha_32; *frame = 3; return TRUE;
        case 'E': case 'e': *def = &alpha_32; *frame = 4; return TRUE;
        case 'F': case 'f': *def = &alpha_32; *frame = 5; return TRUE;
        case 'G': case 'g': *def = &alpha_32; *frame = 6; return TRUE;
        case 'H': case 'h': *def = &alpha_32; *frame = 7; return TRUE;
        case 'J': case 'j': *def = &alpha_32; *frame = 8; return TRUE;
        case 'K': case 'k': *def = &alpha_32; *frame = 9; return TRUE;
        case 'L': case 'l': *def = &alpha_32; *frame = 10; return TRUE;
        case 'N': case 'n': *def = &alpha_32; *frame = 11; return TRUE;
        case 'O': case 'o': *def = &alpha_32; *frame = 12; return TRUE;
        case 'P': case 'p': *def = &alpha_32; *frame = 13; return TRUE;
        case 'Q': case 'q': *def = &alpha_32; *frame = 14; return TRUE;
        case 'R': case 'r': *def = &alpha_32; *frame = 15; return TRUE;
        case 'S': case 's': *def = &alpha_32; *frame = 16; return TRUE;
        case 'T': case 't': *def = &alpha_32; *frame = 17; return TRUE;
        case 'U': case 'u': *def = &alpha_32; *frame = 18; return TRUE;
        case 'V': case 'v': *def = &alpha_32; *frame = 19; return TRUE;
        case 'X': case 'x': *def = &alpha_32; *frame = 20; return TRUE;
        case 'Y': case 'y': *def = &alpha_32; *frame = 21; return TRUE;
        case 'Z': case 'z': *def = &alpha_32; *frame = 22; return TRUE;
        case '1': *def = &alpha_32; *frame = 23; return TRUE;
        case '2': *def = &alpha_32; *frame = 24; return TRUE;
        case '3': *def = &alpha_32; *frame = 25; return TRUE;
        case '4': *def = &alpha_32; *frame = 26; return TRUE;
        case '5': *def = &alpha_32; *frame = 27; return TRUE;
        case '6': *def = &alpha_32; *frame = 28; return TRUE;
        case '7': *def = &alpha_32; *frame = 29; return TRUE;
        case '8': *def = &alpha_32; *frame = 30; return TRUE;
        case '9': *def = &alpha_32; *frame = 31; return TRUE;
        case '0': *def = &alpha_32; *frame = 32; return TRUE;
        default: return FALSE;
    }
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
    
    // Initial-Offsets f??r die Zentrierung auf den Tetromino-Bl??cken
    gameSprites[INDEX_PIECE].offsetX  = -8;  gameSprites[INDEX_PIECE].offsetY  = -8;
    gameSprites[INDEX_SHADOW].offsetX = -8;  gameSprites[INDEX_SHADOW].offsetY = 0;
    gameSprites[INDEX_NEXT].offsetX   = 0; gameSprites[INDEX_NEXT].offsetY   = 8;
    gameSprites[INDEX_HOLD].offsetX   = 0; gameSprites[INDEX_HOLD].offsetY   = -8;

    /* Gemeinsamer Glyph-Pool hat Vorrang vor optionalen Partikel-Slots. */
    for (u8 i = 0; i < TITLE_TEXT_MAX_CHARS; i++) {
        memset(&titleTextSprites[i], 0, sizeof(TitleTextSprite));
        titleTextSprites[i].vdpSprite = SPR_addSprite(&alpha_32, -128, -128, TILE_ATTR(PAL2, 0, 0, 0));
        titleTextSprites[i].def = &alpha_32;
        titleTextSprites[i].frame = 0;
        titleTextSprites[i].visible = FALSE;

        if (titleTextSprites[i].vdpSprite != NULL) {
            SPR_setPriority(titleTextSprites[i].vdpSprite, PRIO_HIGH);
            SPR_setDepth(titleTextSprites[i].vdpSprite, DEPTH_FOREGROUND);
        }
    }

    // Dust-Partikel-Sprites initialisieren (nachrangig hinter Text-Glyphen)
    for (u8 di = 0; di < DUST_SLOT_COUNT; di++) {
        memset(&dustParticles[di], 0, sizeof(DustParticle));
        dustParticles[di].vdpSprite = SPR_addSprite(&anim_dust, -128, -128, TILE_ATTR(PAL2, 0, 0, 0));
        dustParticles[di].active = FALSE;
        dustParticles[di].frameTickLimit = DUST_FRAME_TICKS;
        dustParticles[di].kind = PARTICLE_KIND_DUST;
        dustParticles[di].clipMinX = (s16)DUST_BOARD_MIN_X;
        dustParticles[di].clipMaxX = (s16)DUST_BOARD_MAX_X;
        if (dustParticles[di].vdpSprite != NULL) {
            SPR_setPriority(dustParticles[di].vdpSprite, PRIO_LOW);
            SPR_setDepth(dustParticles[di].vdpSprite, DEPTH_DEFAULT);
        }
    }

    titleTextLength = 0;
    titleTextEnabled = FALSE;
}

void sprites_init_text_only() {
    if (SPR_isInitialized()) {
        SPR_end();
    }
    SPR_init();

    memset(gameSprites, 0, sizeof(gameSprites));
    memset(dustParticles, 0, sizeof(dustParticles));

    for (u8 i = 0; i < TITLE_TEXT_MAX_CHARS; i++) {
        memset(&titleTextSprites[i], 0, sizeof(TitleTextSprite));
        titleTextSprites[i].vdpSprite = SPR_addSprite(&alpha_32, -128, -128, TILE_ATTR(PAL2, 0, 0, 0));
        titleTextSprites[i].def = &alpha_32;
        titleTextSprites[i].frame = 0;
        titleTextSprites[i].visible = FALSE;

        if (titleTextSprites[i].vdpSprite != NULL) {
            SPR_setPriority(titleTextSprites[i].vdpSprite, PRIO_HIGH);
            SPR_setDepth(titleTextSprites[i].vdpSprite, DEPTH_FOREGROUND);
        }
    }

    titleTextLength = 0;
    titleTextEnabled = FALSE;
}

/**
 * Hilfsfunktion zum Ressourcenwechsel ohne unn??tige VDP-Uploads.
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
 * Interne Hilfsfunktion zur Zustandssteuerung ohne Koordinaten-??bergabe
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

    // Sonderkorrektur f??r L/J-Winkel in Grundposition (Rotation 0)
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
    dp->clipMinX = (s16)DUST_BOARD_MIN_X;
    dp->clipMaxX = (s16)DUST_BOARD_MAX_X;

    if (dp->vdpSprite != NULL) {
        SPR_setDefinition(dp->vdpSprite, &anim_dust);
    }
}
void sprites_trigger_dust_at_board_origin(s16 boardOriginX, s16 boardOriginY, s16 pieceX, s16 ghostY, bool riseUp) {
    DustParticle* dp = _acquire_free_particle_slot();
    if (dp == NULL) return;

    dp->active = TRUE;
    dp->frame  = 0;
    dp->frameTick = 0;
    dp->frameTickLimit = DUST_FRAME_TICKS;
    dp->startDelay = 0;
    dp->kind = PARTICLE_KIND_DUST;
    dp->risePerFrame = riseUp ? DUST_RISE_DROP : 0;
    dp->startX = (s16)((boardOriginX + pieceX) << 3);
    dp->startY = (s16)(((boardOriginY + ghostY) << 3) - DUST_OFFSET);
    dp->clipMinX = (s16)((boardOriginX << 3) + DUST_OFFSET);
    dp->clipMaxX = (s16)(((boardOriginX + BOARD_WIDTH) << 3) - DUST_OFFSET);

    if (dp->vdpSprite != NULL) {
        SPR_setDefinition(dp->vdpSprite, &anim_dust);
    }
}

void sprites_trigger_line_clear_explosions_at_origin(u32 clearingLineMask, s16 boardOriginX, s16 boardOriginY) {
    u8 clearedLines[BOARD_HEIGHT];
    u8 clearedCount = 0;
    u8 freeSlots;
    u8 explosionTarget;
    const s16 boardLeftPx = (boardOriginX << 3);
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
        s16 baseY = ((boardOriginY + lineY) << 3) + 4;

        dp->active = TRUE;
        dp->kind = PARTICLE_KIND_EXPLOSION;
        dp->frame = 0;
        dp->frameTick = 0;
        dp->frameTickLimit = EXPLOSION_FRAME_TICKS;
    if (g_disableRandomParticleDelays) {
        dp->startDelay = 0;
    } else {
        dp->startDelay = (u8)(random() % (EXPLOSION_DELAY_MAX + 1));
    }
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

void sprites_trigger_line_clear_explosions(u32 clearingLineMask) {
    sprites_trigger_line_clear_explosions_at_origin(clearingLineMask, RENDER_X, RENDER_Y);
}

void sprites_trigger_explosion_at_board_cell_at_origin(u16 boardX, u16 boardY, u8 delayMax, s16 boardOriginX, s16 boardOriginY) {
    DustParticle* dp = _acquire_free_particle_slot();
    if (dp == NULL) return;

    dp->active = TRUE;
    dp->kind = PARTICLE_KIND_EXPLOSION;
    dp->frame = 0;
    dp->frameTick = 0;
    dp->frameTickLimit = EXPLOSION_FRAME_TICKS;
    if (g_disableRandomParticleDelays) {
        dp->startDelay = 0;
    } else {
        dp->startDelay = (delayMax > 0) ? (u8)(random() % (delayMax + 1)) : 0;
    }
    dp->risePerFrame = 0;
    dp->startX = (s16)(((boardOriginX + (s16)boardX) << 3) + 4 + _rand_jitter(EXPLOSION_JITTER));
    dp->startY = (s16)(((boardOriginY + (s16)boardY) << 3) + 4 + _rand_jitter(EXPLOSION_JITTER));

    if (dp->vdpSprite != NULL) {
        SPR_setDefinition(dp->vdpSprite, &anim_explosion);
        SPR_setPriority(dp->vdpSprite, PRIO_HIGH);
        SPR_setDepth(dp->vdpSprite, DEPTH_DEFAULT);
    }
}

void sprites_trigger_explosion_at_board_cell(u16 boardX, u16 boardY, u8 delayMax) {
    sprites_trigger_explosion_at_board_cell_at_origin(boardX, boardY, delayMax, RENDER_X, RENDER_Y);
}

void sprites_sync_game(Vect2D_s16 piecePos, Vect2D_s16 shadowPos, u8 activeEffect) {
    if (sctx == NULL) return;
    Vect2D_s16 center = _get_center_offset(sctx->game.type, sctx->game.rotation);
    
    // X nach rechts (+), Y nach oben (-)
    // Wir nehmen das mathematische Zentrum und verschieben das Sprite-Mapping
    s16 dynOffsetX = center.x - 16;  // X korrigiert nach rechts
    s16 dynOffsetY = center.y - 16; // Y Feinjustierung (etwas h??her als zuvor)

    // 2. POSITIONIERUNG & OFFSETS ??BERNEHMEN
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

void sprites_sync_vs_effect(const GameContext* player, s16 boardOriginX, s16 boardOriginY) {
    if (player == NULL) {
        _setup_sprite(INDEX_PIECE, 0, 0, 0, FALSE);
        _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
        _setup_sprite(INDEX_NEXT, 0, 0, 0, FALSE);
        _setup_sprite(INDEX_HOLD, 0, 0, 0, FALSE);
        return;
    }

    {
        Vect2D_s16 center = _get_center_offset(player->type, player->rotation);
        s16 dynOffsetX = center.x - 16;
        s16 dynOffsetY = center.y - 16;

        gameSprites[INDEX_PIECE].x = (s16)((boardOriginX + player->pieceX) << 3);
        gameSprites[INDEX_PIECE].y = (s16)((boardOriginY + player->pieceY) << 3);
        gameSprites[INDEX_PIECE].offsetX = dynOffsetX;
        gameSprites[INDEX_PIECE].offsetY = dynOffsetY;
    }

    _setup_sprite(INDEX_SHADOW, 0, 0, 0, FALSE);
    _setup_sprite(INDEX_NEXT, 0, 0, 0, FALSE);
    _setup_sprite(INDEX_HOLD, 0, 0, 0, FALSE);

    switch (player->activeBadEffect) {
        case EFFECT_NO_ROTATE:
            _setup_sprite(INDEX_PIECE, SPRITE_TYPE_NOROTATE, PRIO_HIGH, DEPTH_FOREGROUND, TRUE);
            break;
        case EFFECT_REVERSED:
            _setup_sprite(INDEX_PIECE, SPRITE_TYPE_SPIRAL, PRIO_LOW, 10, TRUE);
            break;
        case EFFECT_FULLSPEED:
            _setup_sprite(INDEX_PIECE, SPRITE_TYPE_SPEED, PRIO_LOW, DEPTH_BACKGROUND, TRUE);
            break;
        default:
            _setup_sprite(INDEX_PIECE, 0, 0, 0, FALSE);
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

            if (dustX < dp->clipMinX) dustX = dp->clipMinX;
            if (dustX > dp->clipMaxX) dustX = dp->clipMaxX;

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

    for (u8 i = 0; i < TITLE_TEXT_MAX_CHARS; i++) {
        TitleTextSprite* ts = &titleTextSprites[i];
        if (ts->vdpSprite == NULL) continue;

        if (!titleTextEnabled || !ts->visible || i >= titleTextLength || !_title_text_is_on_screen(ts)) {
            SPR_setPosition(ts->vdpSprite, -128, -128);
            continue;
        }

        SPR_setAnim(ts->vdpSprite, 0);
        SPR_setFrame(ts->vdpSprite, ts->frame);
        SPR_setPosition(ts->vdpSprite, ts->x, ts->y);
    }

    SPR_update();
}

/**
 * Schaltet Sichtbarkeit manuell (f??r UI/Spezialeffekte).
 */
void sprites_set_visible(u8 index, bool visible) {
    if (index < 4) {
        if (visible) gameSprites[index].attr |= SPRITE_ATTR_VISIBLE;
        else gameSprites[index].attr &= ~SPRITE_ATTR_VISIBLE;
    }
}

void sprites_text_set_enabled(bool enabled) {
    titleTextEnabled = enabled;
}

void sprites_text_set_string(const char* text) {
    u8 inPos = 0;
    u8 outPos = 0;

    while (outPos < TITLE_TEXT_MAX_CHARS && text != NULL && text[inPos] != '\0') {
        const SpriteDefinition* def;
        u8 frame;

        if (_map_char_to_glyph(text[inPos], &def, &frame)) {
            if (titleTextSprites[outPos].vdpSprite != NULL) {
                SPR_setDefinition(titleTextSprites[outPos].vdpSprite, def);
                titleTextSprites[outPos].def = def;
            }

            titleTextSprites[outPos].frame = frame;
            titleTextSprites[outPos].visible = TRUE;
            outPos++;
        }

        inPos++;
    }

    titleTextLength = outPos;

    for (; outPos < TITLE_TEXT_MAX_CHARS; outPos++) {
        titleTextSprites[outPos].visible = FALSE;
    }
}

void sprites_text_set_position(u8 index, s16 x, s16 y) {
    if (index >= TITLE_TEXT_MAX_CHARS) return;

    titleTextSprites[index].x = x;
    titleTextSprites[index].y = y;
}

void sprites_text_set_glyph(u8 index, char c, s16 x, s16 y, bool priority, u8 depth, bool visible) {
    const SpriteDefinition* def;
    u8 frame;

    if (index >= TITLE_TEXT_MAX_CHARS) return;

    if (!visible || !_map_char_to_glyph(c, &def, &frame)) {
        titleTextSprites[index].visible = FALSE;
        return;
    }

    if (titleTextSprites[index].vdpSprite != NULL) {
        SPR_setDefinition(titleTextSprites[index].vdpSprite, def);
        SPR_setPriority(titleTextSprites[index].vdpSprite, priority);
        SPR_setDepth(titleTextSprites[index].vdpSprite, depth);
        titleTextSprites[index].def = def;
    }

    titleTextSprites[index].frame = frame;
    titleTextSprites[index].x = x;
    titleTextSprites[index].y = y;
    titleTextSprites[index].visible = TRUE;

    if ((u8)(index + 1) > titleTextLength) {
        titleTextLength = (u8)(index + 1);
    }
}

void sprites_text_clear(void) {
    for (u8 i = 0; i < TITLE_TEXT_MAX_CHARS; i++) {
        titleTextSprites[i].visible = FALSE;
        titleTextSprites[i].x = -128;
        titleTextSprites[i].y = -128;

        if (titleTextSprites[i].vdpSprite != NULL) {
            SPR_setPosition(titleTextSprites[i].vdpSprite, -128, -128);
        }
    }
    titleTextLength = 0;
}

u8 sprites_text_get_length(void) {
    return titleTextLength;
}

void sprites_cleanup() {
    titleTextEnabled = FALSE;
    titleTextLength = 0;

    for (u8 i = 0; i < TITLE_TEXT_MAX_CHARS; i++) {
        titleTextSprites[i].visible = FALSE;
        titleTextSprites[i].x = -128;
        titleTextSprites[i].y = -128;

        if (titleTextSprites[i].vdpSprite != NULL) {
            SPR_setPosition(titleTextSprites[i].vdpSprite, -128, -128);
        }
    }

    SPR_reset();
}
