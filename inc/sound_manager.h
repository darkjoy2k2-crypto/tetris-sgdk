#pragma once

#include <genesis.h>

// Mapping der Spiel-Ereignisse auf deine WAV-Nummern (1-99)
typedef enum {
    SND_MENU_SELECT = 53,
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
    SND_COMBO       = 52,
    SND_GARBAGE     = 99,
    SND_GOOD_ITEM   = 15,
    SND_ALERT       = 87,
    SND_BAD_ITEM    = 15,  // Oder die nächste freie ID in deiner Liste
    SND_RESET       = 36
} SoundEvent;

/**
 * @brief Initialisiert den XGM2-Treiber.
 * @note Lädt den Z80-Treiber genau einmal.
 */
void SOUND_init();

/**
 * @brief Spielt einen Soundeffekt anhand eines Event-IDs ab.
 * @param event Die vordefinierte Sound-ID.
 * @note Nutzt intern den XGM2-Treiber für PCM-Mixing.
 */
void SOUND_play(SoundEvent event);

/**
 * @brief Startet die Standard-Hintergrundmusik.
 * @note Verwendet intern den aktuell bevorzugten XGM2-Track.
 */
void SOUND_playMusic();

/**
 * @brief Startet einen Musik-Track per numerischer ID.
 * @param id Musik-ID aus dem Sound-Test.
 * @warning Stoppt einen bereits laufenden Track vor dem Neustart.
 */
void SOUND_playMusicById(u16 id);

/**
 * @brief Stoppt die aktuell laufende Musik.
 */
void SOUND_stopMusic();

/**
 * @brief Liefert die Anzahl eingebundener Musik-Tracks.
 * @return Anzahl gültiger Musik-IDs.
 */
u16 SOUND_getMusicCount();

/**
 * @brief Liefert den Anzeigenamen einer Musik-ID.
 * @param id Musik-ID.
 * @return Kurzer Anzeigename für UI-Zwecke.
 */
const char* SOUND_getMusicName(u16 id);