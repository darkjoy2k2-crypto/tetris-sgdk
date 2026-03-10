#include "game_input.h"

void input_update(GameContext* ctx) {
    u16 currentJoy = joyState;
    u16 btnLeft = BUTTON_LEFT;
    u16 btnRight = BUTTON_RIGHT;

    // Mapping für DAS bei Reversed
    if (ctx->activeBadEffect == EFFECT_REVERSED) {
        btnLeft = BUTTON_B; 
        btnRight = BUTTON_A;
    }

    u16 currentDir = 0;
    if (currentJoy & btnLeft) currentDir = btnLeft;
    else if (currentJoy & btnRight) currentDir = btnRight;

    if (currentDir != 0) {
        if (currentDir == ctx->dasDir) ctx->dasTimer++;
        else {
            ctx->dasTimer = 0;
            ctx->dasDir = currentDir;
        }
    } else {
        ctx->dasTimer = 0;
        ctx->dasDir = 0;
    }
}

PlayerInput input_get_mapped_state(GameContext* ctx) {
    PlayerInput pi = {0};
    u16 joy = joyState;
    u16 changed = joy & ~lastJoyState;

    if (ctx->activeBadEffect == EFFECT_REVERSED) {
        pi.moveLeft  = (joy & BUTTON_B);
        pi.moveRight = (joy & BUTTON_A);
        pi.rotateCW  = (changed & BUTTON_UP);
        pi.rotateCCW = (changed & BUTTON_DOWN);
        pi.softDrop  = (joy & BUTTON_LEFT);
        pi.hardDrop  = (changed & BUTTON_RIGHT);
    } else {
        pi.moveLeft  = (joy & BUTTON_LEFT);
        pi.moveRight = (joy & BUTTON_RIGHT);
        pi.rotateCW  = (changed & BUTTON_B);
        pi.rotateCCW = (changed & BUTTON_A);
        pi.softDrop  = (joy & BUTTON_DOWN);
        pi.hardDrop  = (changed & BUTTON_UP);
    }
    pi.hold = (changed & BUTTON_C);
    return pi;
}