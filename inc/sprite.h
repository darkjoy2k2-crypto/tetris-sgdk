#ifndef _SPRITE_H_
#define _SPRITE_H_

#include <genesis.h>
#include "states/states.h" 

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

void sprites_init();
void sprites_update();
void sprites_sync_game(Vect2D_s16 piecePos, Vect2D_s16 shadowPos, u8 activeEffect);
void sprites_set_visible(u8 index, bool visible);
void sprites_cleanup();

#endif