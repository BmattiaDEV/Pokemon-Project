#pragma once

#include "common.h"

// Inizializza texture e risorse VBO per la scena di combattimento / incontro
void battleSceneInit(void);

// Disegna la scena dell'incontro (sfondo dell'area, basi/cerchi dei Pokemon, uccello front avversario e uccello back giocatore)
void battleSceneDraw(int uLoc_projection, int uLoc_view, int wild_mon_index, int player_mon_index);

// Rilascia le risorse grafiche della scena di incontro
void battleSceneExit(void);

