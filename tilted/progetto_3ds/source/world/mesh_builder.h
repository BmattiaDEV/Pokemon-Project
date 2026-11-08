#pragma once

#include "common.h"

// Costruisce in linear RAM il Vertex Buffer Object (VBO) 3D dell'intera mappa
// e riserva in coda 6 vertici dinamici per lo sprite 3D del giocatore
void meshBuilderBuild(void);

// Libera il VBO della mappa dalla linear RAM
void meshBuilderFree(void);

// Restituisce il puntatore ai dati del VBO costruito
TileVertex* meshBuilderGetVbo(void);

// Restituisce il numero totale di vertici della mappa nel VBO (esclusi i 6 vertici del giocatore)
u32 meshBuilderGetVertexCount(void);

// Restituisce l'indice di vertice nel VBO subito dopo il terreno della riga `row_z`
// (prima degli alberi con base su `row_z` e di tutte le righe successive > `row_z`)
u32 meshBuilderGetRowGroundEnd(int row_z);

// Aggiorna i 6 vertici dinamici del giocatore in coda al VBO e fa il flush della cache GPU
void meshBuilderUpdatePlayerVertices(const TileVertex player_verts[6]);
