#pragma once

#include "common.h"
#include "map_data.h"
#include "tileset_defs.h"

#if (MAP_GEN_MIN_LEVEL < MIN_HEIGHT_LEVEL) || (MAP_GEN_MAX_LEVEL > MAX_HEIGHT_LEVEL)
#error "I livelli della mappa superano il range supportato [-32, 32]."
#endif

// Restituisce il livello del terreno base (0 se presente nel range, altrimenti MAP_GEN_MIN_LEVEL)
int worldGetBaseGroundLevel(void);

// Legge in sicurezza il tile grezzo dal livello `level` nella cella (x, y)
int worldGetRawTile(int level, int x, int y);

// Controlla se la cella (x, y) sul livello `level` si trova all'interno del perimetro di una montagna sullo stesso livello
bool worldIsInsideMountainPlateau(int level, int x, int y);

// Restituisce l'indice di riga dell'albero partendo dal basso (0 = base tronco, 1..3 per 4 righe, 0..5 per 6 righe)
int worldGetTreeRowFromBottom(int level, int x, int y, int tile_id);

// Controlla se esiste un tile valido a qualsiasi livello superiore a `level` nella cella (x, y)
bool worldHasTileAbove(int level, int x, int y);

// Controlla se esiste un tile di terreno/montagna (non-overlay) sopra `level` nella cella (x, y)
bool worldHasTerrainAbove(int level, int x, int y);

// Restituisce il tile effettivo da disegnare al livello `level`, riempiendo con erba eventuali buchi sotto montagne/terreno base
int worldGetEffectiveTile(int level, int x, int y);

// Determina se la cella (cx, cy) sul livello `level` solleva il proprio angolo (corner_dx, corner_dy)
bool worldDoesCellRaiseCorner(int level, int cx, int cy, int corner_dx, int corner_dy);

// Calcola l'altezza 3D (Y) del vertice di griglia (gx, gy) per un dato livello `level`
float worldGetCornerHeight(int level, int gx, int gy);

// Trova il livello del terreno su cui poggia un tile overlay (fiori/sentieri/alberi) posto sul layer `level`
int worldGetOverlayBaseLevel(int level, int x, int y);

