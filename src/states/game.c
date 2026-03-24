#include <genesis.h>
#include <string.h>

#include "bg.h"
#include "fonts.h"
#include "gfx.h"
#include "menu_bg.h"
#include "sprite.h"
#include "sprites.h"
#include "sound_manager.h"
#include "states/states.h"
#include "states/game/game_core.h"
#include "states/game/game_conditions.h"
#include "states/game/conditions.h"
#include "states/game/game_logic.h"
#include "states/game/game_view.h"
#include "states/game/game_controls.h"

GameContext* ctx = NULL;
static bool stageClearEffectTriggered = FALSE;

const u16 GARBAGE_INTERVALS[] = {0, 1200, 1000, 850, 700, 580, 460, 340, 240, 120};

static s16 get_board_top_row(void)
{
    for (s16 y = 0; y < BOARD_HEIGHT; y++) {
        u16 rowOffset = (u16)((y << 3) + (y << 1));
        for (u16 x = 0; x < BOARD_WIDTH; x++) {
            if (ctx->board[rowOffset + x] != 0) {
                return y;
            }
        }
    }
    return -1;
}

// Ganz am Ende von game_logic.c einfügen:

static bool handle_gravity(GameContext *gctx)
{
    // Phase 4: handle_gravity
    u16 vBtnSoftDrop = (ctx->activeBadEffect == EFFECT_REVERSED) ? BUTTON_LEFT : BUTTON_DOWN;
    bool moved = false;
    ctx->moveTimer++;

    u16 speedSetting = (gameConditions.speedLevel > SPEED_LEVEL_MAX) ? SPEED_LEVEL_MAX : gameConditions.speedLevel;
    u16 levelOffset = ((ctx->level - 1) << 1) + (ctx->level - 1);
    s16 threshold = GET_TICKS(60 - levelOffset);

    // 0 = no extra speed, 9 = max possible speed for normal gravity path.
    if (speedSetting > 0) {
        threshold -= (s16)(speedSetting * 8);
    }

    if (threshold < 1) threshold = 1;

    if (ctx->activeBadEffect == EFFECT_FULLSPEED)
    {
        // Warning phase: 2 seconds with 1 alert per second before fast gravity starts in second 3.
        if (ctx->badEffectTimer > DUR_FULLSPEED_SPAWNS)
        {
            u16 warningTicks = ctx->badEffectTimer - DUR_FULLSPEED_SPAWNS;
            if (warningTicks == GET_TICKS(120) || warningTicks == GET_TICKS(60)) {
                SOUND_play(SND_ALERT);
            }
        }
        // Fast phase: the next 5 tetrominoes fall very fast, but not maximum speed.
        else if (ctx->badEffectTimer > 0)
        {
            threshold = 4;
        }
    }

    u16 finalThreshold = threshold;
    
    if (joyState & vBtnSoftDrop) {
        finalThreshold = gameConditions.thresholdSD;
    } else if (ctx->activeBadEffect == EFFECT_FREEZE) {
        finalThreshold = 9999;
    }

    if (finalThreshold < 1) finalThreshold = 1;

    if (ctx->moveTimer >= finalThreshold)
    {
        KLog_U2("GRAVITY: Timer reached threshold. Timer:", ctx->moveTimer, "Threshold:", finalThreshold);
        
        if (!checkCollision(ctx->pieceX, ctx->pieceY + 1, ctx->rotation))
        {
            ctx->pieceY++;
            moved = true;
            if (joyState & vBtnSoftDrop) {
                ctx->score++;
                SOUND_play(SND_SOFT_DROP);
            }
        }
        else
        {
            KLog_U2("GRAVITY: Collision below at X:", ctx->pieceX, "Y:", ctx->pieceY + 1);
            bool softDropLock = (joyState & vBtnSoftDrop) != 0;
            menu_bg_riistar_pulse(softDropLock ? 2 : 1);
            lockPiece(softDropLock);
            // lockPiece spawnt nun direkt neu
            moved = true;
        }
        ctx->moveTimer = 0;
    }

    return moved;
}


static bool handle_environment(GameContext *gctx)
{
    // Phase 5: handle_environment
    bool garbageTriggered = false;
    if (gctx == NULL)
    {
        KLog("ENVIRONMENT: Error - gctx is NULL");
        return false;
    }

    bool markersPresent = (gctx->boardFlags & GF_PENDING_MASK) != 0;
    bool blinkActive = (gctx->clearTimer > 0);

    // Garbage-Check: WENN (keine Marker) UND (kein Blink-Timer)
    u16 garbageSetting = (gameConditions.garbageFreq > GARBAGE_FREQ_MAX) ? GARBAGE_FREQ_MAX : gameConditions.garbageFreq;

    if (garbageSetting > 0 && gctx->activeBadEffect != EFFECT_FREEZE && 
        !markersPresent && !blinkActive)
    {
        gctx->garbageTimer++;
        if (gctx->garbageTimer >= gctx->garbageNextThreshold)
        {
            KLog_U2("ENVIRONMENT: Garbage Triggered. Timer:", gctx->garbageTimer, "Threshold:", gctx->garbageNextThreshold);
            
            addGarbageLine();
            garbageTriggered = true;

            gctx->garbageTimer = 0;
            u16 base = GARBAGE_INTERVALS[garbageSetting];
            gctx->garbageNextThreshold = GET_TICKS(base + (random() % 120) - 60);
            if (gctx->garbageNextThreshold < GET_TICKS(30)) {
                gctx->garbageNextThreshold = GET_TICKS(30);
            }
            gctx->boardFlags |= GF_NEEDS_DRAW;
            
            KLog_U1("ENVIRONMENT: New Garbage Threshold set to:", gctx->garbageNextThreshold);
        }
    }

    // Effekt-Timer: [Reduziere aktive Buffs/Debuffs]
    if (gctx->badEffectTimer > 0)
    {
        // Stückbasierte Effekte ausschließen
        if (gctx->activeBadEffect != EFFECT_RAINBOW &&
            gctx->activeBadEffect != EFFECT_SAME_TILES &&
            gctx->activeBadEffect != EFFECT_I_RAIN &&
            gctx->activeBadEffect != EFFECT_MULTIPLIER)
        {
            if (gctx->activeBadEffect == EFFECT_FULLSPEED)
            {
                if (gctx->badEffectTimer > DUR_FULLSPEED_SPAWNS) gctx->badEffectTimer--;
            }
            else 
            {
                gctx->badEffectTimer--;
                if (gctx->badEffectTimer <= 0)
                {
                    KLog_U1("ENVIRONMENT: Bad Effect Expired. ID:", gctx->activeBadEffect);
                    bool preserveEffect = false;
                    
                    // Clean up effect-specific state
                    switch(gctx->activeBadEffect) {
                        case EFFECT_HOLD_LOCK:
                            if (gc_has_rule(GC_RULE_ALLOW_HOLD)) gctx->flags |= GF_CAN_HOLD;
                            gctx->holdType = gctx->lastHoldType;
                            KLog("ENVIRONMENT: HOLD_LOCK released.");
                            break;
                        case EFFECT_REVERSED:
                            KLog("ENVIRONMENT: CONFUSION ended.");
                            break;
                        case EFFECT_HIDE_NEXT:
                            KLog("ENVIRONMENT: NONEXT ended.");
                            break;
                        case EFFECT_NO_ROTATE:
                            gctx->flags &= ~GF_ROT_LOCKED;
                            KLog("ENVIRONMENT: NOROTATION ended.");
                            break;
                        case EFFECT_SHADOW_BOARD:
                            // LIGHTSOUT ends, apply rainbow effect
                            ctx->activeBadEffect = EFFECT_RAINBOW;
                            ctx->sortingRow = 0;
                            preserveEffect = true;
                            KLog("ENVIRONMENT: LIGHTSOUT ended - RAINBOW effect triggered");
                            break;
                        default:
                            break;
                    }

                    if (!preserveEffect) {
                        gctx->activeBadEffect = EFFECT_NONE;
                        gctx->lastActiveBadEffect = 99;
                    }
                    SOUND_play(SND_GOOD_ITEM);
                }
            }
        }
    }
    
    return garbageTriggered;
}

// --- REINE LOGIK INITIALISIERUNG ---
void game_init()
{
    ctx = &sctx->game;
    if (ctx == NULL)
        return;

    stageClearEffectTriggered = FALSE;

    // 2. Logik-Reset aufrufen
    reset_game_logic();

    // 3. Hardware & VDP Setup
    PAL_setPalette(PAL2, anim_norotate.palette->data, DMA);
    UI_init_fonts_and_palettes();
    SOUND_init();

    // Testpfad: Riistar fest aktivieren, damit BG-Geometrie/Scroll isoliert geprüft werden kann.
    menu_bg_set_mode(BG_MODE_RIISTAR);
}


void game_init_draw()
{
    // Sicherheitscheck: Ohne Context kein Zeichnen
    if (ctx == NULL)
        return;

    // 1. VRAM Säuberung
    VDP_clearPlane(BG_A, TRUE);

    // 2. Grafik-Ressourcen laden
    load_background();

    // 3. Tile-Cache Initialisierung
    view_init_cache();
    // PAL_setPalette(PAL0, anim_norotate.palette->data, DMA); //SPRITEPALETTE
    sprites_init();
    // 4. Paletten-Setup für UI und Text
    // PAL_setPalette(PAL3, PAL_FONT_CLEAR.data, CPU);
    // VDP_setTextPalette(PAL3);
    UI_init_fonts_and_palettes(); // Setzt PAL1, PAL2, PAL3 und Font

    // Einziger Fade-In beim Einstieg ins Spiel.
    view_fade_in_frame();
}

void game_update()
{
    if (ctx == NULL)
    {
        KLog("GAME_UPDATE: Error - Context is NULL");
        return;
    }

    if ((config.runtime.gameMode == GAME_MODE_CHALLENGE) &&
        gameConditions.success &&
        !stageClearEffectTriggered) {
        ctx->activeBadEffect = EFFECT_RAINBOW;
        ctx->sortingRow = 0;
        ctx->badEffectTimer = 0;
        ctx->boardFlags |= GF_NEEDS_DRAW;
        stageClearEffectTriggered = TRUE;
    }

    if ((config.runtime.gameMode == GAME_MODE_CHALLENGE) &&
        gameConditions.success &&
        (joyState & BUTTON_START) && !(lastJoyState & BUTTON_START)) {
        config.currentScore = ctx->score;
        config.runtime.challengeResult = CHALLENGE_RESULT_SUCCESS;
        SOUND_play(SND_MENU_SELECT);
        currentState = STATE_CHALLENGE;
        lastJoyState = joyState;
        return;
    }

    // 1. Blink-Phase (Timer)
    if (ctx->clearTimer > 0) {
        // Update blinking animation (show/hide blocks pattern)
        update_blinking_animation();
        
        ctx->clearTimer--;
        if (ctx->clearTimer == 0) {
            finishLineClear();
        }
        ctx->boardFlags |= GF_NEEDS_DRAW;
    }

    // 2. Collapse-Phase (Marker)
    bool collapseActive = false;
    if (ctx->boardFlags & GF_PENDING_MASK) {
        handle_board_collapse();
        ctx->boardFlags |= GF_NEEDS_DRAW;
        collapseActive = true;
    }

    // 2.5 Board Animations (Sorting, Rainbow)
    update_board_animations();


    // 3. controls_update() [Immer aktiv]
    bool moved = controls_update(ctx);
    if (moved) 
    {
        KLog("GAME_UPDATE: Player movement detected.");
    }

    // 4. handle_gravity() [Immer aktiv]
    if (handle_gravity(ctx))
    {
        moved = true;
    }

    // 5. handle_environment()
    bool garbage = handle_environment(ctx);

    // 6. shadow_update()
    update_shadows(moved, collapseActive, garbage);

    // 7. sprites_update()
    sprites_update();

    // Riistar-Drive: X-Parallax aus Boardhoehe.
    menu_bg_riistar_set_stack_top(get_board_top_row());

    if (moved) ctx->boardFlags |= GF_NEEDS_DRAW;
    lastJoyState = joyState;
}


void game_draw()
{
    if (ctx == NULL)
        return;

    // Nutzt das Bit-Flag GF_NEEDS_DRAW aus dem u32 boardFlags Member
    if (ctx->boardFlags & GF_NEEDS_DRAW)
    {
        drawBoard();

        // Debug Bag Anzeige aufrufen
        // view_draw_debug_bag(ctx);

        // Bit-Flag löschen (Reset)
        ctx->boardFlags &= ~GF_NEEDS_DRAW;
    }

    view_update_ui(ctx);
}

void game_cleanup()
{
    for (u16 i = 0; i < 4; i++) {
        sprites_set_visible(i, FALSE);
    }

    for (u16 di = 0; di < DUST_SLOT_COUNT; di++) {
        dustParticles[di].active = FALSE;
        if (dustParticles[di].vdpSprite != NULL) {
            SPR_setPosition(dustParticles[di].vdpSprite, -128, -128);
        }
    }
    SPR_update();

    // Nur lokalen Pointer lösen, kein MEM_free
    VDP_clearPlane(BG_A, TRUE);
    sprites_cleanup(); // Falls vorhanden, um Hardware-Sprites zu entladen
    ctx = NULL;
}