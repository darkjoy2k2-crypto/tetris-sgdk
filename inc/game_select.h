#pragma once

#include <genesis.h>

/**
 * Initialisiert den Auswahlbildschirm (Speicher reservieren, UI zeichnen)
 */
void select_init();

/**
 * Verarbeitet die Menü-Navigation und das Umschalten der Optionen
 */
void select_update();

/**
 * Gibt den Speicher frei und säubert den Bildschirm für das eigentliche Spiel
 */
void select_cleanup();