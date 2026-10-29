#include "map.h"
#include "../map/map_data.h"

// Restituisce l'altezza 3D del terreno base
float getTerrainHeight(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 3.0f * BLOCK_UNIT; 
    return mapTerrainElevations[z][x] * BLOCK_UNIT;
}

// Restituisce l'altezza 3D della struttura (leggermente rialzata per gestire la sovrapposizione)
float getStructureHeight(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 3.0f * BLOCK_UNIT; 
    return (mapStructureElevations[z][x] * BLOCK_UNIT) + 0.01f;
}

// COLLISIONI
int isBlocked(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 1; 
    
    // Blocchiamo il giocatore se sbatte contro i bordi del terreno base
    if (mapTerrainElevations[z][x] >= 3) {
        return 1;
    }
    return 0; // Libero
}

// ERBA SELVATICA (Encounter)
int isGrass(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 0;
    
    return mapTerrainTypes[z][x] == '1';
}