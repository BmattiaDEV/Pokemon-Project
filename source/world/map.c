#include "map.h"
#include "../../data/map/map_data.h"

// Restituisce l'altezza reale in 3D di una specifica casella
float getTileHeight(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 3.0f * BLOCK_UNIT; 
    return mapElevations[z][x] * BLOCK_UNIT;
}

// Ritorna il livello di elevazione grezzo (1 = terra, 2 = collina, 3 = muro)
int mapHeight(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 3; 
    return mapElevations[z][x];
}

// COLLISIONI: Il giocatore sbatte contro qualcosa?
int isBlocked(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 1; 
    
    int tile_elevation = mapElevations[z][x];
    
    // Blocchiamo il giocatore SOLO se prova ad andare sui bordi esterni (>= 3)
    if (tile_elevation >= 3) {
        return 1;
    }
    
    return 0; // Libero di muoversi ovunque (livelli 1 e 2)!
}

// ERBA SELVATICA (Encounter)
int isGrass(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 0;
    
    // '1' rappresenta l'erba alta
    return mapTypes[z][x] == '1';
}