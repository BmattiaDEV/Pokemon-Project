#include "ds_map_internal.h"
#include "player.h"
#include "npc.h"

static inline bool isPlayerBehindSingleTree(const DsSingleTree* tr, float pgx, float pgy) {
    return (pgx >= (float)tr->gx_min - 0.45f &&
            pgx <= (float)tr->gx_max + 0.45f &&
            pgy >= (float)(tr->gy_north - 1) - 0.45f &&
            pgy <= (float)tr->gy_north + 0.45f);
}

static void buildTileOverlayQuad(TileVertex* fv, int gx, int gy, float y_north_off, float y_south_off) {
    float ox, oy;
    dsMapGridToObjCenter((float)gx, (float)gy, &ox, &oy);
    float twx = (ox - g_dsMap.centerX) * g_dsMap.scale;
    float twy = (g_dsMap.gridHeight[gy][gx] - g_dsMap.minZ) * g_dsMap.scale;
    float twz = -(oy - g_dsMap.centerY) * g_dsMap.scale;

    float half_s  = 0.5f * g_dsMap.scale;
    float x_left  = twx - half_s;
    float x_right = twx + half_s;
    float z_north = twz - half_s;
    float z_south = twz + half_s;

    float y_north = twy + y_north_off;
    float y_south = twy + y_south_off;

    fv[0] = (TileVertex){x_left,  y_south, z_south, 0.0f, 0.0f, 0.0f, 0.0f};
    fv[1] = (TileVertex){x_right, y_south, z_south, 1.0f, 0.0f, 0.0f, 0.0f};
    fv[2] = (TileVertex){x_right, y_north, z_north, 1.0f, 1.0f, 0.0f, 0.0f};

    fv[3] = (TileVertex){x_left,  y_south, z_south, 0.0f, 0.0f, 0.0f, 0.0f};
    fv[4] = (TileVertex){x_right, y_north, z_north, 1.0f, 1.0f, 0.0f, 0.0f};
    fv[5] = (TileVertex){x_left,  y_north, z_north, 0.0f, 1.0f, 0.0f, 0.0f};
}

static void drawMapGroupsOpaquePass(int flower_frame, float pgx, float pgy, bool* out_hasFadedTrees) {
    bool hasFaded = false;

    for (u32 g = 0; g < DS_MAP_GROUP_COUNT; ++g) {
        if (g_dsMap.groups[g].vertexCount == 0) continue;

        if (g == 12 && g_dsMap.flowerAnimReady) {
            C3D_TexBind(0, &g_dsMap.rhanaTex[flower_frame]);
        } else {
            C3D_TexBind(0, &g_dsMap.tex[g]);
        }

        if (g == 1 && g_dsMap.treeCount > 0) {
            for (u32 t = 0; t < g_dsMap.treeCount; ++t) {
                if (isPlayerBehindSingleTree(&g_dsMap.trees[t], pgx, pgy)) {
                    hasFaded = true;
                    continue;
                }
                C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.trees[t].firstVertex, 24);
            }
        } else if (g == 3 && g_dsMap.contTreeCount > 0) {
            for (u32 t = 0; t < g_dsMap.contTreeCount; ++t) {
                if (isPlayerBehindSingleTree(&g_dsMap.contTrees[t], pgx, pgy)) {
                    hasFaded = true;
                    continue;
                }
                C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.contTrees[t].firstVertex, 12);
            }
        } else if (g == 4 && g_dsMap.contTreeCount > 0) {
            for (u32 t = 0; t < g_dsMap.contTreeCount; ++t) {
                if (isPlayerBehindSingleTree(&g_dsMap.contTrees[t], pgx, pgy)) {
                    hasFaded = true;
                    continue;
                }
                C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.contTrees[t].secondVertex, 12);
            }
        } else {
            C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.first[g], (int)g_dsMap.groups[g].vertexCount);
        }
    }

    *out_hasFadedTrees = hasFaded;
}

static void drawFootOverlaysPass(int flower_frame) {
    if (!g_dsMap.vbo || (!g_dsMap.flowerTexReady && !g_dsMap.grassTexReady)) return;

    u32 flowerQuads = 0;
    u32 grassQuads  = 0;

    for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
        for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
            bool hasFlower = g_dsMap.flowerTexReady && g_dsMap.gridHasFlowers[gy][gx];
            bool hasGrass  = g_dsMap.grassTexReady  && g_dsMap.gridHasGrass[gy][gx];
            if (!hasFlower && !hasGrass) continue;

            bool overPlayer = playerShouldOverlayTile(gx, gy);
            bool overNpc    = npcShouldOverlayTile(gx, gy) && !playerIsAt(gx, gy + 1) && !npcIsAt(gx, gy + 1);
            if (!overPlayer && !overNpc) continue;

            if (hasFlower && flowerQuads < 4) {
                buildTileOverlayQuad(&g_dsMap.vbo[g_dsMap.flowerFirst + flowerQuads * 6], gx, gy, 0.0f, 0.0f);
                flowerQuads++;
            }
            if (hasGrass && grassQuads < 4) {
                buildTileOverlayQuad(&g_dsMap.vbo[g_dsMap.grassFirst + grassQuads * 6], gx, gy, 1.2f, 4.5f);
                grassQuads++;
            }
        }
    }

    if (flowerQuads > 0 || grassQuads > 0) {
        C3D_BufInfo* buf = C3D_GetBufInfo();
        BufInfo_Init(buf);
        BufInfo_Add(buf, g_dsMap.vbo, sizeof(TileVertex), 3, 0x210);
    }

    if (flowerQuads > 0) {
        GSPGPU_FlushDataCache(&g_dsMap.vbo[g_dsMap.flowerFirst], flowerQuads * 6 * sizeof(TileVertex));

        C3D_DepthTest(true, GPU_ALWAYS, GPU_WRITE_COLOR);
        if (g_dsMap.flowerAnimReady) {
            C3D_TexBind(0, &g_dsMap.flowerOverlayTex[flower_frame]);
        } else {
            C3D_TexBind(0, &g_dsMap.flowerTex);
        }
        C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.flowerFirst, (int)(flowerQuads * 6));
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    }

    if (grassQuads > 0) {
        GSPGPU_FlushDataCache(&g_dsMap.vbo[g_dsMap.grassFirst], grassQuads * 6 * sizeof(TileVertex));

        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_COLOR);
        C3D_TexBind(0, &g_dsMap.grassTex);
        C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.grassFirst, (int)(grassQuads * 6));
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    }
}

static void drawFadedTreesPass(float pgx, float pgy) {
    C3D_BufInfo* buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, g_dsMap.vbo, sizeof(TileVertex), 3, 0x210);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, 0);
    C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
    C3D_TexEnvColor(env, 0x80FFFFFF);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    // 1. Alberi singoli (gruppo 1) davanti al giocatore
    C3D_TexBind(0, &g_dsMap.tex[1]);
    for (u32 t = 0; t < g_dsMap.treeCount; ++t) {
        if (isPlayerBehindSingleTree(&g_dsMap.trees[t], pgx, pgy)) {
            C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.trees[t].firstVertex, 24);
        }
    }

    // 2. Singole sezioni 2x2 di alberi continui (gruppi 4 e 3) davanti al giocatore
    for (u32 t = 0; t < g_dsMap.contTreeCount; ++t) {
        if (isPlayerBehindSingleTree(&g_dsMap.contTrees[t], pgx, pgy)) {
            C3D_TexBind(0, &g_dsMap.tex[4]);
            C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.contTrees[t].secondVertex, 12);
            C3D_TexBind(0, &g_dsMap.tex[3]);
            C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.contTrees[t].firstVertex, 12);
        }
    }

    // Ripristina TexEnv(0) in modalità opaca standard (GPU_REPLACE)
    env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void dsMapDrawFrame(C3D_Mtx* projection, int uLoc_projection, int uLoc_view) {
    if (!g_dsMap.ready || !g_dsMap.vbo) return;

    float px, py, pz;
    playerGetWorldPos(&px, &py, &pz);

    float rad   = CAM_ANGLE_DEG * (M_PI / 180.0f);
    float eye_y = g_dsMap.camDist * sinf(rad);
    float eye_z = g_dsMap.camDist * cosf(rad);

    Mtx_LookAt(&g_dsMap.view,
        FVec3_New(px, py + eye_y, pz + eye_z),
        FVec3_New(px, py,         pz),
        FVec3_New(0.0f, 1.0f, 0.0f),
        false
    );

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_view, &g_dsMap.view);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    // Aggiorna onde 3D e scorrimento UV dell'acqua nel VBO
    dsMapAnimUpdateWaterVbo();

    C3D_BufInfo* buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, g_dsMap.vbo, sizeof(TileVertex), 3, 0x210);

    int flower_frame = dsMapAnimGetFlowerFrame();
    float pgx, pgy;
    playerGetGridPos(&pgx, &pgy);
    bool hasFadedTrees = false;

    // 1. Passata opaca di tutti i gruppi della mappa (saltando gli alberi davanti al giocatore)
    drawMapGroupsOpaquePass(flower_frame, pgx, pgy, &hasFadedTrees);

    // 2. Disegna il giocatore
    if (g_dsMap.playerTexReady) {
        playerBuildQuad(&g_dsMap.vbo[g_dsMap.playerFirst]);
        GSPGPU_FlushDataCache(&g_dsMap.vbo[g_dsMap.playerFirst], 6 * sizeof(TileVertex));

        C3D_TexBind(0, &g_dsMap.playerTex);
        C3D_DrawArrays(GPU_TRIANGLES, (int)g_dsMap.playerFirst, 6);
    }

    // 3. Disegna gli NPC
    npcDrawAll(px, py, pz, g_dsMap.camDist);

    // 4. Disegna gli overlay di fiori ed erba alta sui piedi
    drawFootOverlaysPass(flower_frame);

    // 5. Disegna in semitrasparenza i singoli alberi davanti al giocatore
    if (hasFadedTrees) {
        drawFadedTreesPass(pgx, pgy);
    }
}
