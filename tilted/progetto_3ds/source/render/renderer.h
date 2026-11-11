#pragma once

// Inizializza la pipeline grafica 3D (target schermo, shader, telecamera e modello mappa DS)
void rendererInit(void);

// Disegna un singolo frame 3D sullo schermo superiore
void rendererDrawFrame(void);

// Rilascia le risorse grafiche (shader, texture e mappa DS)
void rendererExit(void);
