#include <genesis.h>
#include "sound_manager.h"
#include "states/game/game_core.h"
#include "states/game/game_view.h" // Enthält Prototyp für set_game_comment
#include "states/game/game_logic.h" // Für checkCollision, lockPiece, performHold
#include "states/states.h"

bool controls_update(GameContext *gctx) {
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

    // --- 2. BEWEGUNG (DAS - über gameConditions) ---
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
            ctx->dasNextThreshold = gameConditions.thresholdLRInitial;
        } else if (ctx->dasDir == currentDir) {
            ctx->dasTimer++;
            if (ctx->dasTimer >= ctx->dasNextThreshold) {
                s16 step = (currentDir == vBtnLeft) ? -1 : 1;
                if (!checkCollision(ctx->pieceX + step, ctx->pieceY, ctx->rotation)) {
                    ctx->pieceX += step;
                    moved = true;
                    SOUND_play(SND_MOVE);
                }
                ctx->dasTimer = 0;
                ctx->dasNextThreshold = gameConditions.thresholdLRRepeat;
            }
        }
    } else {
        ctx->dasTimer = 0;
        ctx->dasDir = 0;
        ctx->dasNextThreshold = gameConditions.thresholdLRInitial;
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
    if (gc_has_rule(GC_RULE_ALLOW_HOLD) && (changed & BUTTON_C)) {
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
        ctx->ghostY = ctx->pieceY;
        lockPiece();
        moved = true;
    }

    return moved;
}