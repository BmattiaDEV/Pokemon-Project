#pragma once

#include "common.h"

#define DS_MAX_MAP_GROUPS 18

typedef enum {
    DS_MAP_ROUTE    = 0, // Mappa iniziale (costa, prato e sentiero per il bosco)
    DS_MAP_CLEARING = 1  // Radura secondaria con sentieri sabbiosi e aiuole recintate
} DsMapId;

// Ruolo logico di ciascun gruppo materiale (indipendente dall'indice nel modello)
typedef enum {
    DS_MAT_GROUND_WALK = 0, // Terreno calpestabile generico (erba, sentiero, sabbia)
    DS_MAT_TREE_SINGLE,     // Alberi singoli 2x2 (tree01_lm2)
    DS_MAT_TREE_SHADOW,     // Ombra alberi (solo visivo)
    DS_MAT_CONTTREE_BOT,    // Alberi continui - parte inferiore e tronchi (conttree_b_lm2)
    DS_MAT_CONTTREE_TOP,    // Alberi continui - chioma superiore (conttree_t_lm2)
    DS_MAT_SOLID_CLIFF,     // Ostacoli solidi (rocce, scogliere, recinzioni, bordi esterni)
    DS_MAT_STAIRS,          // Scale con cambio di quota (newstep_lm21)
    DS_MAT_BEACH,           // Spiaggia calpestabile dove Z >= 0.95 (seaside3_lm67)
    DS_MAT_WATER_ANIM,      // Acqua con animazione onde 3D + scorrimento UV (sea_lm1)
    DS_MAT_FLOWER_ANIM,     // Fiori arancioni animati con overlay sui piedi (rhana)
    DS_MAT_FLOWER_FLAT,     // Fiori piatti calpestabili (nhana)
    DS_MAT_WARP_SHADOW,     // Ombra entrata/uscita mappa (fenter_lm6)
    DS_MAT_WILD_GRASS       // Erba alta con incontri selvatici (nectgr_lm42)
} DsMaterialRole;

// Descrittore di un gruppo materiale del modello 3D (vertici x,y,z,u,v + texture .t3x)
typedef struct {
    const float*   verts;
    u32            vertexCount;
    const void*    tex;
    u32            texSize;
    DsMaterialRole role;
} DsMapGroupData;

// Inizializza la mappa predefinita (DS_MAP_ROUTE)
void dsMapInit(void);

// Rilascia tutte le risorse della mappa corrente
void dsMapExit(void);

// Controlla se la cella (gx, gy) contiene un evento di cambio mappa e, se sì, esegue il teletrasporto
bool dsMapTryWarpAt(int gx, int gy);

// Aggiorna zoom telecamera, animazioni mappa (acqua/fiori), giocatore e NPC
void dsMapUpdate(u32 kHeld, u32 kDown);

// Collisioni e conversione coordinate sulla griglia 32x32 della mappa (0..31)
bool  dsMapCanMove(int from_gx, int from_gy, int to_gx, int to_gy);
bool  dsMapIsWildGrass(int gx, int gy);
float dsMapGetWorldPos(float grid_x, float grid_y, float* out_x, float* out_y, float* out_z);

// Disegna il frame 3D completo della mappa (materiali, giocatore, NPC, overlay e alberi semitrasparenti)
void dsMapDrawFrame(C3D_Mtx* projection, int uLoc_projection, int uLoc_view);
