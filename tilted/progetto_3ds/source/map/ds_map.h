#pragma once

#include "common.h"

// Un materiale del modello: vertici (5 float per vertice: x y z u v) e la texture .t3x
typedef struct {
    const float* verts;
    u32          vertexCount;
    const void*  tex;
    u32          texSize;
} DsMapGroupData;

// Carica texture, buffer dei vertici e genera la griglia di collisione DS
void dsMapInit(void);

// Libera le risorse caricate da dsMapInit
void dsMapExit(void);

// Aggiorna lo zoom opzionale (L/R) e reset visuale (X) sulla mappa DS
void dsMapUpdate(u32 kHeld, u32 kDown);

// Collisioni e altezze sulla griglia 32x32 della mappa DS (coordinate 0..31)
bool  dsMapCanMove(int from_gx, int from_gy, int to_gx, int to_gy);
float dsMapGetWorldPos(float grid_x, float grid_y, float* out_x, float* out_y, float* out_z);

// Disegna la mappa DS e il giocatore (chiamata da rendererDrawFrame)
void dsMapDrawFrame(C3D_Mtx* projection, int uLoc_projection, int uLoc_view);
