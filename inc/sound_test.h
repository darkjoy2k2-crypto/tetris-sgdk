#pragma once

#include <genesis.h>

/**
 * Initialisiert den Sound-Test (Speicher reservieren, UI zeichnen)
 */
void sound_test_init();

/**
 * Verarbeitet Eingaben (Links/Rechts/A)
 */
void sound_test_update();

/**
 * Gibt Speicher frei und säubert den Bildschirm
 */
void sound_test_cleanup();