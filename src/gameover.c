#include <genesis.h>
#include "gameover.h"
#include "states.h"

// Lokaler Kontext für den GameOver-State
typedef struct {
    u16 timer;
} GameOverContext;

static GameOverContext* ctx = NULL;

void gameover_init() {
    // Speicher reservieren
    ctx = MEM_alloc(sizeof(GameOverContext));
    ctx->timer = 0;

    // Bildschirm säubern (vom Spielfeld des Game-States)
    VDP_clearTextArea(0, 0, 40, 28);
    
    // Zentrierte Texte anzeigen
    VDP_drawText("G A M E   O V E R", 12, 10);
    VDP_drawText("Press START to try again", 8, 14);
}

void gameover_update() {
    if (ctx == NULL) return;

    // Ein kleiner Timer, um versehentliches Überspringen 
    // direkt nach dem Game Over zu verhindern (Input-Delay)
    ctx->timer++;
    
    if (ctx->timer > 30) {
        u16 joy = JOY_readJoypad(JOY_1);
        
        if (joy & BUTTON_START) {
            // Zurück zum Titel-Bildschirm wechseln
            currentState = STATE_TITLE;
        }
    }
}

void gameover_cleanup() {
    // Bildschirm für den nächsten State leeren
    VDP_clearTextArea(0, 0, 40, 28);

    // Kontext-Speicher freigeben
    if (ctx != NULL) {
        MEM_free(ctx);
        ctx = NULL;
    }
}