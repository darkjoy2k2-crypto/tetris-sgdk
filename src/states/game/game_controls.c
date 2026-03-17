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
// --- 5. HARD DROP ---
    // Nutzt 'changed' für Flankenerkennung (muss losgelassen worden sein)
    if (changed & vBtnHardDrop) {
        SOUND_play(SND_HARD_DROP);
        
        // 1. Teleportation nach unten
        while (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation)) {
            ctx->pieceY++;
        }
        
        // 2. WICHTIG: ghostY angleichen, um Diskrepanzen im selben Frame zu vermeiden
        ctx->ghostY = ctx->pieceY;

        // 3. LockPiece aufrufen
        // FAKT: lockPiece ruft intern bereits clearLines() und spawnPiece() auf!
        lockPiece();
        
        // Der manuelle Aufruf von spawnPiece() wurde hier ENTFERNT.
        moved = true;
    }

    // --- 6. SELECT TRIGGER (Manual Sort) ---
    // --- 6. DEBUG TRIGGER (Statt Manual Sort jetzt No-Rotate Fluch) ---
// --- 6. DEBUG TRIGGER (EFFECT_FULLSPEED via START) ---
    if (changed & BUTTON_START) {
        // Falls ein Effekt aktiv ist, schalten wir ihn aus (Toggle)
        if (ctx->activeBadEffect != EFFECT_NONE) {
            ctx->activeBadEffect = EFFECT_NONE;
            ctx->badEffectTimer = 0;
            ctx->lastActiveBadEffect = 99; // Erzwingt Sprite-Reset
            SOUND_play(SND_GOOD_ITEM);
        } else {
            // FULLSPEED aktivieren: 120 Frames Piepen + 300 Frames High-Speed
            ctx->activeBadEffect = EFFECT_FULLSPEED;
            ctx->badEffectTimer = 120 + (DUR_FULLSPEED_SPAWNS * 60); 
            ctx->lastActiveBadEffect = 99; // Erzwingt Sprite-Sync
            
            // Erstes Warnsignal sofort
           //SOUND_play(SND_ALERT);
            set_game_comment("GET READY...", 60);
        }
        
        moved = true; 
    }

    return moved;
}