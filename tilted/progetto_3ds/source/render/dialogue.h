#pragma once

#include "common.h"

// Inizializza texture e buffer del box dialoghi (da chiamare in rendererInit)
void dialogueInit(void);

// Rilascia le risorse grafiche del box dialoghi
void dialogueExit(void);

// Avvia il dialogo associato a text_id
void dialogueStart(int text_id);

// Restituisce true mentre una finestra di dialogo è attiva sullo schermo superiore
bool dialogueIsActive(void);

// Aggiorna l'animazione typewriter e gestisce il tasto A per avanzare o chiudere
void dialogueUpdate(u32 kDown);

// Disegna il box dialoghi in basso sullo schermo superiore (400x240)
void dialogueDraw(int uLoc_projection, int uLoc_view);
