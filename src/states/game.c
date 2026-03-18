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
#include "states/game/game_logic.h"
#include "states/game/game_view.h"
#include "states/game/game_controls.h"

GameContext* ctx = NULL;

const u16 GRAVITY_SPEEDS[] = {9999, 60, 30, 15};
const u16 GARBAGE_INTERVALS[] = {0, 1200, 600, 300};

// Ganz am Ende von game_logic.c einfügen:

static bool handle_gravity(GameContext *gctx)
{
    u16 vBtnSoftDrop = (ctx->activeBadEffect == EFFECT_REVERSED) ? BUTTON_LEFT : BUTTON_DOWN;
    bool moved = false;
    ctx->moveTimer++;

    // 1. Basis-Geschwindigkeit berechnen
    u16 levelOffset = ((ctx->level - 1) << 1) + (ctx->level - 1);
    s16 threshold = GET_TICKS(60 - levelOffset);
    if (threshold < 3) threshold = 3;

// 2. FULLSPEED Logik
    if (ctx->activeBadEffect == EFFECT_FULLSPEED)
    {
        if (ctx->badEffectTimer > 1) 
        {
            if (ctx->badEffectTimer % 60 == 0) SOUND_play(SND_ALERT);
        } 
        // Korrektur: Auch bei Timer-Stand 1 (Sirene vorbei) sofort Speed aktivieren
        else if (ctx->badEffectTimer <= 1) 
        {
            threshold = 2; 
        }
    }

u16 finalThreshold = threshold;
    
    if (joyState & vBtnSoftDrop) {
        // Nutzt den dynamischen Wert aus den Optionen (Standard: 2)
        finalThreshold = config.thresholdSD; 
    } else if (ctx->activeBadEffect == EFFECT_FREEZE) {
        finalThreshold = 9999;
    }

    // 4. Bewegung ausführen
    if (ctx->moveTimer >= finalThreshold)
    {
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
            lockPiece();
            moved = true;
        }
        ctx->moveTimer = 0;
    }

    return moved;
}



static void handle_environment(GameContext *gctx, bool moved)
{
    if (gctx == NULL) return;

    // 1. Garbage Logik
    if (config.garbageFreq > 0 && gctx->activeBadEffect != EFFECT_FREEZE)
    {
        gctx->garbageTimer++;
        if (gctx->garbageTimer >= gctx->garbageNextThreshold)
        {
            addGarbageLine();
            
            // WICHTIG: Nach Garbage hat sich das Board verändert. 
            // Der Schatten muss zwingend neu berechnet werden.
            calculate_ghost_y();

            gctx->garbageTimer = 0;
            u16 base = GARBAGE_INTERVALS[config.garbageFreq];
            gctx->garbageNextThreshold = GET_TICKS(base + (random() % 120) - 60);
            gctx->boardFlags |= GF_NEEDS_DRAW;
        }
    }

    // 2. Shadow Logik
    // Wir rufen die zentrale Funktion, wenn eine Bewegung stattfand.
    if (moved && GET_FLAG(config.flags, FLAG_SHADOW))
    {
        calculate_ghost_y();
    }

    // 3. Bad Effect Timer Management
    if (gctx->badEffectTimer > 0)
    {
        // Stückbasierte Effekte ausschließen
        if (gctx->activeBadEffect != EFFECT_RAINBOW &&
            gctx->activeBadEffect != EFFECT_SAME_TILES &&
            gctx->activeBadEffect != EFFECT_I_RAIN)
        {
            if (gctx->activeBadEffect == EFFECT_FULLSPEED)
            {
                if (gctx->badEffectTimer > 1) gctx->badEffectTimer--;
            }
            else 
            {
                gctx->badEffectTimer--;
                if (gctx->badEffectTimer <= 0)
                {
                    if (gctx->activeBadEffect == EFFECT_HOLD_LOCK)
                    {
                        if (GET_FLAG(config.flags, FLAG_HOLD)) gctx->flags |= GF_CAN_HOLD;
                        gctx->holdType = gctx->lastHoldType;
                    }
                    gctx->activeBadEffect = EFFECT_NONE;
                    gctx->lastActiveBadEffect = 99;
                    SOUND_play(SND_GOOD_ITEM);
                }
            }
        }
    }
}

// --- REINE LOGIK INITIALISIERUNG ---
void game_init()
{
    ctx = &sctx->game;
    if (ctx == NULL)
        return;

    // 2. Logik-Reset aufrufen
    reset_game_logic();

    // 3. Hardware & VDP Setup
    PAL_setPalette(PAL2, anim_norotate.palette->data, DMA);
    UI_init_fonts_and_palettes();
    SOUND_init();

    // Hintergrund für das Spiel konfigurieren
    menu_bg_set_mode(BG_MODE_SPACE);
    menu_bg_set_active(GET_FLAG(config.flags, FLAG_BG));
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

    // 5. Visueller Start-Effekt
    view_fade_in_frame();
}

void game_update()
{
    if (ctx == NULL)
        return;

    // 1. Animationen (z.B. Zeilen löschen) blockieren Steuerung
    if (handle_active_animations(ctx))
    {
        lastJoyState = joyState;
        // Hardware-Animationen der Sprites trotzdem weiterlaufen lassen
        sprites_update();
        return;
    }

    // 2. Eingabe verarbeiten
    bool moved = controls_update(ctx);

    // 3. Schwerkraft
    if (handle_gravity(ctx))
        moved = true;

    // 4. Logik-Timer (Effekte, Schatten, Garbage)
    handle_environment(ctx, moved);

    // 5. Sprite-Animationen (Hardware-Update)
    // Ersetzt update_curse_sprites, da die Logik nun in sprite.c kapselt ist
    sprites_update();

    if (moved)
        ctx->boardFlags |= GF_NEEDS_DRAW;

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

    sprites_update();
    view_update_ui(ctx);
}

void game_cleanup()
{
    // Nur lokalen Pointer lösen, kein MEM_free
    VDP_clearPlane(BG_A, TRUE);
    sprites_cleanup(); // Falls vorhanden, um Hardware-Sprites zu entladen
    ctx = NULL;
}