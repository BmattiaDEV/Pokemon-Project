#pragma once

#include "common.h"

// Restituisce true se il tile fa parte di un albero (punte fogliari 21,22,29,30 o corpi albero 120..199)
static inline bool isTreeTile(int tile_id) {
    return (tile_id == 21 || tile_id == 22 ||
            tile_id == 29 || tile_id == 30 ||
            (tile_id >= 120 && tile_id <= 199));
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

