#pragma once

// Inizializza la pipeline grafica 3D (target schermo, shader, telecamera e modello mappa DS)
void rendererInit(void);

// Disegna un singolo frame 3D sullo schermo superiore
void rendererDrawFrame(void);

// Disegna uno schermo nero sullo schermo superiore
void rendererDrawBlackFrame(void);

// Disegna la scena dell'incontro con Pokemon selvatico (sfondo, cerchi/basi e sprite)
void rendererDrawEncounterFrame(int wild_mon_index);

// Rilascia le risorse grafiche (shader, texture e mappa DS)
void rendererExit(void);
