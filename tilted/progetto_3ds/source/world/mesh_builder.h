#pragma once

#include "common.h"

// Costruisce in linear RAM il Vertex Buffer Object (VBO) 3D dell'intera mappa
void meshBuilderBuild(void);

// Libera il VBO della mappa dalla linear RAM
void meshBuilderFree(void);

// Restituisce il puntatore ai dati del VBO costruito
TileVertex* meshBuilderGetVbo(void);

// Restituisce il numero totale di vertici presenti nel VBO
u32 meshBuilderGetVertexCount(void);

