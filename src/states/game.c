#include <genesis.h>
#include "states/game/game_core.h"
#include "states/game/game_logic.h"
#include "states/game/game_view.h"
#include "states/game/game_controls.h" // NEU

#include "gfx.h"
#include "sound_manager.h"
#include "states/states.h"
#include "menu_bg.h"
#include "fonts.h"
#include "bg.h"
#include "sprite.h"
#include "sprites.h"

#include <string.h>

GameContext* ctx = NULL;

const u16 GRAVITY_SPEEDS[] = { 9999, 60, 30, 15 };
const u16 GARBAGE_INTERVALS[] = { 0, 1200, 600, 300 };

// Ganz am Ende von game_logic.c einfügen:






static bool handle_gravity(GameContext* ctx) {
    u16 vBtnSoftDrop = (ctx->activeBadEffect == EFFECT_REVERSED) ? BUTTON_LEFT : BUTTON_DOWN;
    bool moved = false;
    ctx->moveTimer++;
    s16 threshold = GET_TICKS(60 - ((ctx->level - 1) * 3));
    if (threshold < 2) threshold = 3;
    if (ctx->activeBadEffect == EFFECT_FULLSPEED) threshold = 3;
    u16 finalThreshold = (joyState & vBtnSoftDrop) ? (threshold / 12) : 
                        ((ctx->activeBadEffect == EFFECT_FREEZE) ? 9999 : threshold);
    if (finalThreshold < 4 && (joyState & vBtnSoftDrop)) finalThreshold = 4;
    if (ctx->moveTimer >= finalThreshold) {
        if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
            moved = true;
            if (joyState & vBtnSoftDrop) { ctx->score++; SOUND_play(SND_SOFT_DROP); }
        } else {
            lockPiece();
            moved = true;
        }
        ctx->moveTimer = 0;
    }
    return moved;
}

static void handle_environment(GameContext* ctx, bool moved) {
    // Garbage
if (ctx->garbageTimer >= ctx->garbageNextThreshold) {
        addGarbageLine();
        ctx->garbageTimer = 0;
        ctx->garbageNextThreshold = GET_TICKS(GARBAGE_INTERVALS[config.garbageFreq] + (random() % 120) - 60);
        ctx->boardFlags |= GF_NEEDS_DRAW;
    }


    // Shadow
if (moved && GET_FLAG(config.flags, FLAG_SHADOW)) {
                ctx->ghostY = ctx->pieceY;
        while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation)) ctx->ghostY++;
    }

    // Effect Timer
    if (ctx->badEffectTimer > 0 && ctx->activeBadEffect >= 3) {
        ctx->badEffectTimer--;
        if (ctx->badEffectTimer == 0) {
            ctx->activeBadEffect = EFFECT_NONE;
            SOUND_play(SND_GOOD_ITEM);
        }
    }
}

void update_sprite_position(){
    // Tetromino
    gameSprites[0].x = ((RENDER_X + ctx->pieceX) << 3) + gameSprites[0].offsetX; 
    gameSprites[0].y = ((RENDER_Y + ctx->pieceY) << 3) + gameSprites[0].offsetY;

    // Schatten
    gameSprites[1].x = ((RENDER_X + ctx->pieceX) << 3) + gameSprites[1].offsetX;
    gameSprites[1].y = ((RENDER_Y + ctx->ghostY) << 3) + gameSprites[1].offsetY;

    // UI Elemente (Next/Hold) erhalten ebenfalls ihre Offsets (-8)
    gameSprites[2].x = (UI_X << 3) + gameSprites[2].offsetX; 
    gameSprites[2].y = (NEXT_Y << 3) + gameSprites[2].offsetY;

    gameSprites[3].x = (UI_X << 3) + gameSprites[3].offsetX; 
    gameSprites[3].y = (HOLD_Y << 3) + gameSprites[3].offsetY;
}





// --- REINE LOGIK INITIALISIERUNG ---
void game_init() {
    // 1. Speicher-Lifecycle
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    ctx = MEM_alloc(sizeof(GameContext));
    
    // 2. Logik-Reset aufrufen
    reset_game_logic();

    // 3. Hardware & VDP Setup
    PAL_setPalette(PAL2, anim_norotate.palette->data, DMA);
    UI_init_fonts_and_palettes();
    SOUND_init();
    
    menu_bg_set_mode(BG_MODE_SPACE);
    menu_bg_set_active(true);

    // 4. Initiale Sprite-Berechnung
    update_sprite_position();
}

void update_curse_sprites(GameContext* ctx) {
    bool norotActive  = (ctx->activeBadEffect == EFFECT_NO_ROTATE);
    bool skullActive  = (ctx->activeBadEffect == EFFECT_FULLSPEED || ctx->activeBadEffect == EFFECT_SAME_TILES);
    bool holdLocked   = (ctx->activeBadEffect == EFFECT_HOLD_LOCK);
    bool nextHidden   = (ctx->activeBadEffect == EFFECT_HIDE_NEXT);
    bool shadowOn     = GET_FLAG(config.flags, FLAG_SHADOW);

    // --- Slot 0: Tetromino Position ---
    GameSprite* s0 = &gameSprites[0];
    if (norotActive) {
        s0->type = SPRITE_TYPE_NOROTATE;
        SPR_setDefinition(s0->vdpSprite, &anim_norotate);
        s0->attr &= ~SPRITE_ATTR_PRIORITY; // Über das Piece
        sprites_set_visible(0, true);
    } else if (skullActive) {
        s0->type = SPRITE_TYPE_SKULL;
        SPR_setDefinition(s0->vdpSprite, &anim_skull);
        s0->attr |= SPRITE_ATTR_PRIORITY;  // Hinter das Piece (Low Prio)
        sprites_set_visible(0, true);
    } else {
        sprites_set_visible(0, false);
    }
    s0->x = ((RENDER_X + ctx->pieceX) << 3) + s0->offsetX;
    s0->y = ((RENDER_Y + ctx->pieceY) << 3) + s0->offsetY;

    // --- Slot 1: Schatten Position ---
    // Norotate laut Vorgabe NICHT auf den Schatten abbilden
    sprites_set_visible(1, false); 

    // --- Slot 2: Next Platz ---
    GameSprite* s2 = &gameSprites[2];
    if (nextHidden) {
        s2->type = SPRITE_TYPE_SKULL;
        SPR_setDefinition(s2->vdpSprite, &anim_skull);
        s2->attr &= ~SPRITE_ATTR_PRIORITY;
        sprites_set_visible(2, true);
    } else {
        sprites_set_visible(2, false);
    }
    s2->x = (UI_X << 3) + s2->offsetX;
    s2->y = (NEXT_Y << 3) + s2->offsetY;

    // --- Slot 3: Hold Platz ---
    GameSprite* s3 = &gameSprites[3];
    if (holdLocked) {
        s3->type = SPRITE_TYPE_SKULL;
        SPR_setDefinition(s3->vdpSprite, &anim_skull);
        s3->attr &= ~SPRITE_ATTR_PRIORITY;
        sprites_set_visible(3, true);
    } else {
        sprites_set_visible(3, false);
    }
    s3->x = (UI_X << 3) + s3->offsetX;
    s3->y = (HOLD_Y << 3) + s3->offsetY;
}



void game_init_draw() {
    // Sicherheitscheck: Ohne Context kein Zeichnen
    if (ctx == NULL) return;

    // 1. VRAM Säuberung
    VDP_clearPlane(BG_A, TRUE);

    // 2. Grafik-Ressourcen laden
    load_background(); 

    // 3. Tile-Cache Initialisierung
    view_init_cache(); 
   // PAL_setPalette(PAL0, anim_norotate.palette->data, DMA); //SPRITEPALETTE
    sprites_init();
        // 4. Paletten-Setup für UI und Text
    //PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    //VDP_setTextPalette(PAL3);
    UI_init_fonts_and_palettes(); // Setzt PAL1, PAL2, PAL3 und Font

    // 5. Visueller Start-Effekt
    view_fade_in_frame();  

}

void game_update() {
    if (ctx == NULL) return;

    if (handle_active_animations(ctx)) {
        lastJoyState = joyState;
        update_curse_sprites(ctx);
        return;
    }

    bool moved = controls_update(ctx);

    update_curse_sprites(ctx);
    if (handle_gravity(ctx)) moved = true;

    handle_environment(ctx, moved);

    if (moved) ctx->boardFlags |= GF_NEEDS_DRAW;

    lastJoyState = joyState; 
}

void game_draw() {
    if (ctx == NULL) return;

    // Nutzt das Bit-Flag GF_NEEDS_DRAW aus dem u32 boardFlags Member
    if (ctx->boardFlags & GF_NEEDS_DRAW) {
        drawBoard(); 
        
        // Debug Bag Anzeige aufrufen
        view_draw_debug_bag(ctx);
        
        // Bit-Flag löschen (Reset)
        ctx->boardFlags &= ~GF_NEEDS_DRAW; 
    }
    
    sprites_update();
    view_update_ui(ctx); 
}

void game_cleanup() {
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearPlane(BG_A, TRUE);
}