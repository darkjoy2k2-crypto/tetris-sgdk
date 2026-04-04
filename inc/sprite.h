#ifndef _SPRITE_H_
#define _SPRITE_H_

#include <genesis.h>
#include "states/states.h"
#include "states/game/game_core.h" 

// Animations-Geschwindigkeiten (Ticks)
#define NOROT_NEXTFRAME       10
#define NOROT_NETXANIM        60
#define SKULL_NEXTFRAME       2
#define SPIRAL_NEXTFRAME      20
#define SPEED_NEXTFRAME       5
#define EFFECT_DURATION_NOROT GET_TICKS(180)

// Sprite-Verhaltens-Typen
#define SPRITE_TYPE_NOROTATE  0
#define SPRITE_TYPE_SKULL     1
#define SPRITE_TYPE_SPIRAL    2
#define SPRITE_TYPE_SPEED     3
#define SPRITE_TYPE_DUST      4

// Dust-Partikel Konstanten
#define DUST_TOTAL_FRAMES     16   // Gesamtanzahl Animationsframes
#define DUST_FRAME_TICKS      2    // Ticks bis zum nächsten Dust-Frame
#define DUST_RISE_DROP        8    // Pixel Aufstieg pro Frame bei Soft-/Harddrop
#define DUST_OFFSET           8    // X/Y Zentrierung des Dust-Sprites
#define DUST_BASE_SLOTS       8    // Ursprüngliche Dust-Slots (verdoppelt)
#define EXPLOSION_EXTRA_SLOTS 12   // Zusätzliche Slots für Explosionen (verdoppelt, geteilt mit Dust)
#define DUST_SLOT_COUNT       (DUST_BASE_SLOTS + EXPLOSION_EXTRA_SLOTS)
#define EXPLOSIONS_PER_CLEAR  5    // Anzahl Explosionen pro Line-Clear
#define EXPLOSION_FRAME_TICKS 4    // Explosion läuft mit 1 Frame pro Tick
#define EXPLOSION_DELAY_MAX   5    // Startverzögerung 0..5 Frames nach Line-Clear
#define EXPLOSION_JITTER      2    // Minimaler Zufallsversatz in Pixeln (+/-2)
#define EXPLOSION_TOTAL_FRAMES 12  // Explosion-Frames (anim_explosion)
// Board-Grenzen für Dust-Clipping (in Pixeln)
#define DUST_BOARD_MIN_X      ((RENDER_X << 3) + DUST_OFFSET)
#define DUST_BOARD_MAX_X      (((RENDER_X + BOARD_WIDTH) << 3) - DUST_OFFSET)
#define DUST_BOARD_MIN_Y      ((RENDER_Y << 3) - DUST_OFFSET)

// System Attr Flags
#define SPRITE_ATTR_VISIBLE   (1 << 0)
#define SPRITE_ATTR_FLIPX     (1 << 1)
#define SPRITE_ATTR_FLIPY     (1 << 2)
#define SPRITE_ATTR_PRIORITY  (1 << 3)

// --- Sprite Z-Order & Priority ---
#define DEPTH_FOREGROUND     0    
#define DEPTH_DEFAULT        128  
#define DEPTH_BACKGROUND     255  

#define PRIO_LOW             FALSE 
#define PRIO_HIGH            TRUE  

// Sprite-Indizes
#define INDEX_PIECE          0
#define INDEX_SHADOW         1
#define INDEX_NEXT           2
#define INDEX_HOLD           3

#define TITLE_TEXT_MAX_CHARS 24

typedef struct {
    Sprite* vdpSprite;    
    s16 x, y;             
    s16 offsetX, offsetY; 
    u16 frame;            
    u16 animation;        
    u16 animTimer;        
    u16 stateTimer;       
    u16 attr;             
    s16 animDir;          
    u8  type;             
    u8  padding;          
} GameSprite;

// Deklarationen für den Manager (sprite.c)
// Muss exakt mit der Definition in sprite.c (4) übereinstimmen
extern GameSprite gameSprites[4];

// --- Dust-Partikel ---
typedef struct {
    Sprite* vdpSprite;
    bool    active;
    u8      frame;
    u8      frameTick;
    u8      frameTickLimit;
    u8      startDelay;
    u8      kind;
    u8      risePerFrame;
    s16     startX;
    s16     startY;
    s16     clipMinX;
    s16     clipMaxX;
} DustParticle;

extern DustParticle dustParticles[DUST_SLOT_COUNT];

void sprites_init();
void sprites_init_text_only();
void sprites_update();
void sprites_sync_game(Vect2D_s16 piecePos, Vect2D_s16 shadowPos, u8 activeEffect);
void sprites_sync_vs_effect(const GameContext* player, s16 boardOriginX, s16 boardOriginY);
void sprites_set_visible(u8 index, bool visible);
void sprites_trigger_dust(s16 x, s16 y, bool riseUp);
void sprites_trigger_dust_at_board_origin(s16 boardOriginX, s16 boardOriginY, s16 pieceX, s16 ghostY, bool riseUp);
void sprites_trigger_line_clear_explosions(u32 clearingLineMask);
void sprites_trigger_line_clear_explosions_at_origin(u32 clearingLineMask, s16 boardOriginX, s16 boardOriginY);
void sprites_trigger_explosion_at_board_cell(u16 boardX, u16 boardY, u8 delayMax);
void sprites_trigger_explosion_at_board_cell_at_origin(u16 boardX, u16 boardY, u8 delayMax, s16 boardOriginX, s16 boardOriginY);
void sprites_text_set_enabled(bool enabled);
void sprites_text_set_string(const char* text);
void sprites_text_set_position(u8 index, s16 x, s16 y);
void sprites_text_set_glyph(u8 index, char c, s16 x, s16 y, bool visible);
void sprites_text_clear(void);
u8 sprites_text_get_length(void);
void sprites_cleanup();

#endif