#pragma once

#include "common.h"

// Descrittore di un gruppo materiale del modello 3D (vertici x,y,z,u,v + texture .t3x)
typedef struct {
    const float* verts;
    u32          vertexCount;
    const void*  tex;
    u32          texSize;
} DsMapGroupData;

// Inizializza la mappa corrente (geometria VBO, texture, collisioni, animazioni e NPC)
void dsMapInit(void);

// Rilascia tutte le risorse della mappa corrente
void dsMapExit(void);

// Aggiorna zoom telecamera, animazioni mappa (acqua/fiori), giocatore e NPC
void dsMapUpdate(u32 kHeld, u32 kDown);

// Collisioni e conversione coordinate sulla griglia 32x32 della mappa (0..31)
bool  dsMapCanMove(int from_gx, int from_gy, int to_gx, int to_gy);
bool  dsMapIsWildGrass(int gx, int gy);
float dsMapGetWorldPos(float grid_x, float grid_y, float* out_x, float* out_y, float* out_z);

// Disegna il frame 3D completo della mappa (materiali, giocatore, NPC, overlay e alberi semitrasparenti)
void dsMapDrawFrame(C3D_Mtx* projection, int uLoc_projection, int uLoc_view);
