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

GameContext *ctx = NULL;

const u16 GRAVITY_SPEEDS[] = {9999, 60, 30, 15};
const u16 GARBAGE_INTERVALS[] = {0, 1200, 600, 300};

// Ganz am Ende von game_logic.c einfügen:

static bool handle_gravity(GameContext *ctx)
{
    u16 vBtnSoftDrop = (ctx->activeBadEffect == EFFECT_REVERSED) ? BUTTON_LEFT : BUTTON_DOWN;
    bool moved = false;
    ctx->moveTimer++;

    // 1. Basis-Geschwindigkeit berechnen
    s16 threshold = GET_TICKS(60 - ((ctx->level - 1) * 3));
    if (threshold < 3) threshold = 3;

    // 2. FULLSPEED Logik (Warnung vs. Action)
    if (ctx->activeBadEffect == EFFECT_FULLSPEED)
    {
        s16 actionPhaseThreshold = (DUR_FULLSPEED_SPAWNS * 60);
        
        if (ctx->badEffectTimer > actionPhaseThreshold) {
            // WARNPHASE: Piepen alle 30 Frames
            u16 timeLeftInWarn = ctx->badEffectTimer - actionPhaseThreshold;
            if (timeLeftInWarn % 30 == 0) SOUND_play(SND_ALERT);
            // Hier bleibt threshold beim normalen Level-Speed
        } else {
            // AKTIVPHASE: Stein rast
            threshold = 2; 
        }
    }

    // 3. Finaler Check (Soft-Drop oder Freeze)
    u16 finalThreshold = threshold;
    
    if (joyState & vBtnSoftDrop) {
        finalThreshold = threshold / 12;
        if (finalThreshold < 2) finalThreshold = 2;
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

static void handle_environment(GameContext *ctx, bool moved)
{
    if (ctx == NULL)
        return;

    // 1. Garbage Logik (Zeitbasierte neue Zeilen)
    if (ctx->garbageTimer >= ctx->garbageNextThreshold)
    {
        addGarbageLine();
        ctx->garbageTimer = 0;
        ctx->garbageNextThreshold = GET_TICKS(GARBAGE_INTERVALS[config.garbageFreq] + (random() % 120) - 60);
        ctx->boardFlags |= GF_NEEDS_DRAW;
    }

    // 2. Shadow Logik (Berechnung der Fall-Tiefe des Schattens)
    if (moved && GET_FLAG(config.flags, FLAG_SHADOW))
    {
        ctx->ghostY = ctx->pieceY;
        while (!checkCollision(ctx->pieceX, ctx->ghostY + 1, ctx->rotation))
            ctx->ghostY++;
    }

if (ctx->badEffectTimer > 0)
    {
        // NUR Rainbow und stückbasierte Effekte (falls du sie so behalten willst) ausschließen.
        // FULLSPEED MUSS hier dekrementiert werden, damit das Piepen (Frames) funktioniert!
        if (ctx->activeBadEffect != EFFECT_RAINBOW &&
            ctx->activeBadEffect != EFFECT_SAME_TILES &&
            ctx->activeBadEffect != EFFECT_I_RAIN)
        {
            ctx->badEffectTimer--;


            if (ctx->badEffectTimer <= 0)
            {
                // SPEZIALFALL: Hold Lock Ende
                if (ctx->activeBadEffect == EFFECT_HOLD_LOCK)
                {
                    ctx->holdType = ctx->lastHoldType; // Stein wieder einblenden
                    ctx->flags |= GF_CAN_HOLD;         // C-Taste wieder erlauben
                }

                // Übergang von Dunkelheit zu Rainbow
                if (ctx->activeBadEffect == EFFECT_SHADOW_BOARD)
                {
                    ctx->activeBadEffect = EFFECT_RAINBOW;
                    ctx->badEffectTimer = 0;
                    ctx->sortingRow = 0;
                    set_game_comment("RAINBOW REBIRTH!", 90);
                }
                else
                {
                    ctx->activeBadEffect = EFFECT_NONE;
                    ctx->lastActiveBadEffect = 99;
                }
                SOUND_play(SND_GOOD_ITEM);
            }
        }
    }
}

// --- REINE LOGIK INITIALISIERUNG ---
void game_init()
{
    // 1. Speicher-Lifecycle
    if (ctx != NULL)
    {
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
    if (ctx != NULL)
    {
        MEM_free(ctx);
        ctx = NULL;
    }
    VDP_clearPlane(BG_A, TRUE);
}