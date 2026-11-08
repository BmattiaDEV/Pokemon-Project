#pragma once

#include "common.h"

// Restituisce true se il tile fa parte di un albero (punte fogliari 21,22,29,30 o corpi albero 120..199)
static inline bool isTreeTile(int tile_id) {
    return (tile_id == 21 || tile_id == 22 ||
            tile_id == 29 || tile_id == 30 ||
            (tile_id >= 120 && tile_id <= 199));
}

// Restituisce true se il tile dell'albero rappresenta il tronco/base solida a terra (con collisione),
// mentre restituisce false per la chioma alta e le punte laterali (dietro cui il giocatore può camminare)
static inline bool isSolidTreeTile(int tile_id) {
    // Le punte fogliari laterali (1 tile a sinistra/destra) non bloccano il passaggio
    if (tile_id == 21 || tile_id == 22 || tile_id == 29 || tile_id == 30 ||
        tile_id == 162 || tile_id == 167 || tile_id == 170 || tile_id == 175 ||
        tile_id == 178 || tile_id == 183 || tile_id == 186 || tile_id == 191) {
        return false;
    }
    // Albero singolo / coppia orizzontale (4 righe: 120..151):
    // le 2 righe inferiori (tronco + base chioma subito dietro: 136..151) sono solide,
    // mentre la punta superiore dell'albero (120..135) permette di passare dietro
    if (tile_id >= 136 && tile_id <= 151) {
        return true;
    }
    // Coppia verticale di alberi sovrapposti (6 righe: 152..199):
    // le 4 righe inferiori (168..199, incluso il tile subito dietro l'albero posteriore) sono solide,
    // mentre la punta superiore (152..167) permette di passare dietro
    if (tile_id >= 168 && tile_id <= 199) {
        return true;
    }
    return false;
}

// Restituisce true se il tile è un elemento che si appoggia al terreno
// (sassolini/fiori 16..27, sentieri/incroci/piazzole 73..117, o alberi)
static inline bool isOverlayTile(int tile_id) {
    return ((tile_id >= 16 && tile_id <= 27) ||
            (tile_id >= 73 && tile_id <= 117) ||
            isTreeTile(tile_id));
}

// Restituisce true se il tile è un bordo roccioso/parete della montagna (blocco 4x4: 41..68 eccetto il centro 50,51,58,59)
static inline bool isCliffBorderTile(int tile_id) {
    return (tile_id == 41 || tile_id == 42 || tile_id == 43 || tile_id == 44 ||
            tile_id == 49 || tile_id == 52 ||
            tile_id == 57 || tile_id == 60 ||
            tile_id == 65 || tile_id == 66 || tile_id == 67 || tile_id == 68);
}

// Restituisce l'offset orizzontale verso la colonna del tronco (-1 per punte destre, +1 per punte sinistre, 0 per il corpo)
static inline int getTreeTrunkOffsetX(int tile_id) {
    if (tile_id == 21 || tile_id == 29 ||
        tile_id == 162 || tile_id == 170 || tile_id == 178 || tile_id == 186) {
        return 1;
    }
    if (tile_id == 22 || tile_id == 30 ||
        tile_id == 167 || tile_id == 175 || tile_id == 183 || tile_id == 191) {
        return -1;
    }
    return 0;
}
