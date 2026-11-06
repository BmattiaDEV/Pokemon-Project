#pragma once

// Generato automaticamente da converti_mappa.py. Non modificare a mano.
//
// Convenzione: -1 = cella vuota, ogni altro valore = ID tile valido.
// I tile dello stesso livello sono raggruppati in mapAltezzaN.

#define MAP_GEN_W 8
#define MAP_GEN_H 8
#define MAP_GEN_MAX_LEVEL 2

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
