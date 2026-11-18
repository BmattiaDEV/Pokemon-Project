#pragma once

#include "ds_map.h"
#include "ds_map_model.h"

// Scala 1:1 con i tile del gioco: 32 unità OBJ * 16.0f (TILE_W) = 512.0f
#define DS_GRID_SIZE        32
#define DS_OBJ_MIN         (-16)
#define DS_FIT_SIZE        (DS_GRID_SIZE * TILE_W)

// Parametri zoom telecamera (L/R e reset con X)
#define DS_CAM_DIST_START   CAM_DIST
#define DS_CAM_DIST_MIN     100.0f
#define DS_CAM_DIST_MAX     450.0f
#define DS_ZOOM_STEP          3.0f

// Limiti buffer alberi e animazioni
#define FLOWER_ANIM_FRAMES    5
#define MAX_SINGLE_TREES     64
#define MAX_CONT_TREES      128

// Tipi di cella sulla griglia logica 32x32
typedef enum {
    DS_TILE_VOID = 0,
    DS_TILE_WALKABLE,
    DS_TILE_STAIRS,
    DS_TILE_SOLID
} DsTileType;

// Copia della geometria base dell'acqua per l'animazione 3D e UV
typedef struct {
    float y;
    float u;
    float v;
} WaterBaseVertex;

// Descrittore di un singolo albero 2x2 (standalone o fettina di albero continuo)
typedef struct {
    u32 firstVertex;
    u32 secondVertex; // Usato per conttree_t_lm2 (chioma superiore)
    int gx_min;
    int gx_max;
    int gy_north;
    int gy_south;
} DsSingleTree;

// Stato runtime condiviso della mappa attualmente caricata
typedef struct {
    bool            ready;
    DsMapId         mapId;
    TileVertex*     vbo;
    u32             vertexCount;
    u32             playerFirst;
    u32             flowerFirst;
    u32             grassFirst;
    u32             contTreeFirst;
    u32             groupCount;
    u32             first[DS_MAX_MAP_GROUPS];
    DsMapGroupData  groups[DS_MAX_MAP_GROUPS];

    // Indici risolti dinamicamente in base al ruolo (o -1 se assente nella mappa corrente)
    int             treeSingleGroup;
    int             contTreeBotGroup;
    int             contTreeTopGroup;
    int             stairsGroup;
    int             beachGroup;
    int             waterGroup;
    int             flowerAnimGroup;

    // Texture materiali mappa e overlay
    C3D_Tex         tex[DS_MAX_MAP_GROUPS];
    C3D_Tex         playerTex;
    bool            playerTexReady;
    C3D_Tex         flowerTex;
    bool            flowerTexReady;
    C3D_Tex         grassTex;
    bool            grassTexReady;

    // Trasformazione coordinate OBJ -> Mondo 3D e telecamera
    float           centerX;
    float           centerY;
    float           minZ;
    float           scale;
    float           camDist;
    C3D_Mtx         view;

    // Griglia logica 32x32 (collisioni, quote e flag overlay)
    DsTileType      gridType[DS_GRID_SIZE][DS_GRID_SIZE];
    float           gridHeight[DS_GRID_SIZE][DS_GRID_SIZE];
    bool            gridHasFlowers[DS_GRID_SIZE][DS_GRID_SIZE];
    bool            gridHasGrass[DS_GRID_SIZE][DS_GRID_SIZE];
    bool            gridHasShadow[DS_GRID_SIZE][DS_GRID_SIZE];

    // Tabelle alberi singoli e continui per semitrasparenza dinamica
    DsSingleTree    trees[MAX_SINGLE_TREES];
    u32             treeCount;
    DsSingleTree    contTrees[MAX_CONT_TREES];
    u32             contTreeCount;

    // Stato animazioni (onde acqua 3D + oscillazione fiori)
    WaterBaseVertex* waterBase;
    u32              waterVertexCount;
    u32              waterFirst;
    float            waterAnimTime;
    C3D_Tex          rhanaTex[FLOWER_ANIM_FRAMES];
    C3D_Tex          flowerOverlayTex[FLOWER_ANIM_FRAMES];
    bool             flowerAnimReady;
    u32              flowerAnimTicks;
} DsMapState;

extern DsMapState g_dsMap;

// Conversione coordinate Griglia [0..31] <-> OBJ [-16..+16]
static inline void dsMapGridToObjCenter(float gx, float gy, float* ox, float* oy) {
    *ox = (float)DS_OBJ_MIN + gx + 0.5f;
    *oy = 15.5f - gy;
}

static inline bool dsMapObjToGrid(float ox, float oy, int* gx, int* gy) {
    int x = (int)floorf(ox) - DS_OBJ_MIN;
    int y = 15 - (int)floorf(oy);
    if (x < 0 || x >= DS_GRID_SIZE || y < 0 || y >= DS_GRID_SIZE) return false;
    *gx = x;
    *gy = y;
    return true;
}

// Funzioni del modulo Builder (ds_map_builder.c)
bool dsMapBuilderBuildVbo(void);
void dsMapBuilderBuildCollisionGrid(void);

// Funzioni del modulo Animazioni (ds_map_anim.c)
void dsMapAnimInit(void);
void dsMapAnimExit(void);
void dsMapAnimUpdate(void);
void dsMapAnimUpdateWaterVbo(void);
int  dsMapAnimGetFlowerFrame(void);
