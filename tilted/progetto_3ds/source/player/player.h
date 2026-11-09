#pragma once

#include "common.h"

typedef enum {
    DIR_DOWN  = 0,
    DIR_UP    = 1,
    DIR_LEFT  = 2,
    DIR_RIGHT = 3
} PlayerDirection;

// Inizializza il giocatore sulla cella di partenza (start_x, start_y)
void playerInit(int start_x, int start_y);

// Imposta la direzione verso cui è rivolto il giocatore (es. dopo un warp tra mappe)
void playerSetDirection(PlayerDirection dir);

// Aggiorna input, movimento tile-to-tile, collisioni, eventi di passo e animazione del giocatore
void playerUpdate(u32 kDown, u32 kHeld);

// Costruisce i 6 vertici del quad 3D dello sprite del giocatore per il frame corrente
void playerBuildQuad(TileVertex out_verts[6]);

// Restituisce la posizione 3D nel mondo del giocatore (usata dalla telecamera per seguirlo)
void playerGetWorldPos(float* out_x, float* out_y, float* out_z);

// Restituisce la riga di profondità Z (`row_z`) per ordinare il disegno del giocatore rispetto agli alberi
int playerGetSortRow(void);
