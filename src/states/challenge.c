#include "states/challenge.h"
#include "states/states.h"
#include "gfx.h"
#include <genesis.h>
#include "sound_manager.h"


static ChallengeContext* ctx = NULL;
static u8 grid_to_challenge_level[CHALLENGE_LEVEL_COUNT];
static bool challenge_translation_ready = FALSE;

static const char* const challenge_level_names[CHALLENGE_LEVEL_COUNT] = {
    "Start",
    "Erste Linie",
    "Doppelbau",
    "Vierer-Challenge",
    "Punktesammler",
    "30-Sekunden",
    "Lueckenspiel",
    "Herz-Sammler",
    "1000er-Sprung",
    "Schnellbahn",
    "Muellbau 1",
    "Punkte vs. Zeit",
    "Doppel-Muell",
    "Item-Party",
    "Board-Kontrolle",
    "Kurz-Sprint",
    "Trash-Ueberleben",
    "Herz & Schaedel",
    "5000er-Mark",
    "90-Sekunden-Spiel",
    "Geschwindigkeit-1",
    "Muell-Duell",
    "Sauberes Spiel",
    "Genau 5",
    "Mix-Einstieg",
    "Herzen gleich Skulls",
    "Tempo-Grenze",
    "Muell-Welle",
    "Langsam & Sicher",
    "Alles-Mix",
    "Sprint-Finale",
    "Leicht-Freischrift",
    "Geschwindigkeit-2",
    "Muell-Experte",
    "Hybrid-Sprint",
    "Ueberlebens-Training",
    "Schaedel-Sammler",
    "Saubermacher",
    "Kettenreaktion",
    "Schnell-Muell",
    "Zeit-Druck",
    "Gegensaetze",
    "Uebelkeit-Test",
    "Genau 7",
    "10k+ Punkte",
    "Ausdauer-Mittel",
    "Hybrid-Muell",
    "Kettenwelle",
    "Speed-7-Warm",
    "Hoher Muell",
    "Chaoten-Quiz",
    "Gesamt-Mix",
    "Praezision",
    "Herzen-Flut",
    "Schaedel-Vermeiden",
    "Jetztzeit",
    "Muell-Welt",
    "Hybrid-Fast",
    "Chain-Master",
    "Schwer-Mix",
    "Ueberlebens-Mittel",
    "3x Ziele",
    "Speed-Champion",
    "Finale-Mittel",
    "Geschwindigkeit-8",
    "Muell-Inferno",
    "Chaos-Storm",
    "30-Sekunden-Sprint",
    "Herzen-Jagd",
    "Schaedel-MinField",
    "Saubermacher-Schwer",
    "6x-Chain",
    "Zeitdruck-Hoelle",
    "Pyramiden-Bau",
    "Muell-Chaos",
    "Item-Ballett",
    "20k+ Punkte",
    "Ueberlebens-Hart",
    "Alles-Kombo",
    "Speed-9-Intro",
    "Schaedel-Flood",
    "40-Sekunden-Deal",
    "Hybrid-Hell",
    "Exakt 9",
    "7x-Chain-Ziel",
    "Board-Explosion",
    "Tempo-Uhr",
    "Alles-14-Linien",
    "Finale-Chaos",
    "Item-Maschine",
    "30k-Mark",
    "Ueberleben (Hard)",
    "Hybrid-15",
    "Triple-Ziel",
    "Praezision-Extrem",
    "Hard-Capstone",
    "Geschwindigkeit-Hoelle",
    "Muell-Apokalypse",
    "Chaos-Kern",
    "20-Sekunden-Deal",
    "Herzen-Marathon",
    "Schaedel-Albtraum",
    "Saubermacher-Infinity",
    "8x-Chain-Legendary",
    "50-Sekunden-Inferno",
    "Pyramiden-Mega",
    "Muell-Apokalypse-2",
    "Item-Chaos-Ballett",
    "50k-Punkte",
    "Ueberlebens-Legendaer",
    "Ultra-Kombo",
    "Speed-9-Meister",
    "Schaedel-Paradies",
    "30-Sekunden-Legendaer",
    "Hybrid-Supreme",
    "Exakt 20",
    "9x-Chain-Ultimum",
    "Board-Raum",
    "240-Sekunden-Legendaer",
    "Alles-oder-Nichts",
    "Finale-Ultimum",
    "Item-Koloss",
    "Chaos-Meister",
    "60k-Punkte",
    "Ueberlebens-Ultimate",
    "All-Meister",
    "Exakt 24",
    "Legendaer-Final"
};

static u8 challenge_abs_diff(u8 a, u8 b) {
    return (a > b) ? (a - b) : (b - a);
}

static void challenge_build_translation_table(void) {
    u8 x;
    u8 y;
    u8 next_level = 0;
    u8 distance;

    for (x = 0; x < CHALLENGE_LEVEL_COUNT; x++) {
        grid_to_challenge_level[x] = CHALLENGE_LEVEL_INVALID;
    }

    for (distance = 0; distance < 32 && next_level < CHALLENGE_LEVEL_COUNT; distance++) {
        for (y = 0; y < CHALLENGE_GRID_HEIGHT; y++) {
            for (x = 0; x < CHALLENGE_GRID_WIDTH; x++) {
                if ((challenge_abs_diff(x, 0) + challenge_abs_diff(y, 2)) == distance) {
                    grid_to_challenge_level[y * CHALLENGE_GRID_WIDTH + x] = next_level;
                    next_level++;
                }
            }
        }
    }

    challenge_translation_ready = TRUE;
}

/* Bit manipulation helpers */
static inline u8 bit_get(u32 *bitmap, u8 index) {
    u8 word_idx = index >> 5;
    u8 bit_idx = index & 31;
    return (bitmap[word_idx] >> bit_idx) & 1;
}

static inline void bit_set(u32 *bitmap, u8 index) {
    u8 word_idx = index >> 5;
    u8 bit_idx = index & 31;
    bitmap[word_idx] |= (1UL << bit_idx);
}

static inline void bit_clr(u32 *bitmap, u8 index) {
    u8 word_idx = index >> 5;
    u8 bit_idx = index & 31;
    bitmap[word_idx] &= ~(1UL << bit_idx);
}

static inline u8 challenge_is_open(u8 level_id) {
    return challenge_is_unlocked(level_id) || challenge_is_cleared(level_id);
}

static inline u8 challenge_has_link(u8 a, u8 b) {
    /* Avoid clutter: only link open nodes if at least one side is already cleared. */
    return challenge_is_open(a) && challenge_is_open(b) &&
           (challenge_is_cleared(a) || challenge_is_cleared(b));
}

static void challenge_rebuild_unlocked_from_cleared(void) {
    u8 level_id;

    config.challenge_unlocked[0] = 0;
    config.challenge_unlocked[1] = 0;
    config.challenge_unlocked[2] = 0;
    config.challenge_unlocked[3] = 0;

    /* Keep one deterministic entry point always available. */
    bit_set(config.challenge_unlocked, (u8)(2 * CHALLENGE_GRID_WIDTH));

    for (level_id = 0; level_id < CHALLENGE_LEVEL_COUNT; level_id++) {
        if (challenge_is_cleared(level_id)) {
            bit_set(config.challenge_unlocked, level_id);

            /* Expand unlock net around every cleared level. */
            if ((level_id & 15) > 0) bit_set(config.challenge_unlocked, (u8)(level_id - 1));
            if ((level_id & 15) < 15) bit_set(config.challenge_unlocked, (u8)(level_id + 1));
            if ((level_id >> 4) > 0) bit_set(config.challenge_unlocked, (u8)(level_id - CHALLENGE_GRID_WIDTH));
            if ((level_id >> 4) < 7) bit_set(config.challenge_unlocked, (u8)(level_id + CHALLENGE_GRID_WIDTH));
        }
    }
}

static bool challenge_try_move(s8 dx, s8 dy) {
    s16 nx = (s16)ctx->cursor_x + dx;
    s16 ny = (s16)ctx->cursor_y + dy;

    if (nx < 0 || nx > 15 || ny < 0 || ny > 7) {
        return FALSE;
    }

    ctx->cursor_x = (u8)nx;
    ctx->cursor_y = (u8)ny;
    ctx->current_level_id = (ctx->cursor_y * 16) + ctx->cursor_x;
    ctx->needsRedraw = TRUE;
    SOUND_play(SND_MOVE);
    return TRUE;
}

static void challenge_apply_dpad_repeat(bool holdLeft, bool holdRight, bool holdUp, bool holdDown,
                                        bool pressLeft, bool pressRight, bool pressUp, bool pressDown) {
    bool has_hold;

    if (pressLeft) {
        challenge_try_move(-1, 0);
        ctx->holdDir = -1;
        ctx->holdTimer = 0;
        ctx->holdNextThreshold = config.thresholdLRInitial;
        return;
    }
    if (pressRight) {
        challenge_try_move(1, 0);
        ctx->holdDir = 1;
        ctx->holdTimer = 0;
        ctx->holdNextThreshold = config.thresholdLRInitial;
        return;
    }
    if (pressUp) {
        challenge_try_move(0, -1);
        ctx->holdDir = -2;
        ctx->holdTimer = 0;
        ctx->holdNextThreshold = config.thresholdLRInitial;
        return;
    }
    if (pressDown) {
        challenge_try_move(0, 1);
        ctx->holdDir = 2;
        ctx->holdTimer = 0;
        ctx->holdNextThreshold = config.thresholdLRInitial;
        return;
    }

    has_hold = holdLeft || holdRight || holdUp || holdDown;
    if (!has_hold) {
        ctx->holdDir = 0;
        ctx->holdTimer = 0;
        return;
    }

    if ((ctx->holdDir == -1 && !holdLeft) ||
        (ctx->holdDir == 1 && !holdRight) ||
        (ctx->holdDir == -2 && !holdUp) ||
        (ctx->holdDir == 2 && !holdDown)) {
        if (holdLeft) ctx->holdDir = -1;
        else if (holdRight) ctx->holdDir = 1;
        else if (holdUp) ctx->holdDir = -2;
        else if (holdDown) ctx->holdDir = 2;
        else ctx->holdDir = 0;

        ctx->holdTimer = 0;
        ctx->holdNextThreshold = config.thresholdLRInitial;
    }

    if (ctx->holdDir == 0) return;

    ctx->holdTimer++;
    if (ctx->holdTimer < ctx->holdNextThreshold) return;

    ctx->holdTimer = 0;
    ctx->holdNextThreshold = config.thresholdLRRepeat;

    if (ctx->holdDir == -1) challenge_try_move(-1, 0);
    else if (ctx->holdDir == 1) challenge_try_move(1, 0);
    else if (ctx->holdDir == -2) challenge_try_move(0, -1);
    else if (ctx->holdDir == 2) challenge_try_move(0, 1);
}

static void challenge_rebuild_frontier(void) {
    u8 i;

    for (i = 0; i < 4; i++) {
        ctx->frontier_open[i] = 0;
    }

    for (i = 0; i < 128; i++) {
        u8 x;
        u8 y;
        u8 has_cleared_neighbor = FALSE;

        if (challenge_is_cleared(i)) continue;

        x = i & 15;
        y = i >> 4;

        if (x > 0 && challenge_is_cleared((u8)(i - 1))) has_cleared_neighbor = TRUE;
        if (x < 15 && challenge_is_cleared((u8)(i + 1))) has_cleared_neighbor = TRUE;
        if (y > 0 && challenge_is_cleared((u8)(i - 16))) has_cleared_neighbor = TRUE;
        if (y < 7 && challenge_is_cleared((u8)(i + 16))) has_cleared_neighbor = TRUE;

        if (has_cleared_neighbor) {
            bit_set(ctx->frontier_open, i);
        }
    }
}

u8 challenge_is_unlocked(u8 level_id) {
    if (level_id >= 128) return 0;
    return bit_get(config.challenge_unlocked, level_id);
}

u8 challenge_is_cleared(u8 level_id) {
    if (level_id >= 128) return 0;
    return bit_get(config.challenge_cleared, level_id);
}

u8 challenge_translate_grid_to_level(u8 grid_id) {
    if (grid_id >= CHALLENGE_LEVEL_COUNT) return CHALLENGE_LEVEL_INVALID;
    if (!challenge_translation_ready) {
        challenge_build_translation_table();
    }
    return grid_to_challenge_level[grid_id];
}

const char* challenge_get_level_name(u8 challenge_level_id) {
    if (challenge_level_id >= CHALLENGE_LEVEL_COUNT) return "???";
    return challenge_level_names[challenge_level_id];
}

void challenge_mark_cleared(u8 level_id) {
    if (level_id >= 128) return;
    
    /* Mark as cleared */
    bit_set(config.challenge_cleared, level_id);
    
    /* Unlock neighbors: up, down, left, right */
    u8 x = level_id % 16;
    u8 y = level_id / 16;
    
    /* Up */
    if (y > 0) bit_set(config.challenge_unlocked, (y-1)*16 + x);
    /* Down */
    if (y < 7) bit_set(config.challenge_unlocked, (y+1)*16 + x);
    /* Left */
    if (x > 0) bit_set(config.challenge_unlocked, y*16 + (x-1));
    /* Right */
    if (x < 15) bit_set(config.challenge_unlocked, y*16 + (x+1));
}

void challenge_init(void) {
    ctx = &sctx->challenge;
    challenge_build_translation_table();

    /* Test-start point: 3rd row, 1st column (1-based) => x=0, y=2 */
    ctx->cursor_x = 0;
    ctx->cursor_y = 2;
    ctx->current_level_id = (ctx->cursor_y * 16) + ctx->cursor_x;
    ctx->needsRedraw = TRUE;
    ctx->holdDir = 0;
    ctx->holdTimer = 0;
    ctx->holdNextThreshold = config.thresholdLRInitial;
    ctx->frontier_open[0] = 0;
    ctx->frontier_open[1] = 0;
    ctx->frontier_open[2] = 0;
    ctx->frontier_open[3] = 0;

    /* Progress is loaded in main/save_manager. Rebuild current unlock net from persisted 4x32-bit cleared map. */
    challenge_rebuild_unlocked_from_cleared();

    challenge_rebuild_frontier();
    
    /* Clear VDP and prepare screen */
    VDP_clearPlane(BG_A, TRUE);
    VDP_setTextPalette(PAL0);
}

void challenge_init_draw(void) {
    if (ctx == NULL) return;

    UI_init_fonts_and_palettes();

    VDP_clearTextArea(0, 0, 40, 28);
    VDP_setTextPalette(PAL2);
    VDP_drawText("--- CHALLENGE ---", 12, 2);
    VDP_setTextPalette(PAL3);
    VDP_drawText("A: CLEAR  C: SAVE+BACK", 6, 27);
    ctx->needsRedraw = TRUE;
}

void challenge_draw(void) {
    if (ctx == NULL || !ctx->needsRedraw) return;

    u8 x, y;
    u8 level_id;
    char cell;
    char cell_txt[2];
    u16 screen_x, screen_y;
    
    /* Centered: 16 cols * (char+space) with horizontal/vertical connectors */
    u16 base_screen_y = 6;
    u16 base_screen_x = 4;

    /* Draw 16x8 grid with spacing */
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 16; x++) {
            level_id = y * 16 + x;
            
            /* Determine cell character */
            if (challenge_is_cleared(level_id)) {
                cell = 'X';
            } else if (challenge_is_unlocked(level_id)) {
                cell = 'O';
            } else {
                cell = '.';
            }
            
            /* Screen position: 2 chars per cell (char + connector slot) */
            screen_x = base_screen_x + x * 2;
            screen_y = base_screen_y + y * 2;

            cell_txt[0] = cell;
            cell_txt[1] = '\0';
            
            /* Draw cell character */
            if (x == ctx->cursor_x && y == ctx->cursor_y) {
                /* Cursor: red text */
                VDP_setTextPalette(PAL2);  /* Red palette */
                VDP_drawText(cell_txt, screen_x, screen_y);
            } else if (cell == 'X') {
                VDP_setTextPalette(PAL0);  /* Green */
                VDP_drawText(cell_txt, screen_x, screen_y);
            } else if (cell == 'O') {
                VDP_setTextPalette(PAL1);  /* Yellow */
                VDP_drawText(cell_txt, screen_x, screen_y);
            } else {
                VDP_setTextPalette(PAL3);  /* White */
                VDP_drawText(cell_txt, screen_x, screen_y);
            }
            
            /* Horizontal connector to right neighbor */
            if (x < 15) {
                u8 right_id = level_id + 1;
                if (challenge_has_link(level_id, right_id)) {
                    VDP_setTextPalette(PAL0);
                    VDP_drawText("-", screen_x + 1, screen_y);
                } else {
                    VDP_drawText(" ", screen_x + 1, screen_y);
                }
            }

            /* Vertical connector to bottom neighbor */
            if (y < 7) {
                u8 down_id = level_id + 16;
                if (challenge_has_link(level_id, down_id)) {
                    VDP_setTextPalette(PAL0);
                    VDP_drawText("I", screen_x, screen_y + 1);
                } else {
                    VDP_drawText(" ", screen_x, screen_y + 1);
                }
            }
        }
    }
    
    /* Draw info at bottom */
    {
        char info[40];
        u8 translated_level = challenge_translate_grid_to_level(ctx->current_level_id);
        u16 display_number = (translated_level == CHALLENGE_LEVEL_INVALID) ? 0 : (translated_level + 1);
        const char* level_name = challenge_is_open(ctx->current_level_id)
            ? challenge_get_level_name(translated_level)
            : "???";

        VDP_clearTextArea(0, 24, 40, 2);
        VDP_setTextPalette(PAL3);
        sprintf(info, "%03d: %s", display_number, level_name);
        VDP_drawText(info, 2, 24);
        VDP_drawText("A: CLEAR  C: SAVE+BACK", 6, 27);
    }

    ctx->needsRedraw = FALSE;
}

void challenge_update(void) {
    if (ctx == NULL) return;

    bool holdLeft = (joyState & BUTTON_LEFT);
    bool holdRight = (joyState & BUTTON_RIGHT);
    bool holdUp = (joyState & BUTTON_UP);
    bool holdDown = (joyState & BUTTON_DOWN);

    bool pressLeft = (joyState & BUTTON_LEFT) && !(lastJoyState & BUTTON_LEFT);
    bool pressRight = (joyState & BUTTON_RIGHT) && !(lastJoyState & BUTTON_RIGHT);
    bool pressUp = (joyState & BUTTON_UP) && !(lastJoyState & BUTTON_UP);
    bool pressDown = (joyState & BUTTON_DOWN) && !(lastJoyState & BUTTON_DOWN);

    bool goLeft = (joyState & BUTTON_LEFT) && !(lastJoyState & BUTTON_LEFT);
    bool goRight = (joyState & BUTTON_RIGHT) && !(lastJoyState & BUTTON_RIGHT);
    bool goUp = (joyState & BUTTON_UP) && !(lastJoyState & BUTTON_UP);
    bool goDown = (joyState & BUTTON_DOWN) && !(lastJoyState & BUTTON_DOWN);
    bool pressedA = (joyState & BUTTON_A) && !(lastJoyState & BUTTON_A);
    bool pressedC = (joyState & BUTTON_C) && !(lastJoyState & BUTTON_C);

    (void)goLeft;
    (void)goRight;
    (void)goUp;
    (void)goDown;

    challenge_apply_dpad_repeat(holdLeft, holdRight, holdUp, holdDown,
                                pressLeft, pressRight, pressUp, pressDown);
    
    /* Handle A button */
    if (pressedA) {
        u8 level_id = ctx->current_level_id;
        
        if (!challenge_is_unlocked(level_id)) {
            SOUND_play(SND_ALERT);
        } else {
            if (!challenge_is_cleared(level_id)) {
                challenge_mark_cleared(level_id);
                challenge_rebuild_frontier();
                ctx->needsRedraw = TRUE;
                SOUND_play(SND_LINE_CLEAR);
            } else {
                SOUND_play(SND_MOVE);
            }
        }
    }

    if (pressedC) {
        SOUND_play(SND_MENU_SELECT);
        config.sramop = SRAM_SAVE;
        config.preferredState = STATE_TITLE;
        currentState = STATE_SAVE;
    }
}

void challenge_cleanup(void) {
    VDP_clearTextArea(0, 0, 40, 28);
    ctx = NULL;
}
