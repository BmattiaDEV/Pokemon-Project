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

// Imposta la direzione verso cui è rivolto il giocatore
void playerSetDirection(PlayerDirection dir);

// Aggiorna input, movimento tile-to-tile, collisioni e animazione del giocatore
void playerUpdate(u32 kDown, u32 kHeld);

// Costruisce i 6 vertici del quad 3D dello sprite del giocatore per il frame corrente
void playerBuildQuad(TileVertex out_verts[6]);

// Restituisce la posizione continua sulla griglia (pos_x, pos_y)
void playerGetGridPos(float* out_x, float* out_y);

// Restituisce la posizione 3D nel mondo del giocatore (usata dalla telecamera per seguirlo)
void playerGetWorldPos(float* out_x, float* out_y, float* out_z);

// Determina se il tile (gx, gy) deve disegnare il proprio overlay (fiori o erba alta) sopra i piedi
bool playerShouldOverlayTile(int gx, int gy);
