#pragma once

#include "common.h"

typedef void (*TransitionMidCallback)(void* user_data);

// Inizializza e rilascia le risorse della transizione schermo (quad fullscreen in VRAM lineare)
void transitionInit(void);
void transitionExit(void);

// Avvia una transizione dissolvenza nero -> callback a metà schermo nero -> dissolvenza in apertura
void transitionStart(TransitionMidCallback on_black, void* user_data);

// Restituisce true mentre è in corso una transizione di cambio mappa
bool transitionIsActive(void);

// Avanza i frame di dissolvenza ed esegue la callback nel punto completamente nero
void transitionUpdate(void);

// Disegna il velo nero di dissolvenza sullo schermo superiore (400x240)
void transitionDraw(int uLoc_projection, int uLoc_view);
