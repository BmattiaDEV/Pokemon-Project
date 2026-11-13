#pragma once

#include "common.h"

// Inizializza il sottosistema audio NDSP della console (canale 0 dedicato alla BGM)
bool audioInit(void);

// Riproduce in loop infinito un file WAV PCM non compresso caricato in memoria (es. via bin2o)
bool audioPlayLoopingWav(const void* wavData, u32 wavSize);

// Imposta il volume della musica di sottofondo (0.0f = muto, 1.0f = massimo)
void audioSetBgmVolume(float volume);

// Ferma la musica corrente e libera il buffer lineare
void audioStopBgm(void);

// Rilascia tutte le risorse audio e chiude NDSP
void audioExit(void);

