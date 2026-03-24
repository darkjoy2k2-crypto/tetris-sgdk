#include <genesis.h>
#include <string.h>
#include <kdebug.h> // Erforderlich für KLog/KDebug-Ausgaben in BlastEm

#include "states/states.h"
#include "states/title.h"
#include "states/game_select.h"
#include "states/game.h"
#include "states/challenge.h"
#include "sound_manager.h"
#include "states/sound_test.h"
#include "states/gameover.h"
#include "states/highscore.h"
#include "states/options.h"
#include "menu_bg.h"
#include "fonts.h"
#include "states/save_manager.h"
#include "states/gfxtest.h"

// --- GLOBALE VARIABLEN ---
GameState currentState = STATE_TITLE;
GameState lastState = STATE_NONE;
u16 joyState = 0;
u16 lastJoyState = 0;

StateUnion *sctx = NULL; 
static u16 stateFadeTarget[48];
static u16 stateFadeSource[48];
static u16 stateFadeDest[48];
static u16 fullFadeTarget[64];
static bool bgPaletteFreezeByUiFade = FALSE;

#define UI_FADE_TICKS GET_TICKS(30)

static void set_all_palettes_black(void);

static bool use_menu_bg_text_fade(GameState state) {
    switch (state) {
        case STATE_TITLE:
        case STATE_SELECT:
        case STATE_SOUNDTEST:
        case STATE_GAMEOVER:
        case STATE_HIGHSCORE:
        case STATE_OPTIONS:
        case STATE_SAVE:
        case STATE_GFXTEST:
            return TRUE;
        default:
            return FALSE;
    }
}

static u16 scale_vdp_color(u16 c, u16 num, u16 den) {
    u16 r = (c >> 1) & 0x7;
    u16 g = (c >> 5) & 0x7;
    u16 b = (c >> 9) & 0x7;
    u16 rr = (u16)((r * num) / den);
    u16 gg = (u16)((g * num) / den);
    u16 bb = (u16)((b * num) / den);

    if (rr > 7) rr = 7;
    if (gg > 7) gg = 7;
    if (bb > 7) bb = 7;

    return (u16)((bb << 9) | (gg << 5) | (rr << 1));
}

static void build_menu_text_bg_ramp(u16 bg, u16* out48) {
    u16 dark = scale_vdp_color(bg, 2, 5);   // ~40%
    u16 mid  = scale_vdp_color(bg, 3, 5);   // ~60%
    u16 lit  = scale_vdp_color(bg, 4, 5);   // ~80%

    for (u16 i = 0; i < 48; i++) out48[i] = bg;

    // Vollstaendige Vorstufen fuer Font-Indizes 1..7 je Palette,
    // damit auch gelbe/rote Varianten gleichmaessig aus BG einblenden.
    for (u16 p = 0; p < 3; p++) {
        u16 o = (u16)(p << 4);
        out48[o + 1] = dark;
        out48[o + 2] = dark;
        out48[o + 3] = mid;
        out48[o + 4] = mid;
        out48[o + 5] = lit;
        out48[o + 6] = bg;
        out48[o + 7] = bg;
    }
}

static void begin_ui_fade(bool freezeBgPalette) {
    if (freezeBgPalette) {
        menu_bg_set_palette_frozen(TRUE);
        bgPaletteFreezeByUiFade = TRUE;
    }
}

static void update_ui_fade_freeze(void) {
    if (!bgPaletteFreezeByUiFade) return;
    if (PAL_isDoingFade()) return;

    menu_bg_set_palette_frozen(FALSE);
    bgPaletteFreezeByUiFade = FALSE;
}

static void start_boot_sequence(void) {
    u16 pal0Target[16];
    u16 pal0Black[16];

    // 1. Alles schwarz
    set_all_palettes_black();

    // 2. BG-Tiles und -Geometrie laden; intern setzt menu_bg_set_mode_instant PAL0 korrekt.
    menu_bg_set_mode_instant(BG_MODE_MENU);

    // Ziel-Palette (BG-Blau + Tetromino-Farben) von PAL0 merken, dann wieder schwarz.
    PAL_getColors(0, pal0Target, 16);
    PAL_setPalette(PAL0, palette_black, CPU);
    for (u16 i = 0; i < 16; i++) pal0Black[i] = 0x0000;

    // 3. Von schwarz auf BG-Palette einfaden.
    //    PAL1-3 bleiben schwarz und werden vom ersten State-Wechsel gefadet.
    begin_ui_fade(TRUE);
    PAL_fade(0, 15, pal0Black, pal0Target, UI_FADE_TICKS, FALSE);
}

static void set_all_palettes_black(void) {
    PAL_setPalette(PAL0, palette_black, CPU);
    PAL_setPalette(PAL1, palette_black, CPU);
    PAL_setPalette(PAL2, palette_black, CPU);
    PAL_setPalette(PAL3, palette_black, CPU);
}

static void set_ui_palettes_black(void) {
    PAL_setPalette(PAL1, palette_black, CPU);
    PAL_setPalette(PAL2, palette_black, CPU);
    PAL_setPalette(PAL3, palette_black, CPU);
}

static void fade_in_current_state(void) {
    u16 bg = PAL_getColor(0);
    bool menuFade = use_menu_bg_text_fade(currentState);
    PAL_getColors(16, stateFadeTarget, 48);
    if (menuFade) {
        build_menu_text_bg_ramp(bg, stateFadeSource);
        begin_ui_fade(TRUE);
        PAL_fade(16, 63, stateFadeSource, stateFadeTarget, UI_FADE_TICKS, TRUE);
    } else {
        set_ui_palettes_black();
        PAL_fadeIn(16, 63, stateFadeTarget, UI_FADE_TICKS, TRUE);
    }
}

static void fade_out_last_state(void) {
    if (lastState == STATE_NONE) return;

    if (use_menu_bg_text_fade(lastState)) {
        u16 bg = PAL_getColor(0);
        PAL_getColors(16, stateFadeSource, 48);
        build_menu_text_bg_ramp(bg, stateFadeDest);
        begin_ui_fade(TRUE);
        PAL_fade(16, 63, stateFadeSource, stateFadeDest, UI_FADE_TICKS, TRUE);
    } else {
        PAL_fadeOut(16, 63, UI_FADE_TICKS, TRUE);
    }
}

GlobalConfig config = {
    .serializable = {
        .currentScore = 0,
        .playerName = "ABC",
        .randMode = 0,
        .speedLevel = 3,
        .garbageFreq = 3,
        .itemMode = 1,
        .flags = FLAG_SHADOW | FLAG_HOLD | FLAG_NEXT | FLAG_MUSIC | FLAG_SOUND | FLAG_BG,
        .thresholdLRInitial = 6,
        .thresholdLRRepeat = 2,
        .thresholdSD = 3,
        .highscores = {
            {10000, "PET", 0}, {9000, "SGK", 0}, {8000, "CPU", 0},
            {7000, "VDP", 0},  {6000, "ACE", 0}, {5000, "SKY", 0},
            {4000, "DAN", 0},  {3000, "EVA", 0}, {2000, "MAX", 0},
            {1000, "JOE", 0}
        }
    },
    .preferredState = STATE_NONE,
    .sramop = SRAM_NONE
};

StateHandler states[11]; 

void initStateMachine() {
    states[STATE_TITLE]     = (StateHandler){ title_init,      title_init_draw,      title_update,      title_draw,      title_cleanup };
    states[STATE_SELECT]    = (StateHandler){ select_init,     select_init_draw,     select_update,     select_draw,     select_cleanup };
    states[STATE_GAME]      = (StateHandler){ game_init,       game_init_draw,       game_update,       game_draw,       game_cleanup };
    states[STATE_SOUNDTEST] = (StateHandler){ sound_test_init, sound_test_init_draw, sound_test_update, sound_test_draw, sound_test_cleanup };
    states[STATE_GAMEOVER]  = (StateHandler){ gameover_init,   gameover_init_draw,   gameover_update,   gameover_draw,   gameover_cleanup };
    states[STATE_HIGHSCORE] = (StateHandler){ highscore_init,  highscore_init_draw,  highscore_update,  highscore_draw,  highscore_cleanup };
    states[STATE_OPTIONS]   = (StateHandler){ options_init,    options_init_draw,    options_update,    options_draw,    options_cleanup };
    states[STATE_SAVE]      = (StateHandler){ saving_init,     saving_init_draw,     saving_update,     saving_draw,     saving_cleanup };
    states[STATE_CHALLENGE] = (StateHandler){ challenge_init,  challenge_init_draw,  challenge_update,  challenge_draw,  challenge_cleanup };
    states[STATE_GFXTEST]   = (StateHandler){ gfxtest_init,    gfxtest_init_draw,    gfxtest_update,    gfxtest_draw,    gfxtest_cleanup };
}

void initHighscores() {
    char* names[] = {"PET", "SGK", "CPU", "VDP", "ACE", "SKY", "DAN", "EVA", "MAX", "JOE"};
    for (u16 i = 0; i < 10; i++) {
        strncpy(config.highscores[i].name, names[i], 3);
        config.highscores[i].name[3] = '\0';
        config.highscores[i].score = (u32)((10 - i) * 1000);
        config.highscores[i].isNew = 0;
    }
}

int main(bool hardReset) {
    // 0. KDebug Test-Ausgabe (Erscheint sofort im BlastEm Terminal)
    KLog("!n");    KLog("--- LOG RESET: NEW SESSION STARTED ---");    
    KLog("--- BOOT SEQUENCE START ---");
    KLog_U1("Hard Reset: ", hardReset);

    // 1. Hardware-Basis-Inits
    JOY_init();
    
    // Speicher für die Union reservieren
    sctx = MEM_alloc(sizeof(StateUnion));
    if (sctx == NULL) {
        VDP_drawText("FATAL: MEMORY FULL", 10, 10);
        KLog("CRITICAL: MEM_alloc failed for sctx!");
        while(1); 
    }
    memset(sctx, 0, sizeof(StateUnion));

    // Region Check
    if (IS_PAL_SYSTEM) {
        VDP_setScreenHeight240();
        KLog("Region: PAL System detected.");
    } else {
        VDP_setScreenHeight224();
        KLog("Region: NTSC System detected.");
    }

    // 2. Ressourcen laden
    VDP_loadFont(&TS_FONT_CLEAR, CPU);
    set_all_palettes_black();
    VDP_setTextPalette(PAL3);
    
    // Inits
    initHighscores();
    initStateMachine();
    
    // 3. Hintergrund-System initialisieren
    menu_bg_init();
    start_boot_sequence();

    joyState = JOY_readJoypad(JOY_1);
    lastJoyState = joyState; 
    
    SOUND_init(); 
    SOUND_playMusic();

    // Start-Zustand: SRAM Initialisierung über SaveManager
    KLog("MAIN: Switching to STATE_SAVE (SRAM_INIT)");
    currentState = STATE_SAVE;
    config.sramop = SRAM_INIT;
    
    // 4. Hauptschleife
    while(1) {
        joyState = JOY_readJoypad(JOY_1);

        // State-Wechsel-Logik
        if (currentState != lastState) {
            bool enteringGame = (currentState == STATE_GAME);
            bool leavingGame = (lastState == STATE_GAME);
            bool titleToChallenge = (lastState == STATE_TITLE && currentState == STATE_CHALLENGE);

            KLog_U1("STATE_CHANGE: Old State = ", lastState);
            KLog_U1("STATE_CHANGE: New State = ", currentState);

            if (enteringGame || leavingGame || titleToChallenge) {
                // Fullscreen-Transitions: zuerst komplett auf schwarz, BG waehrend schwarz umschalten.
                menu_bg_set_palette_frozen(FALSE);
                bgPaletteFreezeByUiFade = FALSE;
                PAL_fadeOut(0, 63, UI_FADE_TICKS, FALSE);

                // Waerend Schwarz den Menu-BG sofort deaktivieren.
                menu_bg_set_mode_instant(BG_MODE_NONE);
            } else {
                fade_out_last_state();
            }

            if (lastState != STATE_NONE) states[lastState].cleanup();

            // RAM-Bereich für den neuen State säubern (Union-Gefahr verhindern)
            memset(sctx, 0, sizeof(StateUnion));

            lastJoyState = joyState; 
            states[currentState].init();
            states[currentState].init_draw();

            if (enteringGame) {
                // Kein globaler Fade-In hier: Game steuert seinen eigenen Start-Fade.
            } else if (leavingGame || titleToChallenge) {
                PAL_getColors(0, fullFadeTarget, 64);
                set_all_palettes_black();
                PAL_fadeIn(0, 63, fullFadeTarget, UI_FADE_TICKS, FALSE);
            } else {
                fade_in_current_state();
            }
            lastState = currentState;
        }

        // State-spezifische Logik & Grafik
        states[currentState].update();
        states[currentState].draw();
        
        // Hintergrund-Animation
        menu_bg_update();
        update_ui_fade_freeze();

        lastJoyState = joyState;

        // VBlank-Synchronisation
        SYS_doVBlankProcess();
    }

    return 0;
}