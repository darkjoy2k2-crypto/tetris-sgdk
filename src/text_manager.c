#include "text_manager.h"

#include "sprite.h"
#include "sprites.h"

#define TITLE_WORD_COUNT      3
#define TITLE_WORD_MAX_LEN    8
#define TITLE_TEXT_GAP_PX     2
#define TITLE_SCREEN_W_NTSC   320
#define TITLE_SCREEN_H_NTSC   224
#define TITLE_SCREEN_H_PAL    240
#define TITLE_FLY_SPEED_RTL   4
#define TITLE_FLY_SPEED_LTR   4
#define TITLE_WOBBLE_AMP_PX   5
#define TITLE_WOBBLE_STEP     14
#define TITLE_LINE_STEP_Y     32
#define TITLE_START_Y         40
#define TITLE_HOLD_ALL_TICKS  180
#define TITLE_FALL_DELAY_STEP 4
#define TITLE_FALL_PAIR_SIZE  2

typedef enum TitleWordPhase {
    TITLE_WORD_OFF = 0,
    TITLE_WORD_RTL,
    TITLE_WORD_LTR,
    TITLE_WORD_HOLD,
    TITLE_WORD_DONE
} TitleWordPhase;

typedef struct TitleWordState {
    const char* text;
    u8 len;
    u8 startSlot;
    s16 width;
    s16 x;
    s16 centerX;
    s16 baseY;
    u16 wavePhase;
    bool nextTriggered;
    TitleWordPhase phase;
} TitleWordState;

typedef struct FallingGlyph {
    bool active;
    char c;
    s16 x;
    s16 y;
    s16 vx;
    s16 vy;
    u16 delay;
} FallingGlyph;

static const char* titleWords[TITLE_WORD_COUNT] = {
    "tetris",
    "vibe",
    "sgdk"
};

static TitleWordState words[TITLE_WORD_COUNT];
static FallingGlyph falling[TITLE_TEXT_MAX_CHARS];
static u8 totalSlots = 0;
static bool wordEnabled = FALSE;
static bool wordInitialized = FALSE;
static bool finaleStarted = FALSE;
static u16 allHoldTimer = 0;
static s16 screenH = TITLE_SCREEN_H_NTSC;

static u8 text_manager_glyph_width(char c) {
    if (c == 'M' || c == 'm' || c == 'W' || c == 'w') return 48;
    if (c == 'I' || c == 'i' || c == '!' || c == '.' || c == ';') return 16;
    return 32;
}

static s16 text_manager_word_width(const char* text, u8 len) {
    s16 total = 0;
    for (u8 i = 0; i < len; i++) {
        total += text_manager_glyph_width(text[i]);
        if (i + 1 < len) total += TITLE_TEXT_GAP_PX;
    }
    return total;
}

static void text_manager_render_word(u8 wordIndex) {
    TitleWordState* w = &words[wordIndex];
    s16 penX = w->x;

    if (w->phase == TITLE_WORD_OFF || w->phase == TITLE_WORD_DONE) return;

    for (u8 i = 0; i < w->len; i++) {
        u8 glyphIndex = i;
        u8 slot = (u8)(w->startSlot + i);
        char c;
        s16 wobbleY;

        if (w->phase == TITLE_WORD_RTL) {
            glyphIndex = (u8)(w->len - 1 - i);
        }

        c = w->text[glyphIndex];
        wobbleY = F16_toInt(F16_mul(FIX16(TITLE_WOBBLE_AMP_PX), sinFix16((w->wavePhase + (i << 6)) & 1023)));

        sprites_text_set_glyph(slot, c, penX, (s16)(w->baseY + wobbleY), TRUE);
        penX += text_manager_glyph_width(c) + TITLE_TEXT_GAP_PX;
    }
}

static void text_manager_start_word_rtl(u8 wordIndex) {
    TitleWordState* w = &words[wordIndex];

    w->phase = TITLE_WORD_RTL;
    w->x = TITLE_SCREEN_W_NTSC;
    w->wavePhase = 0;
}

static bool text_manager_all_words_holding(void) {
    for (u8 i = 0; i < TITLE_WORD_COUNT; i++) {
        if (words[i].phase != TITLE_WORD_HOLD) return FALSE;
    }
    return TRUE;
}

static void text_manager_start_finale(void) {
    u8 slot = 0;

    finaleStarted = TRUE;

    for (u8 i = 0; i < TITLE_WORD_COUNT; i++) {
        TitleWordState* w = &words[i];
        s16 penX = w->centerX;

        w->phase = TITLE_WORD_DONE;

        for (u8 j = 0; j < w->len; j++) {
            s16 wobbleY = F16_toInt(F16_mul(FIX16(TITLE_WOBBLE_AMP_PX), sinFix16((w->wavePhase + (j << 6)) & 1023)));
            FallingGlyph* fg = &falling[slot];

            fg->active = TRUE;
            fg->c = w->text[j];
            fg->x = penX;
            fg->y = (s16)(w->baseY + wobbleY);
            fg->vx = (s16)((random() % 3) - 1);
            fg->vy = 0;
            fg->delay = (u16)(((slot / TITLE_FALL_PAIR_SIZE) * TITLE_FALL_DELAY_STEP));

            penX += text_manager_glyph_width(w->text[j]) + TITLE_TEXT_GAP_PX;
            slot++;
        }
    }
}

static void text_manager_update_finale(void) {
    for (u8 i = 0; i < totalSlots; i++) {
        FallingGlyph* fg = &falling[i];

        if (!fg->active) continue;

        if (fg->delay > 0) {
            fg->delay--;
        } else {
            if (fg->vy < 14) fg->vy++;
            fg->x += fg->vx;
            fg->y += fg->vy;
        }

        if (fg->y > (screenH + 40)) {
            fg->active = FALSE;
            continue;
        }

        sprites_text_set_glyph(i, fg->c, fg->x, fg->y, TRUE);
    }
}

void text_manager_init(void) {
    PAL_setPalette(PAL2, anim_norotate.palette->data, CPU);

    sprites_init_text_only();
    sprites_text_clear();

    screenH = (s16)(IS_PAL_SYSTEM ? TITLE_SCREEN_H_PAL : TITLE_SCREEN_H_NTSC);
    totalSlots = 0;
    finaleStarted = FALSE;
    allHoldTimer = 0;

    for (u8 i = 0; i < TITLE_TEXT_MAX_CHARS; i++) {
        falling[i].active = FALSE;
    }

    for (u8 i = 0; i < TITLE_WORD_COUNT; i++) {
        TitleWordState* w = &words[i];
        const char* text = titleWords[i];
        u8 len = 0;

        while (text[len] != '\0' && len < TITLE_WORD_MAX_LEN) len++;

        w->text = text;
        w->len = len;
        w->startSlot = totalSlots;
        w->width = text_manager_word_width(text, len);
        w->centerX = (s16)((TITLE_SCREEN_W_NTSC - w->width) >> 1);
        w->x = TITLE_SCREEN_W_NTSC;
        w->baseY = (s16)(TITLE_START_Y + (i * TITLE_LINE_STEP_Y));
        w->wavePhase = 0;
        w->nextTriggered = FALSE;
        w->phase = TITLE_WORD_OFF;

        totalSlots = (u8)(totalSlots + len);
    }

    if (totalSlots == 0 || totalSlots > TITLE_TEXT_MAX_CHARS) {
        wordEnabled = FALSE;
        wordInitialized = FALSE;
        return;
    }

    text_manager_start_word_rtl(0);

    wordEnabled = TRUE;
    wordInitialized = TRUE;

    sprites_text_set_enabled(TRUE);
}

void text_manager_set_enabled(bool enabled) {
    wordEnabled = enabled;
    sprites_text_set_enabled(enabled);
}

void text_manager_update(void) {
    if (!wordInitialized) {
        return;
    }

    sprites_text_clear();

    if (!wordEnabled) {
        sprites_update();
        return;
    }

    if (!finaleStarted) {
        for (u8 i = 0; i < TITLE_WORD_COUNT; i++) {
            TitleWordState* w = &words[i];

            if (w->phase == TITLE_WORD_OFF || w->phase == TITLE_WORD_DONE) {
                continue;
            }

            w->wavePhase = (u16)((w->wavePhase + TITLE_WOBBLE_STEP) & 1023);

            if (w->phase == TITLE_WORD_RTL) {
                w->x -= TITLE_FLY_SPEED_RTL;
                if ((w->x + w->width) < 0) {
                    w->phase = TITLE_WORD_LTR;
                    w->x = (s16)(-w->width);

                    if (!w->nextTriggered && i + 1 < TITLE_WORD_COUNT) {
                        text_manager_start_word_rtl((u8)(i + 1));
                        w->nextTriggered = TRUE;
                    }
                }
            } else if (w->phase == TITLE_WORD_LTR) {
                w->x += TITLE_FLY_SPEED_LTR;
                if (w->x >= w->centerX) {
                    w->x = w->centerX;
                    w->phase = TITLE_WORD_HOLD;
                }
            }

            text_manager_render_word(i);
        }

        if (text_manager_all_words_holding()) {
            allHoldTimer++;
            if (allHoldTimer >= TITLE_HOLD_ALL_TICKS) {
                text_manager_start_finale();
            }
        } else {
            allHoldTimer = 0;
        }
    } else {
        text_manager_update_finale();
    }

    sprites_update();
}

bool text_manager_is_finished(void) {
    if (!wordInitialized || !finaleStarted) return FALSE;

    for (u8 i = 0; i < totalSlots; i++) {
        if (falling[i].active) return FALSE;
    }

    return TRUE;
}

void text_manager_cleanup(void) {
    if (!wordInitialized) return;

    sprites_text_set_enabled(FALSE);
    sprites_update();
    sprites_cleanup();

    totalSlots = 0;
    wordEnabled = FALSE;
    wordInitialized = FALSE;
}
