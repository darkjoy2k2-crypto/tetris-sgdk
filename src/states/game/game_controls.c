#include <genesis.h>
#include "states/states.h"
#include "states/game/game_controls.h"
#include "states/game/game_logic.h"
#include "sound_manager.h"

bool controls_update(GameContext *ctx) {
    if (ctx == NULL) return false;

    u16 changed = joyState & ~lastJoyState;
    bool moved = false;

    // --- 1. VIRTUAL MAPPING (Verwirr-Effekt) ---
    u16 vBtnLeft     = BUTTON_LEFT;
    u16 vBtnRight    = BUTTON_RIGHT;
    u16 vBtnSoftDrop = BUTTON_DOWN;
    u16 vBtnHardDrop = BUTTON_UP;
    u16 vBtnRotCCW   = BUTTON_A; 
    u16 vBtnRotCW    = BUTTON_B; 

    if (ctx->activeBadEffect == EFFECT_REVERSED) {
        vBtnLeft     = BUTTON_B;    
        vBtnRight    = BUTTON_A;    
        vBtnRotCW    = BUTTON_UP;   
        vBtnRotCCW   = BUTTON_DOWN; 
        vBtnSoftDrop = BUTTON_LEFT; 
        vBtnHardDrop = BUTTON_RIGHT;
    }

    // --- 2. BEWEGUNG (DAS - Delayed Auto Shift) ---
    u16 currentDir = (joyState & vBtnLeft) ? vBtnLeft : ((joyState & vBtnRight) ? vBtnRight : 0);
    
    if (currentDir != 0) {
        if (changed & currentDir) {
            s16 step = (currentDir == vBtnLeft) ? -1 : 1;
            if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                ctx->pieceX += step;
                moved = true;
                SOUND_play(SND_MOVE);
            }
            ctx->dasTimer = 0;
            ctx->dasDir = currentDir;
        } else if (ctx->dasDir == currentDir) {
            ctx->dasTimer++;
            if (ctx->dasTimer >= 6 && (ctx->dasTimer - 6) % 2 == 0) {
                s16 step = (currentDir == vBtnLeft) ? -1 : 1;
                if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                    ctx->pieceX += step;
                    moved = true;
                    SOUND_play(SND_MOVE);
                }
            }
        }
    } else {
        ctx->dasTimer = 0;
        ctx->dasDir = 0;
    }

    // --- 3. ROTATION (Wall-Kicks) ---
    if (changed & (vBtnRotCW | vBtnRotCCW)) {
        if (ctx->activeBadEffect == EFFECT_NO_ROTATE) {
            if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
                ctx->pieceY++;
                moved = true;
            }
            SOUND_play(SND_BAD_ITEM);
        } else {
            u16 nr = (changed & vBtnRotCW) ? (ctx->rotation + 1) % 4 : (ctx->rotation + 3) % 4;
            s16 kicks[] = {0, 1, -1, 2, -2};
            for(u8 i=0; i<5; i++) {
                if (!checkCollision(ctx->pieceX + kicks[i], ctx->pieceY, nr)) {
                    ctx->pieceX += kicks[i];
                    ctx->rotation = nr;
                    moved = true;
                    SOUND_play(SND_ROTATE);
                    break;
                }
            }
        }
    }

    // --- 4. HOLD ---
if (GET_FLAG(config.flags, FLAG_HOLD) && (changed & BUTTON_C)) {
            if (ctx->activeBadEffect != EFFECT_HOLD_LOCK) {
            performHold();
            moved = true; 
        } else {
            SOUND_play(SND_BAD_ITEM);
        }
    }

    // --- 5. HARD DROP ---
    if (changed & vBtnHardDrop) {
        SOUND_play(SND_HARD_DROP);
        while (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
        }
        lockPiece();
        if (clearLines() == 0) spawnPiece();
        moved = true;
    }

    // --- 6. SELECT TRIGGER (Manual Sort) ---
    // --- 6. DEBUG TRIGGER (Statt Manual Sort jetzt No-Rotate Fluch) ---
if (changed & BUTTON_START) {
    // Falls der Fluch schon aktiv ist, schalten wir ihn aus (Toggle)
    if (ctx->activeBadEffect == EFFECT_NO_ROTATE) {
        ctx->activeBadEffect = EFFECT_NONE;
        ctx->badEffectTimer = 0;
        SOUND_play(SND_GOOD_ITEM);
    } else {
        // Fluch aktivieren
        ctx->activeBadEffect = EFFECT_NO_ROTATE;
        // 10 Sekunden (600 Frames bei 60Hz)
        ctx->badEffectTimer = GET_TICKS(600); 
        
        SOUND_play(SND_BAD_ITEM);
    }
    
    // moved auf true setzen, damit das Board-Redraw/Sprite-Update getriggert wird
    moved = true; 
}

    return moved;
}