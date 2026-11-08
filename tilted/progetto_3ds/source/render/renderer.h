#pragma once

// Inizializza la pipeline grafica 3D (target schermo, shader, telecamera, texture .t3x e VBO mappa)
void rendererInit(void);

// Disegna un singolo frame 3D sullo schermo superiore
void rendererDrawFrame(void);

// Rilascia le risorse grafiche (VBO, shader, texture)
void rendererExit(void);

