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

// Costruisce un quad 3D inclinato stile Gen 4 in posizione mondo (wx, wy, wz)
void playerBuildBillboardQuad(TileVertex out_verts[6], float wx, float wy, float wz, float u_left, float v_bottom, float u_right, float v_top);

// Costruisce i 6 vertici del quad 3D dello sprite del giocatore per il frame corrente
void playerBuildQuad(TileVertex out_verts[6]);

// Restituisce la posizione continua sulla griglia (pos_x, pos_y)
void playerGetGridPos(float* out_x, float* out_y);

// Restituisce la posizione 3D nel mondo del giocatore (usata dalla telecamera per seguirlo)
void playerGetWorldPos(float* out_x, float* out_y, float* out_z);

// Restituisce true se il giocatore occupa (o sta entrando in) la cella (gx, gy)
bool playerIsAt(int gx, int gy);

// Logica condivisa (giocatore e NPC) per decidere se la cella (gx, gy) deve mostrare l'overlay sopra i piedi
bool characterShouldOverlayTile(int gx, int gy, int cur_x, int cur_y, int target_x, int target_y, PlayerDirection dir, bool moving, float move_progress);

// Determina se il tile (gx, gy) deve disegnare il proprio overlay (fiori o erba alta) sopra i piedi del giocatore
bool playerShouldOverlayTile(int gx, int gy);
