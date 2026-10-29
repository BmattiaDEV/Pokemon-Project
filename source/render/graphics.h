#pragma once
#include "../core/common.h"

// Inizializza la grafica, alloca il VBO, costruisce la mappa e carica le texture
bool graphicsInit(void);

// Libera la memoria allocata alla chiusura del gioco
void graphicsFree(void);

// Disegna l'overworld (mappa 3D + eventuale player)
void graphicsRenderOverworld(C3D_RenderTarget* target, int uLoc_projection, int uLoc_modelView, int uLoc_uvoff, C3D_Mtx* projection, float wx, float wz, int tx, int ty, int frame);

// Funzione esposta per permettere a map_builder.c di inviare i vertici al VBO
void addVert(float x, float y, float z, float u, float v, const float* c);