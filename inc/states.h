#ifndef _STATES_H_
#define _STATES_H_

#include <genesis.h>

// 1. DER BAUPLAN (Die Struct muss zuerst kommen!)
typedef struct {
    void (*init)();
    void (*update)();
    void (*cleanup)();
} StateHandler;

// 2. DIE ZUSTÄNDE
typedef enum {
    STATE_NONE = 0,
    STATE_TITLE = 1,
    STATE_SELECT = 2,
    STATE_GAME = 3,
    STATE_SOUNDTEST = 4,
    STATE_GAMEOVER = 5
} GameState;

// 3. GLOBALE VARIABLEN (Extern-Deklarationen)
extern GameState currentState;
extern GameState lastState;
extern u16 joyState;
extern u16 lastJoyState;

#endif