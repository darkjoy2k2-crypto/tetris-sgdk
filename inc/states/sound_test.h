#pragma once

#include <genesis.h>

/**
 * Initialisiert den Sound-Test (Speicher reservieren, UI zeichnen)
 */
void sound_test_init();
void sound_test_init_draw();

/**
 * Verarbeitet Eingaben (Links/Rechts/A)
 */
void sound_test_update();
void sound_test_draw();

/**
 * Gibt Speicher frei und säubert den Bildschirm
 */
void sound_test_cleanup();