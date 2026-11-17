#pragma once

// Singola schermata di dialogo (massimo 2 righe per finestra stile Gen 4)
typedef struct {
    const char* line1;
    const char* line2;
} NpcDialoguePage;

// Restituisce l'array di pagine per l'ID testo dell'NPC e scrive il numero di pagine in out_pageCount
const NpcDialoguePage* npcTextsGet(int text_id, int* out_pageCount);
