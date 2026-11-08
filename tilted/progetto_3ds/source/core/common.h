#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Configurazione display e colore di sfondo
#define CLEAR_COLOR 0x1F2430FF
#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

// Limiti dei livelli d'altezza (-32 -> +32, fino a 64 dislivelli / 65 livelli)
#define MIN_HEIGHT_LEVEL (-32)
#define MAX_HEIGHT_LEVEL (32)
#define MAX_HEIGHT_LAYERS 65

// Costanti del Tileset (outdoor.png: 128x512, 8x32 blocchi da 16x16)
#define TILESET_COLS 8
#define TILESET_ROWS 32
#define TOTAL_TILES  (TILESET_COLS * TILESET_ROWS)
#define TILE_W       16.0f
#define TILE_H       16.0f
#define TEX_W        (TILESET_COLS * TILE_W) // 128.0f
#define TEX_H        (TILESET_ROWS * TILE_H) // 512.0f

// Costanti dello Spritesheet Giocatore (player.png: 32x512, 13 frame da 32x32)
#define PLAYER_SPRITE_W   32.0f
#define PLAYER_SPRITE_H   32.0f
#define PLAYER_TEX_W      32.0f
#define PLAYER_TEX_H      512.0f
#define PLAYER_RISE_Y     25.0f // Altezza 3D (+Y) dello sprite del giocatore
#define PLAYER_SLANT_Z    17.0f // Inclinazione 3D (-Z) verso l'angolo della telecamera
#define PLAYER_MOVE_SPEED (1.0f / 16.0f) // 16 frame per percorrere 1 tile (1px/frame a 60fps)

// Configurazione angolo telecamera, dislivelli e inclinazione 3D alberi
#define CAM_ANGLE_DEG 56.0f  // Angolo di inclinazione della telecamera (in gradi)
#define CAM_DIST      200.0f // Distanza della telecamera dal giocatore
#define LEVEL_HEIGHT  12.0f  // Dislivello per ogni piano (livello L => quota L * LEVEL_HEIGHT)
#define TREE_RISE_Y   9.0f   // Guadagno in altezza (+Y) per ogni riga dell'albero
#define TREE_PULL_Z   3.0f   // Avanzamento verso la telecamera (+Z) per ogni riga dell'albero

// Struttura vertice per il rendering 3D dei tile e del giocatore
typedef struct {
    float x, y, z;    // Posizione 3D del vertice nel mondo
    float u, v;       // Coordinate UV della texture
    float ox, oy;     // Offset 2D nello spazio vista (inOff.xy)
} TileVertex;
