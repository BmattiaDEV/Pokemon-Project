#pragma once

#include "common.h"

// Inizializza texture e risorse VBO per la scena di combattimento / incontro
void battleSceneInit(void);

// Disegna la scena dell'incontro (sfondo dell'area, basi/cerchi dei Pokemon, sprite avversario e giocatore)
// wild_mon_index: 0 = BIDOOF, 1 = SHINX, 2 = STARLY, 3 = BUDEW
// player_mon_index: 0 = CHIMCHAR
void battleSceneDraw(int uLoc_projection, int uLoc_view, int wild_mon_index, int player_mon_index);

// Rilascia le risorse grafiche della scena di incontro
void battleSceneExit(void);

