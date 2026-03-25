#ifndef _BRAIN_H_
#define _BRAIN_H_

#include <genesis.h>

/**
 * Forward Declarations zur Vermeidung zirkulärer Includes.
 * Die Strukturen sind in vs_state.h und game_core.h definiert.
 */
typedef struct VsContext VsContext;
typedef struct GameContext GameContext;

/**
 * Initialisiert die KI-Variablen im VsContext.
 * Setzt Scores zurück und markiert den Planungsstatus als ungültig.
 */
void vs_brain_reset(VsContext* vctx);

/**
 * Führt die Bitboard-basierte Simulation durch.
 * Berechnet TargetX und TargetRot unter Einbeziehung von player->nextType (Lookahead).
 */
void vs_brain_think(VsContext* vctx, GameContext* player);

/**
 * Zentrale Einstiegsstelle für den VS-Modus.
 * Übernimmt Kontext-Bindung, Bewegungssteuerung und den Aufruf der Lock-Logik.
 */
void vs_brain_update_player(VsContext* vctx, GameContext* player, bool* deadFlag, bool* needsRedraw);

#endif // _BRAIN_H_