#pragma once

#include "common.h"

typedef enum {
    GAME_STATE_WORLD,
    GAME_STATE_ENCOUNTER
} GameState;

// Inizializza il sistema di incontri e il generatore casuale
void encounterInit(void);

// Restituisce lo stato corrente (mondo o incontro)
GameState encounterGetState(void);

// Chiamata quando il giocatore atterra su una nuova casella (gx, gy)
void encounterOnStep(int gx, int gy);

// Chiude l'incontro premendo A e torna al mondo
void encounterDismiss(void);

