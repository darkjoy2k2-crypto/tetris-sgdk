#pragma once

#include <genesis.h>

// Mapping der Spiel-Ereignisse auf deine WAV-Nummern (1-99)
typedef enum {
    SND_MOVE        = 21,   // 65 Entspricht WAV_001
    SND_ROTATE      = 22,   // Entspricht WAV_002
    SND_SOFT_DROP   = 15,
    SND_HARD_DROP   = 16,
    SND_PIECE_LOCK  = 18,
    SND_LINE_CLEAR  = 91,  // Beispiel: Ab 10 kommen die Erfolgs-Sounds
    SND_TETRIS      = 36,
    SND_LEVEL_UP    = 90,
    SND_GAME_OVER   = 40,
    SND_HOLD        = 13,
    SND_COMBO       = 89,
    SND_GARBAGE     = 99
} SoundEvent;

/**
 * Initialisiert den XGM2 Treiber
 */
void SOUND_init();

/**
 * Spielt einen Sound basierend auf dem Event ab.
 * Nutzt intern den XGM2-Treiber für 4-Kanal-Mixing.
 */
void SOUND_play(SoundEvent event);

/**
 * Startet die Hintergrundmusik (XGM/XGM2 Format)
 */
void SOUND_playMusic();