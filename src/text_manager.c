#include "text_manager.h"

#include <string.h>

#include "sprite.h"
#include "sprites.h"

#define TEXT_MANAGER_MAX_WORDS 4
#define TITLE_WORD_COUNT       3
#define HIGHSCORE_WORD_COUNT   1
#define VS_COUNTDOWN_WORD_COUNT 4
#define TITLE_WORD_MAX_LEN     12
#define TITLE_TEXT_GAP_PX      -2
#define TITLE_SCREEN_W_NTSC    320
#define TITLE_SCREEN_H_NTSC    224
#define TITLE_SCREEN_H_PAL     240
#define TITLE_FLY_SPEED_RTL    16
#define TITLE_FLY_SPEED_LTR    16
#define TITLE_WOBBLE_AMP_PX    5
#define TITLE_WOBBLE_STEP      14
#define TITLE_LINE_STEP_Y      32
#define TITLE_START_Y          48
#define TITLE_HOLD_ALL_TICKS   180
#define TITLE_FALL_DELAY_STEP  2
#define TITLE_FALL_PAIR_SIZE   2
#define HIGHSCORE_TEXT_Y        160
#define HIGHSCORE_HOLD_TICKS    60
#define HIGHSCORE_START_DELAY   30
#define VS_WINNER_TEXT_Y        112
#define VS_WINNER_HOLD_TICKS    60
#define VS_COUNTDOWN_TEXT_Y     112
#define VS_COUNTDOWN_HOLD_TICKS 20
#define VS_COUNTDOWN_START_DELAY 8

typedef enum TextManagerScene {
    TEXT_SCENE_NONE = 0,
    TEXT_SCENE_TITLE,
    TEXT_SCENE_HIGHSCORE,
    TEXT_SCENE_VS_WINNER,
    TEXT_SCENE_VS_COUNTDOWN
} TextManagerScene;

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
    u8 depth;
    s16 width;
    s16 x;
    s16 centerX;
    s16 baseY;
    u16 wavePhase;
    bool nextTriggered;
    bool priority;
    TitleWordPhase phase;
} TitleWordState;

typedef struct FallingGlyph {
    bool active;
    char c;
    u8 depth;
    s16 x;
    s16 y;
    s16 vx;
    s16 vy;
    u16 delay;
    bool priority;
} FallingGlyph;

static const char* titleWords[TITLE_WORD_COUNT] = {
    "tetris",
    "vibe",
    "sgdk"
};

static const char* highscoreWords[HIGHSCORE_WORD_COUNT] = {
    "highscores"
};

static char vsWinnerText[TITLE_WORD_MAX_LEN + 1] = "DRAW";

static const char* vsWinnerWords[1] = {
    vsWinnerText
};

static const char* vsCountdownWords[VS_COUNTDOWN_WORD_COUNT] = {
    "3",
    "2",
    "1",
    "GO!!!"
};

static const bool titleWordPriorities[TITLE_WORD_COUNT] = {
    PRIO_LOW,
    PRIO_HIGH,
    PRIO_HIGH
};

static const bool highscoreWordPriorities[HIGHSCORE_WORD_COUNT] = {
    PRIO_HIGH
};

static const bool vsWinnerWordPriorities[1] = {
    PRIO_HIGH
};

static const bool vsCountdownWordPriorities[VS_COUNTDOWN_WORD_COUNT] = {
    PRIO_HIGH,
    PRIO_HIGH,
    PRIO_HIGH,
    PRIO_HIGH
};

static const u8 titleWordDepths[TITLE_WORD_COUNT] = {
    6,
    3,
    0
};

static const u8 highscoreWordDepths[HIGHSCORE_WORD_COUNT] = {
    DEPTH_FOREGROUND
};

static const u8 vsWinnerWordDepths[1] = {
    DEPTH_FOREGROUND
};

static const u8 vsCountdownWordDepths[VS_COUNTDOWN_WORD_COUNT] = {
    DEPTH_FOREGROUND,
    DEPTH_FOREGROUND,
    DEPTH_FOREGROUND,
    DEPTH_FOREGROUND
};

static TitleWordState words[TEXT_MANAGER_MAX_WORDS];
static FallingGlyph falling[TITLE_TEXT_MAX_CHARS];
static u8 activeWordCount = 0;
static u8 totalSlots = 0;
static bool wordEnabled = FALSE;
static bool wordInitialized = FALSE;
static bool finaleStarted = FALSE;
static bool exitRequested = FALSE;
static u16 allHoldTimer = 0;
static u16 holdTicksTarget = TITLE_HOLD_ALL_TICKS;
static u16 startDelayTicks = 0;
static s16 screenH = TITLE_SCREEN_H_NTSC;
static TextManagerScene activeScene = TEXT_SCENE_NONE;
static u8 countdownWordIndex = 0;
static bool sceneFinished = FALSE;

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

        sprites_text_set_glyph(slot, c, penX, (s16)(w->baseY + wobbleY), w->priority, w->depth, TRUE);
        penX += text_manager_glyph_width(c) + TITLE_TEXT_GAP_PX;
    }
}

static void text_manager_start_word_rtl(u8 wordIndex) {
    TitleWordState* w = &words[wordIndex];

    w->phase = TITLE_WORD_RTL;
    w->x = TITLE_SCREEN_W_NTSC;
    w->wavePhase = 0;
}

static void text_manager_start_word_ltr(u8 wordIndex) {
    TitleWordState* w = &words[wordIndex];

    w->phase = TITLE_WORD_LTR;
    w->x = (s16)(-w->width - 48);
    w->wavePhase = 0;
}

static void text_manager_start_word_center(u8 wordIndex) {
    TitleWordState* w = &words[wordIndex];

    w->phase = TITLE_WORD_HOLD;
    w->x = w->centerX;
    w->wavePhase = 0;
    allHoldTimer = 0;
}

static bool text_manager_has_active_falling(void) {
    for (u8 i = 0; i < totalSlots; i++) {
        if (falling[i].active) return TRUE;
    }

    return FALSE;
}

static bool text_manager_all_words_holding(void) {
    if (activeWordCount == 0) return FALSE;

    for (u8 i = 0; i < activeWordCount; i++) {
        if (words[i].phase != TITLE_WORD_HOLD) return FALSE;
    }

    return TRUE;
}

static void text_manager_start_finale(void) {
    u8 slot = 0;

    if (finaleStarted) return;

    finaleStarted = TRUE;

    for (u8 i = 0; i < activeWordCount; i++) {
        TitleWordState* w = &words[i];
        s16 penX = w->centerX;

        if (w->phase == TITLE_WORD_OFF || w->phase == TITLE_WORD_DONE) continue;

        w->phase = TITLE_WORD_DONE;

        for (u8 j = 0; j < w->len; j++) {
            s16 wobbleY = F16_toInt(F16_mul(FIX16(TITLE_WOBBLE_AMP_PX), sinFix16((w->wavePhase + (j << 6)) & 1023)));
            FallingGlyph* fg = &falling[slot];

            fg->active = TRUE;
            fg->c = w->text[j];
            fg->depth = w->depth;
            fg->x = penX;
            fg->y = (s16)(w->baseY + wobbleY);
            fg->vx = (s16)((random() % 3) - 1);
            fg->vy = 0;
            fg->delay = (u16)(((slot / TITLE_FALL_PAIR_SIZE) * TITLE_FALL_DELAY_STEP));
            fg->priority = w->priority;

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

        sprites_text_set_glyph(i, fg->c, fg->x, fg->y, fg->priority, fg->depth, TRUE);
    }
}

static void text_manager_reset_common(u16 holdTicks) {
    PAL_setPalette(PAL2, anim_norotate.palette->data, CPU);

    sprites_text_set_enabled(FALSE);
    sprites_init_text_only();
    sprites_text_clear();

    screenH = (s16)(IS_PAL_SYSTEM ? TITLE_SCREEN_H_PAL : TITLE_SCREEN_H_NTSC);
    activeWordCount = 0;
    totalSlots = 0;
    finaleStarted = FALSE;
    exitRequested = FALSE;
    allHoldTimer = 0;
    holdTicksTarget = holdTicks;
    startDelayTicks = 0;
    activeScene = TEXT_SCENE_NONE;
    countdownWordIndex = 0;
    sceneFinished = FALSE;
    wordEnabled = FALSE;
    wordInitialized = FALSE;

    for (u8 i = 0; i < TITLE_TEXT_MAX_CHARS; i++) {
        falling[i].active = FALSE;
    }
}

static void text_manager_configure_words(const char* const* wordList, const bool* priorities, const u8* depths, u8 wordCount, s16 startY, s16 lineStepY) {
    activeWordCount = wordCount;
    totalSlots = 0;

    for (u8 i = 0; i < wordCount; i++) {
        TitleWordState* w = &words[i];
        const char* text = wordList[i];
        u8 len = 0;

        while (text[len] != '\0' && len < TITLE_WORD_MAX_LEN) len++;

        w->text = text;
        w->len = len;
        w->startSlot = totalSlots;
        w->depth = depths[i];
        w->width = text_manager_word_width(text, len);
        w->centerX = (s16)((TITLE_SCREEN_W_NTSC - w->width) >> 1);
        w->x = TITLE_SCREEN_W_NTSC;
        w->baseY = (s16)(startY + (i * lineStepY));
        w->wavePhase = 0;
        w->nextTriggered = FALSE;
        w->priority = priorities[i];
        w->phase = TITLE_WORD_OFF;

        totalSlots = (u8)(totalSlots + len);
    }

    if (totalSlots == 0 || totalSlots > TITLE_TEXT_MAX_CHARS) {
        wordEnabled = FALSE;
        wordInitialized = FALSE;
        return;
    }

    wordEnabled = TRUE;
    wordInitialized = TRUE;
    sprites_text_set_enabled(TRUE);
}

static void text_manager_update_title_scene(void) {
    if (!finaleStarted) {
        for (u8 i = 0; i < activeWordCount; i++) {
            TitleWordState* w = &words[i];

            if (w->phase == TITLE_WORD_OFF || w->phase == TITLE_WORD_DONE) continue;

            w->wavePhase = (u16)((w->wavePhase + TITLE_WOBBLE_STEP) & 1023);

            if (w->phase == TITLE_WORD_RTL) {
                w->x -= TITLE_FLY_SPEED_RTL;
                if ((w->x + w->width) < 0) {
                    w->phase = TITLE_WORD_LTR;
                    w->x = (s16)(-w->width);

                    if (!w->nextTriggered && i + 1 < activeWordCount) {
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
            if (allHoldTimer >= holdTicksTarget) {
                text_manager_start_finale();
            }
        } else {
            allHoldTimer = 0;
        }
    } else {
        text_manager_update_finale();
    }
}

static void text_manager_update_highscore_scene(void) {
    TitleWordState* w;

    if (activeWordCount == 0) return;

    if (finaleStarted) {
        text_manager_update_finale();
        return;
    }

    w = &words[0];
    if (w->phase == TITLE_WORD_OFF || w->phase == TITLE_WORD_DONE) return;

    w->wavePhase = (u16)((w->wavePhase + TITLE_WOBBLE_STEP) & 1023);

    if (w->phase == TITLE_WORD_LTR) {
        w->x += TITLE_FLY_SPEED_LTR;
        if (w->x >= w->centerX) {
            w->x = w->centerX;
            w->phase = TITLE_WORD_HOLD;
            allHoldTimer = 0;
        }
    } else if (w->phase == TITLE_WORD_HOLD) {
        if (allHoldTimer < holdTicksTarget) allHoldTimer++;

        if (exitRequested && allHoldTimer >= holdTicksTarget) {
            text_manager_start_finale();
        }
    }

    if (!finaleStarted) {
        text_manager_render_word(0);
    }
}

static void text_manager_update_vs_countdown_scene(void) {
    TitleWordState* w;

    if (activeWordCount == 0 || sceneFinished) return;

    if (countdownWordIndex >= activeWordCount) {
        sceneFinished = TRUE;
        return;
    }

    if (finaleStarted) {
        text_manager_update_finale();

        if (!text_manager_has_active_falling()) {
            finaleStarted = FALSE;
            allHoldTimer = 0;
            countdownWordIndex++;

            if (countdownWordIndex < activeWordCount) {
                text_manager_start_word_center(countdownWordIndex);
            } else {
                sceneFinished = TRUE;
            }
        }
        return;
    }

    w = &words[countdownWordIndex];
    if (w->phase == TITLE_WORD_OFF || w->phase == TITLE_WORD_DONE) {
        text_manager_start_word_center(countdownWordIndex);
        w = &words[countdownWordIndex];
    }

    w->wavePhase = (u16)((w->wavePhase + TITLE_WOBBLE_STEP) & 1023);
    text_manager_render_word(countdownWordIndex);

    if (allHoldTimer < holdTicksTarget) {
        allHoldTimer++;
    } else {
        text_manager_start_finale();
    }
}

void text_manager_init(void) {
    text_manager_reset_common(TITLE_HOLD_ALL_TICKS);
    activeScene = TEXT_SCENE_TITLE;
    text_manager_configure_words(titleWords, titleWordPriorities, titleWordDepths, TITLE_WORD_COUNT, TITLE_START_Y, TITLE_LINE_STEP_Y);

    if (!wordInitialized) return;

    text_manager_start_word_rtl(0);
}

void text_manager_init_highscore(void) {
    text_manager_reset_common(HIGHSCORE_HOLD_TICKS);
    activeScene = TEXT_SCENE_HIGHSCORE;
    startDelayTicks = HIGHSCORE_START_DELAY;
    text_manager_configure_words(highscoreWords, highscoreWordPriorities, highscoreWordDepths, HIGHSCORE_WORD_COUNT, HIGHSCORE_TEXT_Y, 0);

    if (!wordInitialized) return;

    text_manager_start_word_ltr(0);
}

void text_manager_init_vs_countdown(void) {
    text_manager_reset_common(VS_COUNTDOWN_HOLD_TICKS);
    activeScene = TEXT_SCENE_VS_COUNTDOWN;
    startDelayTicks = VS_COUNTDOWN_START_DELAY;
    countdownWordIndex = 0;
    text_manager_configure_words(vsCountdownWords, vsCountdownWordPriorities, vsCountdownWordDepths, VS_COUNTDOWN_WORD_COUNT, VS_COUNTDOWN_TEXT_Y, 0);

    if (!wordInitialized) return;

    text_manager_start_word_center(0);
}

void text_manager_init_vs_winner(const char* text) {
    text_manager_reset_common(VS_WINNER_HOLD_TICKS);
    activeScene = TEXT_SCENE_VS_WINNER;

    strncpy(vsWinnerText, (text != NULL) ? text : "DRAW", TITLE_WORD_MAX_LEN);
    vsWinnerText[TITLE_WORD_MAX_LEN] = '\0';

    text_manager_configure_words(vsWinnerWords, vsWinnerWordPriorities, vsWinnerWordDepths, 1, VS_WINNER_TEXT_Y, 0);

    if (!wordInitialized) return;

    text_manager_start_word_ltr(0);
}

void text_manager_glyphs_visible(bool state) {
    sprites_text_set_enabled(state);

    if (!state) {
        sprites_text_clear();
        sprites_update();
    }
}

void text_manager_set_enabled(bool enabled) {
    wordEnabled = enabled;
    sprites_text_set_enabled(enabled);
}

void text_manager_request_exit(void) {
    exitRequested = TRUE;
}

void text_manager_update(void) {
    if (!wordInitialized) return;

    sprites_text_clear();

    if (!wordEnabled) {
        sprites_update();
        return;
    }

    if (startDelayTicks > 0) {
        startDelayTicks--;
        sprites_update();
        return;
    }

    if (activeScene == TEXT_SCENE_TITLE) {
        text_manager_update_title_scene();
    } else if (activeScene == TEXT_SCENE_HIGHSCORE || activeScene == TEXT_SCENE_VS_WINNER) {
        text_manager_update_highscore_scene();
    } else if (activeScene == TEXT_SCENE_VS_COUNTDOWN) {
        text_manager_update_vs_countdown_scene();
    }

    sprites_update();
}

bool text_manager_is_finished(void) {
    if (!wordInitialized) return FALSE;

    if (activeScene == TEXT_SCENE_VS_COUNTDOWN) {
        return sceneFinished;
    }

    if (!finaleStarted) return FALSE;
    return !text_manager_has_active_falling();
}

void text_manager_cleanup(void) {
    sprites_text_set_enabled(FALSE);
    sprites_text_clear();
    sprites_update();
    sprites_cleanup();

    activeWordCount = 0;
    totalSlots = 0;
    wordEnabled = FALSE;
    wordInitialized = FALSE;
    finaleStarted = FALSE;
    exitRequested = FALSE;
    allHoldTimer = 0;
    activeScene = TEXT_SCENE_NONE;
    countdownWordIndex = 0;
    sceneFinished = FALSE;
}
