#pragma once

#include "common.h"
#include "player.h"

#define MAX_NPCS 32

typedef enum {
    NPC_MOVE_STILL  = 0, // Resta fermo sulla cella iniziale
    NPC_MOVE_WANDER = 1  // Cammina casualmente entro un'area (es. 3x3 con raggio 1) attorno allo spawn
} NpcMoveMode;

typedef struct {
    int             home_x;
    int             home_y;
    int             wander_radius; // 1 = area massima 3x3 centrata in (home_x, home_y)
    NpcMoveMode     move_mode;
    int             grid_x;
    int             grid_y;
    int             target_x;
    int             target_y;
    float           pos_x;
    float           pos_y;
    PlayerDirection dir;           // 0=DIR_DOWN, 1=DIR_UP, 2=DIR_LEFT, 3=DIR_RIGHT
    int             sprite_id;     // Riga in data/npcs.png (0..3)
    int             text_id;       // Indice del dialogo in source/npc/npc_texts.c
    bool            moving;
    float           move_progress;
    int             step_parity;
    int             wait_frames;
} Npc;

// Carica la texture degli NPC, inizializza la tabella degli NPC e il VBO lineare
void npcInit(void);

// Libera texture e VBO degli NPC
void npcExit(void);

// Aggiunge un NPC a runtime (wander_radius = 1 corrisponde a un'area 3x3 attorno a gx, gy)
int npcAdd(int gx, int gy, PlayerDirection dir, int sprite_id, NpcMoveMode move_mode, int wander_radius, int text_id);

// Aggiorna movimento autonomo, animazione dei passi e collisioni di tutti gli NPC
void npcUpdateAll(void);

// Se il giocatore ha di fronte un NPC fermo sulla sua cella, lo gira verso di lui e avvia il dialogo
bool npcTryInteract(int player_gx, int player_gy, PlayerDirection player_dir);

// Restituisce il numero attuale di NPC registrati
int npcGetCount(void);

// Restituisce true se un NPC occupa (o sta entrando in) la cella (gx, gy) (usato per le collisioni)
bool npcIsAt(int gx, int gy);

// Restituisce il puntatore all'NPC che occupa (o sta entrando in) la cella (gx, gy), oppure NULL
Npc* npcGetAt(int gx, int gy);

// Restituisce true se almeno un NPC richiede l'overlay di fiori/erba alta sopra i piedi sulla cella (gx, gy)
bool npcShouldOverlayTile(int gx, int gy);

// Disegna tutti gli NPC con lo Z-buffer 3D attivo e la stessa inclinazione prospettica del giocatore
void npcDrawAll(float cam_x, float cam_y, float cam_z, float cam_dist);
