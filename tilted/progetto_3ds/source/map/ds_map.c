#include "ds_map.h"
#include "ds_map_model.h"
#include "player.h"
#include "player_t3x.h"
#include "flower_overlay_t3x.h"

// Scala 1:1 con i tile del gioco: 32 unità OBJ * 16.0f (TILE_W) = 512.0f
#define DS_GRID_SIZE       32
#define DS_OBJ_MIN        (-16)
#define DS_FIT_SIZE       (DS_GRID_SIZE * TILE_W) // 512.0f -> 1 unità OBJ = 1 TILE (16.0f)

#define DS_CAM_DIST_START  CAM_DIST // 200.0f come nella mappa a tile
#define DS_CAM_DIST_MIN    100.0f
#define DS_CAM_DIST_MAX    450.0f
#define DS_ZOOM_STEP         3.0f

// Tipi di cella sulla mappa DS
typedef enum {
    DS_TILE_VOID = 0,
    DS_TILE_WALKABLE,
    DS_TILE_STAIRS,
    DS_TILE_SOLID
} DsTileType;

static bool           dsReady  = false;
static TileVertex*    dsVbo    = NULL;
static u32            dsVertexCount = 0;
static u32            dsPlayerFirst = 0;
static u32            dsFlowerFirst = 0;
static u32            dsFirst[DS_MAP_GROUP_COUNT];
static DsMapGroupData dsGroups[DS_MAP_GROUP_COUNT];
static C3D_Tex        dsTex[DS_MAP_GROUP_COUNT];
static C3D_Tex        dsPlayerTex;
static bool           dsPlayerTexReady = false;
static C3D_Tex        dsFlowerTex;
static bool           dsFlowerTexReady = false;
static bool           dsGridHasFlowers[DS_GRID_SIZE][DS_GRID_SIZE];
static bool           dsGridHasShadow[DS_GRID_SIZE][DS_GRID_SIZE];
static C3D_Mtx        dsView;

// Trasformazione dalle coordinate OBJ a quelle del gioco
static float dsCenterX = 0.0f;
static float dsCenterY = 0.0f;
static float dsMinZ    = 0.0f;
static float dsScale   = 1.0f;

// Distanza telecamera (regolabile con L/R)
static float dsDist = DS_CAM_DIST_START;

// Griglia logica 32x32 (gx = 0..31 corrisponde a OBJ X = -16..+15,
//                      gy = 0..31 corrisponde a OBJ Y = +15..-16)
static DsTileType dsGridType[DS_GRID_SIZE][DS_GRID_SIZE];
static float      dsGridHeight[DS_GRID_SIZE][DS_GRID_SIZE];

// Converte (gx, gy) della griglia [0..31] nel centro cella in coordinate OBJ
static inline void gridToObjCenter(float gx, float gy, float* ox, float* oy) {
    *ox = (float)DS_OBJ_MIN + gx + 0.5f;
    *oy = 15.5f - gy; // gy=0 è il nord (+Y nell'OBJ), gy=31 è il sud (-Y nell'OBJ)
}

// Converte coordinate OBJ (ox, oy) in indici griglia [0..31]
static inline bool objToGrid(float ox, float oy, int* gx, int* gy) {
    int x = (int)floorf(ox) - DS_OBJ_MIN;
    int y = 15 - (int)floorf(oy);
    if (x < 0 || x >= DS_GRID_SIZE || y < 0 || y >= DS_GRID_SIZE) return false;
    *gx = x;
    *gy = y;
    return true;
}

// Test 2D punto-in-triangolo sul piano XY dell'OBJ
static bool pointInTriXY(float px, float py,
                         const float* a, const float* b, const float* c,
                         float* out_z) {
    float v0x = c[0] - a[0], v0y = c[1] - a[1];
    float v1x = b[0] - a[0], v1y = b[1] - a[1];
    float v2x = px   - a[0], v2y = py   - a[1];

    float dot00 = v0x * v0x + v0y * v0y;
    float dot01 = v0x * v1x + v0y * v1y;
    float dot02 = v0x * v2x + v0y * v2y;
    float dot11 = v1x * v1x + v1y * v1y;
    float dot12 = v1x * v2x + v1y * v2y;

    float denom = dot00 * dot11 - dot01 * dot01;
    if (fabsf(denom) < 1e-5f) return false;

    float inv = 1.0f / denom;
    float u = (dot11 * dot02 - dot01 * dot12) * inv;
    float v = (dot00 * dot12 - dot01 * dot02) * inv;
    if (u >= -0.02f && v >= -0.02f && (u + v) <= 1.02f) {
        float w = 1.0f - u - v;
        *out_z = w * a[2] + v * b[2] + u * c[2];
        return true;
    }
    return false;
}

// Costruisce la mappa di collisione e altezza analizzando i gruppi dell'OBJ
static void dsBuildCollisionGrid(void) {
    // Indici gruppi in ds_map_model.h:
    // 0: ngrass_lm2 (terreno)
    // 1: tree01_lm2 (alberi singoli: collisione SOLO sul tronco!)
    // 2: tshadow_lm2 (ombra alberi: calpestabile)
    // 3: conttree_b_lm2 (tronchi alberi continui: SOLIDO tranne corridoio entrata)
    // 4: conttree_t_lm2 (foglie alberi continui: NESSUNA COLLISIONE)
    // 5: criffp_lm42, 6: file1Material, 7: file2Material, 8: criff_lm44 (bordi rocce: SOLIDO)
    // 9: newstep_lm21 (scale: CALPESTABILE + cambio livello)
    // 10: seaside3_lm67 (spiaggia/riva), 11: sea_lm1 (mare: SOLIDO)
    // 12: rhana (fiori: CALPESTABILE)
    // 13: fenter_lm6 (ombra entrata foresta: CALPESTABILE, passaggio verso altre mappe)
    // 14: lgreenp_lm2 (erba chiara: CALPESTABILE)
    // 15: wcliff_lm4, 16: searock_lm15 (scogli: SOLIDO)

    const int walkable_groups[] = { 0, 12, 13, 14 };
    const int cliff_groups[]    = { 5, 6, 7, 8, 15, 16 };

    for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
        for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
            dsGridType[gy][gx]       = DS_TILE_VOID;
            dsGridHeight[gy][gx]     = 2.0f;
            dsGridHasFlowers[gy][gx] = false;
            dsGridHasShadow[gy][gx]  = false;
        }
    }

    // 1. Terreno calpestabile (erba, fiori, entrata foresta, erba chiara, spiaggia alta)
    for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
        for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
            float ox, oy, z_hit;
            gridToObjCenter((float)gx, (float)gy, &ox, &oy);

            for (unsigned int w = 0; w < sizeof(walkable_groups)/sizeof(walkable_groups[0]); ++w) {
                int g = walkable_groups[w];
                for (u32 i = 0; i + 2 < dsGroups[g].vertexCount; i += 3) {
                    const float* v0 = &dsGroups[g].verts[(i + 0) * 5];
                    const float* v1 = &dsGroups[g].verts[(i + 1) * 5];
                    const float* v2 = &dsGroups[g].verts[(i + 2) * 5];
                    if (pointInTriXY(ox, oy, v0, v1, v2, &z_hit)) {
                        dsGridType[gy][gx]   = DS_TILE_WALKABLE;
                        dsGridHeight[gy][gx] = roundf(z_hit);
                        if (g == 12) dsGridHasFlowers[gy][gx] = true;
                        if (g == 13) dsGridHasShadow[gy][gx]  = true;
                    }
                }
            }

            // Spiaggia calpestabile (seaside3_lm67 dove Z >= 0.95)
            for (u32 i = 0; i + 2 < dsGroups[10].vertexCount; i += 3) {
                const float* v0 = &dsGroups[10].verts[(i + 0) * 5];
                const float* v1 = &dsGroups[10].verts[(i + 1) * 5];
                const float* v2 = &dsGroups[10].verts[(i + 2) * 5];
                if (pointInTriXY(ox, oy, v0, v1, v2, &z_hit) && z_hit >= 0.95f) {
                    if (dsGridType[gy][gx] == DS_TILE_VOID) {
                        dsGridType[gy][gx]   = DS_TILE_WALKABLE;
                        dsGridHeight[gy][gx] = 1.0f;
                    }
                }
            }
        }
    }

    // 2. Ostacoli rocce/scogliere (bloccano il passaggio)
    for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
        for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
            float ox, oy, z_hit;
            gridToObjCenter((float)gx, (float)gy, &ox, &oy);

            for (unsigned int c = 0; c < sizeof(cliff_groups)/sizeof(cliff_groups[0]); ++c) {
                int g = cliff_groups[c];
                for (u32 i = 0; i + 2 < dsGroups[g].vertexCount; i += 3) {
                    const float* v0 = &dsGroups[g].verts[(i + 0) * 5];
                    const float* v1 = &dsGroups[g].verts[(i + 1) * 5];
                    const float* v2 = &dsGroups[g].verts[(i + 2) * 5];
                    if (pointInTriXY(ox, oy, v0, v1, v2, &z_hit)) {
                        dsGridType[gy][gx] = DS_TILE_SOLID;
                    }
                }
            }
        }
    }

    // 3. Alberi singoli (tree01_lm2, gruppo 1):
    //    Ogni albero è composto da 24 vertici (4 quad) e copre una matrice di 2x2 tile.
    //    - La base del tronco (minY + 0.4f) è SOLIDA.
    //    - Una casella in più di collisione dietro a ogni albero (minY + 1.4f): SOLIDA
    //      così non è più possibile passare in mezzo agli alberi della foresta!
    for (u32 i = 0; i + 23 < dsGroups[1].vertexCount; i += 24) {
        // Il 4° quad (vertici i+18 .. i+23) è la base a terra del tronco
        const float* b0 = &dsGroups[1].verts[(i + 18) * 5];
        const float* b1 = &dsGroups[1].verts[(i + 19) * 5];
        const float* b2 = &dsGroups[1].verts[(i + 20) * 5];

        float minX = fminf(b0[0], fminf(b1[0], b2[0]));
        float maxX = fmaxf(b0[0], fmaxf(b1[0], b2[0]));
        float minY = fminf(b0[1], fminf(b1[1], b2[1]));

        for (float x = minX + 0.5f; x < maxX; x += 1.0f) {
            int gx, gy;
            // 1. Base del tronco: solido
            if (objToGrid(x, minY + 0.4f, &gx, &gy)) {
                dsGridType[gy][gx] = DS_TILE_SOLID;
            }
            // 2. Una casella in più di collisione dietro all'albero: solido
            if (objToGrid(x, minY + 1.4f, &gx, &gy)) {
                dsGridType[gy][gx] = DS_TILE_SOLID;
            }
        }
    }

    // 4. Alberi continui:
    //    - conttree_t_lm2 (gruppo 4, foglie): IGNORATO così ci passi sotto!
    //    - conttree_b_lm2 (gruppo 3, tronchi):
    //      Ogni sezione di tronchi continui ha 2 quad (12 vertici).
    //      La riga inferiore (base tronco, minY + 0.4f) è SOLIDA.
    //      Una casella in più di collisione dietro (minY + 1.4f) è SOLIDA.
    for (u32 s = 0; s + 11 < dsGroups[3].vertexCount; s += 12) {
        float minX = 1e30f, maxX = -1e30f;
        float minY = 1e30f;
        for (int v = 0; v < 12; ++v) {
            const float* pt = &dsGroups[3].verts[(s + v) * 5];
            if (pt[0] < minX) minX = pt[0];
            if (pt[0] > maxX) maxX = pt[0];
            if (pt[1] < minY) minY = pt[1];
        }

        for (float x = minX + 0.5f; x < maxX; x += 1.0f) {
            int gx, gy;
            // 1. Base del tronco: solido
            if (objToGrid(x, minY + 0.4f, &gx, &gy)) {
                dsGridType[gy][gx] = DS_TILE_SOLID;
            }
            // 2. Una casella in più di collisione dietro: solido
            if (objToGrid(x, minY + 1.4f, &gx, &gy)) {
                dsGridType[gy][gx] = DS_TILE_SOLID;
            }
        }
    }

    // Albero all'entrata della foresta (gx=21..22):
    // Solo questo tipo di albero ha l'ombra calpestabile verso l'apertura.
    // - L'albero stesso (gy = 9 e gy = 10) è RIGOROSAMENTE SOLIDO (non si può entrare nel tronco).
    // - La sua ombra (gy = 11 e gy = 12) è TOTALMENTE CALPESTABILE!
    dsGridType[9][21]  = DS_TILE_SOLID;
    dsGridType[9][22]  = DS_TILE_SOLID;
    dsGridType[10][21] = DS_TILE_SOLID;
    dsGridType[10][22] = DS_TILE_SOLID;
    dsGridType[11][21] = DS_TILE_WALKABLE;
    dsGridType[11][22] = DS_TILE_WALKABLE;
    dsGridType[12][21] = DS_TILE_WALKABLE;
    dsGridType[12][22] = DS_TILE_WALKABLE;

    // 5. Scale (newstep_lm21, gruppo 9): sempre calpestabili e permettono cambio quota
    for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
        for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
            float ox, oy, z_hit;
            gridToObjCenter((float)gx, (float)gy, &ox, &oy);
            for (u32 i = 0; i + 2 < dsGroups[9].vertexCount; i += 3) {
                const float* v0 = &dsGroups[9].verts[(i + 0) * 5];
                const float* v1 = &dsGroups[9].verts[(i + 1) * 5];
                const float* v2 = &dsGroups[9].verts[(i + 2) * 5];
                if (pointInTriXY(ox, oy, v0, v1, v2, &z_hit)) {
                    dsGridType[gy][gx]   = DS_TILE_STAIRS;
                    dsGridHeight[gy][gx] = z_hit;
                }
            }
        }
    }
}

void dsMapInit(void) {
    if (dsReady) return;
    dsMapFillGroups(dsGroups);

    // 1. Limiti del modello
    float minX = 1e30f, maxX = -1e30f;
    float minY = 1e30f, maxY = -1e30f;
    float minZ = 1e30f;
    dsVertexCount = 0;
    for (u32 g = 0; g < DS_MAP_GROUP_COUNT; ++g) {
        dsFirst[g] = dsVertexCount;
        for (u32 i = 0; i < dsGroups[g].vertexCount; ++i) {
            const float* v = &dsGroups[g].verts[i * 5];
            if (v[0] < minX) minX = v[0];
            if (v[0] > maxX) maxX = v[0];
            if (v[1] < minY) minY = v[1];
            if (v[1] > maxY) maxY = v[1];
            if (v[2] < minZ) minZ = v[2];
        }
        dsVertexCount += dsGroups[g].vertexCount;
    }
    if (dsVertexCount == 0) return;

    dsCenterX = (minX + maxX) * 0.5f;
    dsCenterY = (minY + maxY) * 0.5f;
    dsMinZ    = minZ;

    float extent = (maxX - minX > maxY - minY) ? (maxX - minX) : (maxY - minY);
    if (extent <= 0.0f) extent = 1.0f;
    dsScale = DS_FIT_SIZE / extent;

    // 2. Buffer dei vertici della mappa, del giocatore (+6) e dell'overlay fiori (+24) in linear RAM
    dsPlayerFirst = dsVertexCount;
    dsFlowerFirst = dsVertexCount + 6;
    dsVbo = (TileVertex*)linearAlloc((dsVertexCount + 6 + 24) * sizeof(TileVertex));
    if (!dsVbo) return;

    for (u32 g = 0; g < DS_MAP_GROUP_COUNT; ++g) {
        for (u32 i = 0; i < dsGroups[g].vertexCount; ++i) {
            const float* s = &dsGroups[g].verts[i * 5];
            TileVertex* d = &dsVbo[dsFirst[g] + i];
            d->x  = (s[0] - dsCenterX) * dsScale;
            d->y  = (s[2] - dsMinZ) * dsScale;
            d->z  = -(s[1] - dsCenterY) * dsScale;
            d->u  = s[3];
            d->v  = s[4];
            d->ox = 0.0f;
            d->oy = 0.0f;
        }
    }
    GSPGPU_FlushDataCache(dsVbo, dsVertexCount * sizeof(TileVertex));

    // 3. Texture dei materiali della mappa DS
    for (u32 g = 0; g < DS_MAP_GROUP_COUNT; ++g) {
        Tex3DS_Texture t3x = Tex3DS_TextureImport(dsGroups[g].tex, dsGroups[g].texSize, &dsTex[g], NULL, false);
        if (t3x) Tex3DS_TextureFree(t3x);
        C3D_TexSetFilter(&dsTex[g], GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&dsTex[g], GPU_REPEAT, GPU_REPEAT);
    }

    // 4. Texture del giocatore per disegnarlo dentro la mappa DS
    Tex3DS_Texture pt3x = Tex3DS_TextureImport(player_t3x, player_t3x_size, &dsPlayerTex, NULL, false);
    if (pt3x) {
        Tex3DS_TextureFree(pt3x);
        C3D_TexSetFilter(&dsPlayerTex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&dsPlayerTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        dsPlayerTexReady = true;
    }

    // 5. Texture overlay della metà anteriore dei fiori (per coprire i piedi del personaggio)
    Tex3DS_Texture ft3x = Tex3DS_TextureImport(flower_overlay_t3x, flower_overlay_t3x_size, &dsFlowerTex, NULL, false);
    if (ft3x) {
        Tex3DS_TextureFree(ft3x);
        C3D_TexSetFilter(&dsFlowerTex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&dsFlowerTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        dsFlowerTexReady = true;
    }

    // 6. Genera la griglia di collisione (con filtro tronchi/foglie/entrate)
    dsBuildCollisionGrid();

    dsReady = true;
}

void dsMapExit(void) {
    if (dsReady) {
        for (u32 g = 0; g < DS_MAP_GROUP_COUNT; ++g) {
            C3D_TexDelete(&dsTex[g]);
        }
        if (dsPlayerTexReady) {
            C3D_TexDelete(&dsPlayerTex);
            dsPlayerTexReady = false;
        }
        if (dsFlowerTexReady) {
            C3D_TexDelete(&dsFlowerTex);
            dsFlowerTexReady = false;
        }
    }
    if (dsVbo) {
        linearFree(dsVbo);
        dsVbo = NULL;
    }
    dsReady = false;
}

bool dsMapCanMove(int from_gx, int from_gy, int to_gx, int to_gy) {
    if (to_gx < 0 || to_gx >= DS_GRID_SIZE || to_gy < 0 || to_gy >= DS_GRID_SIZE) {
        return false;
    }
    DsTileType destType = dsGridType[to_gy][to_gx];
    if (destType == DS_TILE_VOID || destType == DS_TILE_SOLID) {
        return false;
    }

    // Se una delle due celle è una scala (newstep_lm21), consenti il cambio di quota
    DsTileType fromType = dsGridType[from_gy][from_gx];
    if (fromType == DS_TILE_STAIRS || destType == DS_TILE_STAIRS) {
        return true;
    }

    // Altrimenti consenti il movimento solo sullo stesso livello di altezza
    float h0 = dsGridHeight[from_gy][from_gx];
    float h1 = dsGridHeight[to_gy][to_gx];
    return fabsf(h0 - h1) <= 0.6f;
}

float dsMapGetWorldPos(float grid_x, float grid_y, float* out_x, float* out_y, float* out_z) {
    float ox, oy;
    gridToObjCenter(grid_x, grid_y, &ox, &oy);

    // Calcola l'altezza Z dell'OBJ interpolando sulla cella corrente (inclusi i gradini)
    int gx = (int)floorf(grid_x + 0.5f);
    int gy = (int)floorf(grid_y + 0.5f);
    if (gx < 0) gx = 0; else if (gx >= DS_GRID_SIZE) gx = DS_GRID_SIZE - 1;
    if (gy < 0) gy = 0; else if (gy >= DS_GRID_SIZE) gy = DS_GRID_SIZE - 1;

    // Sull'ombra dell'entrata della foresta (gx=21..22, gy=11), allinea il personaggio
    // al centro della chiazza d'ombra circolare evitando che sporga verso l'albero
    if ((gx == 21 || gx == 22) && gy == 11) {
        oy = 4.85f;
    }

    float obj_z = dsGridHeight[gy][gx];

    // Se siamo sulle scale, cerca l'altezza esatta sul triangolo di newstep_lm21 e alza il personaggio
    float stair_z;
    bool on_stair = false;
    for (u32 i = 0; i + 2 < dsGroups[9].vertexCount; i += 3) {
        const float* v0 = &dsGroups[9].verts[(i + 0) * 5];
        const float* v1 = &dsGroups[9].verts[(i + 1) * 5];
        const float* v2 = &dsGroups[9].verts[(i + 2) * 5];
        if (pointInTriXY(ox, oy, v0, v1, v2, &stair_z)) {
            obj_z = stair_z + 0.35f;
            on_stair = true;
            break;
        }
    }
    if (!on_stair && dsGridType[gy][gx] == DS_TILE_STAIRS) {
        obj_z += 0.35f;
    }

    float wx = (ox - dsCenterX) * dsScale;
    float wy = (obj_z - dsMinZ) * dsScale;
    float wz = -(oy - dsCenterY) * dsScale;

    if (out_x) *out_x = wx;
    if (out_y) *out_y = wy;
    if (out_z) *out_z = wz;
    return wy;
}

void dsMapUpdate(u32 kHeld, u32 kDown) {
    // L/R permette ancora di fare zoom in/out sul personaggio, X ripristina lo zoom standard
    if (kDown & KEY_X) dsDist = DS_CAM_DIST_START;
    if (kHeld & KEY_L) dsDist -= DS_ZOOM_STEP;
    if (kHeld & KEY_R) dsDist += DS_ZOOM_STEP;
    if (dsDist < DS_CAM_DIST_MIN) dsDist = DS_CAM_DIST_MIN;
    if (dsDist > DS_CAM_DIST_MAX) dsDist = DS_CAM_DIST_MAX;

    // Muove il giocatore sulla mappa DS (D-Pad o Circle Pad)
    playerUpdate(kDown, kHeld);
}

void dsMapDrawFrame(C3D_Mtx* projection, int uLoc_projection, int uLoc_view) {
    float px, py, pz;
    playerGetWorldPos(&px, &py, &pz);

    // La telecamera segue il giocatore con la stessa inclinazione della mappa a tile
    float rad   = CAM_ANGLE_DEG * (M_PI / 180.0f);
    float eye_y = dsDist * sinf(rad);
    float eye_z = dsDist * cosf(rad);

    Mtx_LookAt(&dsView,
        FVec3_New(px, py + eye_y, pz + eye_z),
        FVec3_New(px, py,         pz),
        FVec3_New(0.0f, 1.0f, 0.0f),
        false
    );

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_view, &dsView);

    // Z-buffer attivo: le foglie degli alberi (più alte e più vicine alla camera)
    // coprono automaticamente il giocatore quando ci cammina sotto!
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    C3D_BufInfo* buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, dsVbo, sizeof(TileVertex), 3, 0x210);

    // 1. Disegna tutti i materiali della mappa DS
    for (u32 g = 0; g < DS_MAP_GROUP_COUNT; ++g) {
        if (dsGroups[g].vertexCount == 0) continue;
        C3D_TexBind(0, &dsTex[g]);
        C3D_DrawArrays(GPU_TRIANGLES, (int)dsFirst[g], (int)dsGroups[g].vertexCount);
    }

    // 2. Costruisce e disegna il quad 3D del giocatore sulla mappa DS (allocato in coda a dsVbo)
    if (dsPlayerTexReady && dsVbo) {
        playerBuildQuad(&dsVbo[dsPlayerFirst]);
        GSPGPU_FlushDataCache(&dsVbo[dsPlayerFirst], 6 * sizeof(TileVertex));

        C3D_TexBind(0, &dsPlayerTex);
        C3D_DrawArrays(GPU_TRIANGLES, (int)dsPlayerFirst, 6);
    }

    // 3. Disegna l'overlay dei fiori in primo piano (solo la metà anteriore, fiore 2)
    //    quando il giocatore si trova sopra una cella con fiori
    if (dsFlowerTexReady && dsVbo) {
        float pgx, pgy;
        playerGetGridPos(&pgx, &pgy);

        int min_gx = (int)floorf(pgx - 0.6f);
        int max_gx = (int)floorf(pgx + 0.6f);
        int min_gy = (int)floorf(pgy - 0.6f);
        int max_gy = (int)floorf(pgy + 0.6f);

        u32 flowerQuads = 0;
        for (int gy = min_gy; gy <= max_gy; ++gy) {
            for (int gx = min_gx; gx <= max_gx; ++gx) {
                if (gx < 0 || gx >= DS_GRID_SIZE || gy < 0 || gy >= DS_GRID_SIZE) continue;
                if (!dsGridHasFlowers[gy][gx]) continue;
                if (flowerQuads >= 4) break;

                // Controllo Z:
                // Se il personaggio ha meno Z (è davanti al fiore, pgy > gy + 0.35f),
                // tutto il fiore deve rimanere DIETRO al personaggio (nessun overlay in primo piano).
                // Se hanno la stessa Z (pgy circa uguale a gy, entro [-0.45, +0.35]),
                // mostra la metà anteriore del fiore davanti al personaggio.
                if (pgy > (float)gy + 0.35f) {
                    continue; // Personaggio davanti al fiore: tutto il fiore resta dietro
                }
                if (pgy < (float)gy - 0.45f) {
                    continue; // Personaggio dietro al fiore
                }
                if (fabsf(pgx - (float)gx) > 0.85f) {
                    continue; // Non sovrapposto orizzontalmente
                }

                float ox, oy;
                gridToObjCenter((float)gx, (float)gy, &ox, &oy);
                float twx = (ox - dsCenterX) * dsScale;
                float twy = (dsGridHeight[gy][gx] - dsMinZ) * dsScale;
                float twz = -(oy - dsCenterY) * dsScale;

                float half_s = 0.5f * dsScale;
                float x_left  = twx - half_s;
                float x_right = twx + half_s;
                float z_north = twz - half_s;
                float z_south = twz + half_s;

                TileVertex* fv = &dsVbo[dsFlowerFirst + flowerQuads * 6];
                fv[0] = (TileVertex){x_left,  twy, z_south, 0.0f, 0.0f, 0.0f, 0.0f};
                fv[1] = (TileVertex){x_right, twy, z_south, 1.0f, 0.0f, 0.0f, 0.0f};
                fv[2] = (TileVertex){x_right, twy, z_north, 1.0f, 1.0f, 0.0f, 0.0f};

                fv[3] = (TileVertex){x_left,  twy, z_south, 0.0f, 0.0f, 0.0f, 0.0f};
                fv[4] = (TileVertex){x_right, twy, z_north, 1.0f, 1.0f, 0.0f, 0.0f};
                fv[5] = (TileVertex){x_left,  twy, z_north, 0.0f, 1.0f, 0.0f, 0.0f};

                flowerQuads++;
            }
        }

        if (flowerQuads > 0) {
            GSPGPU_FlushDataCache(&dsVbo[dsFlowerFirst], flowerQuads * 6 * sizeof(TileVertex));

            C3D_DepthTest(true, GPU_ALWAYS, GPU_WRITE_ALL);
            C3D_TexBind(0, &dsFlowerTex);
            C3D_DrawArrays(GPU_TRIANGLES, (int)dsFlowerFirst, (int)(flowerQuads * 6));
            C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
        }
    }
}