#pragma once

#include "common.h"

#define COLLISION_BLOCKED_LEVEL (-999)

// Restituisce il livello di terreno calpestabile della cella (x, y),
// oppure COLLISION_BLOCKED_LEVEL se la cella è fuori mappa o occupata da una parete rocciosa (cliff)
int collisionGetWalkableLevel(int x, int y);

// Controlla se il giocatore può muoversi dalla cella (from_x, from_y) a quota `from_level`
// verso la cella adiacente (to_x, to_y):
// - Blocca fuori mappa, pareti di montagna e dislivelli d'altezza
// - Blocca alberi solidi (tronchi/basi) posti alla stessa altezza del giocatore
// - Permette il passaggio su erba, sentieri, fiori e dietro le chiome alte degli alberi
bool collisionCanMove(int from_x, int from_y, int from_level, int to_x, int to_y);

