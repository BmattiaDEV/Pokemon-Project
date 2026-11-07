#pragma once

// Generato automaticamente da converti_mappa.py. Non modificare a mano.
//
// Convenzione: -1 = cella vuota, ogni altro valore = ID tile valido.
// Range altezze supportato: [-32, 32].

#define MAP_ABS_MIN_LEVEL (-32)
#define MAP_ABS_MAX_LEVEL (32)
#define MAP_GEN_W 8
#define MAP_GEN_H 8
#define MAP_GEN_MIN_LEVEL (0)
#define MAP_GEN_MAX_LEVEL (2)
#define MAP_GEN_NUM_LEVELS 3

// Tile del livello 0 (altezza_0.csv), -1 = vuoto
static const int mapAltezza0[MAP_GEN_H][MAP_GEN_W] __attribute__((unused)) = {
    {0,0,0,0,0,0,0,0},
    {0,0,0,-1,-1,0,0,0},
    {0,0,-1,-1,-1,0,0,0},
    {0,-1,-1,-1,-1,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,-1,0,0},
    {0,0,0,0,0,0,0,0}
};

// Tile del livello 1 (altezza_1.csv), -1 = vuoto
static const int mapAltezza1[MAP_GEN_H][MAP_GEN_W] __attribute__((unused)) = {
    {-1,-1,8,9,9,10,-1,-1},
    {-1,-1,12,0,0,14,5,-1},
    {8,9,13,0,0,14,-1,-1},
    {12,0,0,0,0,14,-1,-1},
    {16,17,17,17,17,18,-1,-1},
    {-1,5,-1,-1,20,21,22,-1},
    {-1,5,-1,-1,24,25,26,-1},
    {-1,-1,-1,-1,28,29,30,-1}
};

// Tile del livello 2 (altezza_2.csv), -1 = vuoto
static const int mapAltezza2[MAP_GEN_H][MAP_GEN_W] __attribute__((unused)) = {
    {-1,-1,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,8,10,-1,-1,-1},
    {-1,-1,-1,12,14,-1,-1,-1},
    {-1,-1,-1,16,18,-1,-1,-1},
    {-1,-1,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,-1,-1,-1,-1,-1}
};

// Tabella indicizzata per (level - MAP_GEN_MIN_LEVEL)
static const int (* const mapLevels[MAP_GEN_NUM_LEVELS])[MAP_GEN_W] __attribute__((unused)) = {
    mapAltezza0,
    mapAltezza1,
    mapAltezza2
};
