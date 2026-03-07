#pragma once

#include <genesis.h>

// Einmaliges Setup beim Spielstart (lädt Tiles/Paletten)
void menu_bg_init();

// Schaltet den Hintergrund aktiv/inaktiv und kümmert sich um das Zeichnen/Löschen
void menu_bg_set_active(bool active);

// Führt die Animation aus (nur wenn aktiv)
void menu_bg_update();

