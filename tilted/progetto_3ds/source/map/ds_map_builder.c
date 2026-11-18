#include "ds_map_internal.h"
#include "npc.h"

// Test 2D punto-in-triangolo sul piano XY dell'OBJ (restituisce anche la quota Z interpolata)
static bool pointInTriXY(
    float px, float py,
    const float* a, const float* b, const float* c,
    float* out_z)
{
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

// Suddivide una striscia orizzontale di alberi continui (conttree_b / conttree_t)
// nel k-esimo segmento singolo 2x2 (su numSlices alberi totali nella striscia)
static void buildContTreeSliceQuad(TileVertex* dst, const float* qv, float minX, float maxX, int k, int numSlices) {
    float u_at_minX = 0.0f, u_at_maxX = 0.0f;
    for (int v = 0; v < 6; ++v) {
        const float* pt = &qv[v * 5];
        if (fabsf(pt[0] - minX) < 0.05f) u_at_minX = pt[3];
        if (fabsf(pt[0] - maxX) < 0.05f) u_at_maxX = pt[3];
    }

    float x0 = minX + 2.0f * (float)k;
    float x1 = x0 + 2.0f;
    float u0 = u_at_minX + (u_at_maxX - u_at_minX) * ((float)k / (float)numSlices);
    float u1 = u_at_minX + (u_at_maxX - u_at_minX) * ((float)(k + 1) / (float)numSlices);

    for (int v = 0; v < 6; ++v) {
        const float* s = &qv[v * 5];
        bool isLeft = (fabsf(s[0] - minX) < 0.05f);
        float sx = isLeft ? x0 : x1;
        float su = isLeft ? u0 : u1;
        dst[v].x  = (sx   - g_dsMap.centerX) * g_dsMap.scale;
        dst[v].y  = (s[2] - g_dsMap.minZ)    * g_dsMap.scale;
        dst[v].z  = -(s[1] - g_dsMap.centerY) * g_dsMap.scale;
        dst[v].u  = su;
        dst[v].v  = s[4];
        dst[v].ox = 0.0f;
        dst[v].oy = 0.0f;
    }
}

bool dsMapBuilderBuildVbo(void) {
    float minX = 1e30f, maxX = -1e30f;
    float minY = 1e30f, maxY = -1e30f;
    float minZ = 1e30f;

    g_dsMap.vertexCount = 0;
    for (u32 g = 0; g < g_dsMap.groupCount; ++g) {
        g_dsMap.first[g] = g_dsMap.vertexCount;
        for (u32 i = 0; i < g_dsMap.groups[g].vertexCount; ++i) {
            const float* v = &g_dsMap.groups[g].verts[i * 5];
            if (v[0] < minX) minX = v[0];
            if (v[0] > maxX) maxX = v[0];
            if (v[1] < minY) minY = v[1];
            if (v[1] > maxY) maxY = v[1];
            if (v[2] < minZ) minZ = v[2];
        }
        g_dsMap.vertexCount += g_dsMap.groups[g].vertexCount;
    }
    if (g_dsMap.vertexCount == 0) return false;

    g_dsMap.centerX = (minX + maxX) * 0.5f;
    g_dsMap.centerY = (minY + maxY) * 0.5f;
    g_dsMap.minZ    = minZ;

    float extent = (maxX - minX > maxY - minY) ? (maxX - minX) : (maxY - minY);
    if (extent <= 0.0f) extent = 1.0f;
    g_dsMap.scale = DS_FIT_SIZE / extent;

    // Alloca VBO: geometria mappa + giocatore (6) + overlay fiori (24) + overlay erba (24) + fettine conttree (MAX_CONT_TREES*24)
    g_dsMap.playerFirst   = g_dsMap.vertexCount;
    g_dsMap.flowerFirst   = g_dsMap.vertexCount + 6;
    g_dsMap.grassFirst    = g_dsMap.vertexCount + 6 + 24;
    g_dsMap.contTreeFirst = g_dsMap.vertexCount + 6 + 24 + 24;

    u32 totalVerts = g_dsMap.vertexCount + 6 + 24 + 24 + MAX_CONT_TREES * 24;
    g_dsMap.vbo = (TileVertex*)linearAlloc(totalVerts * sizeof(TileVertex));
    if (!g_dsMap.vbo) return false;

    for (u32 g = 0; g < g_dsMap.groupCount; ++g) {
        for (u32 i = 0; i < g_dsMap.groups[g].vertexCount; ++i) {
            const float* s = &g_dsMap.groups[g].verts[i * 5];
            TileVertex* d = &g_dsMap.vbo[g_dsMap.first[g] + i];
            d->x  = (s[0] - g_dsMap.centerX) * g_dsMap.scale;
            d->y  = (s[2] - g_dsMap.minZ)    * g_dsMap.scale;
            d->z  = -(s[1] - g_dsMap.centerY) * g_dsMap.scale;
            d->u  = s[3];
            d->v  = s[4];
            d->ox = 0.0f;
            d->oy = 0.0f;
        }
    }

    GSPGPU_FlushDataCache(g_dsMap.vbo, g_dsMap.vertexCount * sizeof(TileVertex));
    return true;
}

static bool isWalkableGroundRole(DsMaterialRole role) {
    return (role == DS_MAT_GROUND_WALK ||
            role == DS_MAT_FLOWER_ANIM ||
            role == DS_MAT_FLOWER_FLAT ||
            role == DS_MAT_WARP_SHADOW ||
            role == DS_MAT_WILD_GRASS);
}

void dsMapBuilderBuildCollisionGrid(void) {
    for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
        for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
            g_dsMap.gridType[gy][gx]       = DS_TILE_VOID;
            g_dsMap.gridHeight[gy][gx]     = 2.0f;
            g_dsMap.gridHasFlowers[gy][gx] = false;
            g_dsMap.gridHasGrass[gy][gx]   = false;
            g_dsMap.gridHasShadow[gy][gx]  = false;
        }
    }

    // 1. Terreno calpestabile (erba, sabbia, fiori, entrata foresta, erba alta, sentiero e spiaggia)
    for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
        for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
            float ox, oy, z_hit;
            dsMapGridToObjCenter((float)gx, (float)gy, &ox, &oy);

            for (u32 g = 0; g < g_dsMap.groupCount; ++g) {
                DsMaterialRole role = g_dsMap.groups[g].role;
                if (!isWalkableGroundRole(role)) continue;

                for (u32 i = 0; i + 2 < g_dsMap.groups[g].vertexCount; i += 3) {
                    const float* v0 = &g_dsMap.groups[g].verts[(i + 0) * 5];
                    const float* v1 = &g_dsMap.groups[g].verts[(i + 1) * 5];
                    const float* v2 = &g_dsMap.groups[g].verts[(i + 2) * 5];
                    if (pointInTriXY(ox, oy, v0, v1, v2, &z_hit)) {
                        g_dsMap.gridType[gy][gx]   = DS_TILE_WALKABLE;
                        g_dsMap.gridHeight[gy][gx] = roundf(z_hit);
                        if (role == DS_MAT_FLOWER_ANIM) g_dsMap.gridHasFlowers[gy][gx] = true;
                        if (role == DS_MAT_WARP_SHADOW) g_dsMap.gridHasShadow[gy][gx]  = true;
                        if (role == DS_MAT_WILD_GRASS)  g_dsMap.gridHasGrass[gy][gx]   = true;
                    }
                }
            }

            if (g_dsMap.beachGroup >= 0) {
                const DsMapGroupData* bg = &g_dsMap.groups[g_dsMap.beachGroup];
                for (u32 i = 0; i + 2 < bg->vertexCount; i += 3) {
                    const float* v0 = &bg->verts[(i + 0) * 5];
                    const float* v1 = &bg->verts[(i + 1) * 5];
                    const float* v2 = &bg->verts[(i + 2) * 5];
                    if (pointInTriXY(ox, oy, v0, v1, v2, &z_hit) && z_hit >= 0.95f) {
                        if (g_dsMap.gridType[gy][gx] == DS_TILE_VOID) {
                            g_dsMap.gridType[gy][gx]   = DS_TILE_WALKABLE;
                            g_dsMap.gridHeight[gy][gx] = 1.0f;
                        }
                    }
                }
            }
        }
    }

    // 2. Ostacoli solidi (rocce, scogliere, recinzioni imped_lm53, bordi esterni)
    for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
        for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
            float ox, oy, z_hit;
            dsMapGridToObjCenter((float)gx, (float)gy, &ox, &oy);

            for (u32 g = 0; g < g_dsMap.groupCount; ++g) {
                if (g_dsMap.groups[g].role != DS_MAT_SOLID_CLIFF) continue;

                for (u32 i = 0; i + 2 < g_dsMap.groups[g].vertexCount; i += 3) {
                    const float* v0 = &g_dsMap.groups[g].verts[(i + 0) * 5];
                    const float* v1 = &g_dsMap.groups[g].verts[(i + 1) * 5];
                    const float* v2 = &g_dsMap.groups[g].verts[(i + 2) * 5];
                    if (pointInTriXY(ox, oy, v0, v1, v2, &z_hit)) {
                        g_dsMap.gridType[gy][gx] = DS_TILE_SOLID;
                    }
                }
            }
        }
    }

    // 3. Alberi singoli (DS_MAT_TREE_SINGLE: 24 vertici / 4 quad per albero)
    g_dsMap.treeCount = 0;
    if (g_dsMap.treeSingleGroup >= 0) {
        int tg = g_dsMap.treeSingleGroup;
        const DsMapGroupData* tgd = &g_dsMap.groups[tg];

        for (u32 i = 0; i + 23 < tgd->vertexCount; i += 24) {
            const float* b0 = &tgd->verts[(i + 18) * 5];
            const float* b1 = &tgd->verts[(i + 19) * 5];
            const float* b2 = &tgd->verts[(i + 20) * 5];

            float minX = fminf(b0[0], fminf(b1[0], b2[0]));
            float maxX = fmaxf(b0[0], fmaxf(b1[0], b2[0]));
            float minY = fminf(b0[1], fminf(b1[1], b2[1]));

            int tree_gx_min = DS_GRID_SIZE, tree_gx_max = -1;
            int tree_gy_north = DS_GRID_SIZE, tree_gy_south = -1;

            for (float x = minX + 0.5f; x < maxX; x += 1.0f) {
                int gx, gy;
                if (dsMapObjToGrid(x, minY + 0.4f, &gx, &gy)) {
                    g_dsMap.gridType[gy][gx] = DS_TILE_SOLID;
                    if (gx < tree_gx_min) tree_gx_min = gx;
                    if (gx > tree_gx_max) tree_gx_max = gx;
                    tree_gy_south = gy;
                }
                if (dsMapObjToGrid(x, minY + 1.4f, &gx, &gy)) {
                    g_dsMap.gridType[gy][gx] = DS_TILE_SOLID;
                    if (gx < tree_gx_min) tree_gx_min = gx;
                    if (gx > tree_gx_max) tree_gx_max = gx;
                    tree_gy_north = gy;
                }
            }

            if (g_dsMap.treeCount < MAX_SINGLE_TREES && tree_gx_min <= tree_gx_max && tree_gy_north <= tree_gy_south) {
                g_dsMap.trees[g_dsMap.treeCount++] = (DsSingleTree){
                    .firstVertex  = g_dsMap.first[tg] + i,
                    .secondVertex = 0,
                    .gx_min       = tree_gx_min,
                    .gx_max       = tree_gx_max,
                    .gy_north     = tree_gy_north,
                    .gy_south     = tree_gy_south
                };
            }
        }
    }

    // 4. Alberi continui (DS_MAT_CONTTREE_BOT + DS_MAT_CONTTREE_TOP)
    g_dsMap.contTreeCount = 0;
    if (g_dsMap.contTreeBotGroup >= 0) {
        int bg = g_dsMap.contTreeBotGroup;
        int tg = g_dsMap.contTreeTopGroup;
        const DsMapGroupData* cbot = &g_dsMap.groups[bg];
        const DsMapGroupData* ctop = (tg >= 0) ? &g_dsMap.groups[tg] : NULL;

        for (u32 s = 0; s + 11 < cbot->vertexCount; s += 12) {
            float minX = 1e30f, maxX = -1e30f;
            float minY = 1e30f;
            for (int v = 0; v < 12; ++v) {
                const float* pt = &cbot->verts[(s + v) * 5];
                if (pt[0] < minX) minX = pt[0];
                if (pt[0] > maxX) maxX = pt[0];
                if (pt[1] < minY) minY = pt[1];
            }

            for (float x = minX + 0.5f; x < maxX; x += 1.0f) {
                int gx, gy;
                if (dsMapObjToGrid(x, minY + 0.4f, &gx, &gy)) {
                    g_dsMap.gridType[gy][gx] = DS_TILE_SOLID;
                }
                if (dsMapObjToGrid(x, minY + 1.4f, &gx, &gy)) {
                    g_dsMap.gridType[gy][gx] = DS_TILE_SOLID;
                }
            }

            int numSlices = (int)roundf((maxX - minX) / 2.0f);
            if (numSlices < 1) numSlices = 1;

            for (int k = 0; k < numSlices; ++k) {
                if (g_dsMap.contTreeCount >= MAX_CONT_TREES || !g_dsMap.vbo) break;

                float sliceMinX = minX + 2.0f * (float)k;
                int gx0 = 0, gx1 = 0, gy_s = 0, gy_n = 0;
                bool ok0 = dsMapObjToGrid(sliceMinX + 0.5f, minY + 0.4f, &gx0, &gy_s);
                bool ok1 = dsMapObjToGrid(sliceMinX + 1.5f, minY + 1.4f, &gx1, &gy_n);
                if (!ok0 || !ok1) continue;

                u32 baseVertB = g_dsMap.contTreeFirst + g_dsMap.contTreeCount * 24;
                u32 baseVertT = baseVertB + 12;

                buildContTreeSliceQuad(&g_dsMap.vbo[baseVertB + 0], &cbot->verts[(s + 0) * 5], minX, maxX, k, numSlices);
                buildContTreeSliceQuad(&g_dsMap.vbo[baseVertB + 6], &cbot->verts[(s + 6) * 5], minX, maxX, k, numSlices);

                if (ctop && s + 11 < ctop->vertexCount) {
                    buildContTreeSliceQuad(&g_dsMap.vbo[baseVertT + 0], &ctop->verts[(s + 0) * 5], minX, maxX, k, numSlices);
                    buildContTreeSliceQuad(&g_dsMap.vbo[baseVertT + 6], &ctop->verts[(s + 6) * 5], minX, maxX, k, numSlices);
                }

                g_dsMap.contTrees[g_dsMap.contTreeCount++] = (DsSingleTree){
                    .firstVertex  = baseVertB,
                    .secondVertex = baseVertT,
                    .gx_min       = (gx0 < gx1) ? gx0 : gx1,
                    .gx_max       = (gx0 > gx1) ? gx0 : gx1,
                    .gy_north     = (gy_n < gy_s) ? gy_n : gy_s,
                    .gy_south     = (gy_n > gy_s) ? gy_n : gy_s
                };
            }
        }
        if (g_dsMap.vbo && g_dsMap.contTreeCount > 0) {
            GSPGPU_FlushDataCache(&g_dsMap.vbo[g_dsMap.contTreeFirst], g_dsMap.contTreeCount * 24 * sizeof(TileVertex));
        }
    }

    // Passaggi evento foresta per mappa
    if (g_dsMap.mapId == DS_MAP_ROUTE) {
        g_dsMap.gridType[9][21]  = DS_TILE_SOLID;
        g_dsMap.gridType[9][22]  = DS_TILE_SOLID;
        g_dsMap.gridType[10][21] = DS_TILE_SOLID;
        g_dsMap.gridType[10][22] = DS_TILE_SOLID;
        g_dsMap.gridType[11][21] = DS_TILE_WALKABLE;
        g_dsMap.gridType[11][22] = DS_TILE_WALKABLE;
        g_dsMap.gridType[12][21] = DS_TILE_WALKABLE;
        g_dsMap.gridType[12][22] = DS_TILE_WALKABLE;
    } else if (g_dsMap.mapId == DS_MAP_CLEARING) {
        g_dsMap.gridType[26][16] = DS_TILE_WALKABLE;
        g_dsMap.gridType[26][17] = DS_TILE_WALKABLE;
        g_dsMap.gridType[27][16] = DS_TILE_WALKABLE;
        g_dsMap.gridType[27][17] = DS_TILE_WALKABLE;
        g_dsMap.gridType[28][16] = DS_TILE_SOLID;
        g_dsMap.gridType[28][17] = DS_TILE_SOLID;
        g_dsMap.gridType[29][16] = DS_TILE_SOLID;
        g_dsMap.gridType[29][17] = DS_TILE_SOLID;
    }

    // 5. Scale (DS_MAT_STAIRS)
    if (g_dsMap.stairsGroup >= 0) {
        const DsMapGroupData* sg = &g_dsMap.groups[g_dsMap.stairsGroup];
        for (int gy = 0; gy < DS_GRID_SIZE; ++gy) {
            for (int gx = 0; gx < DS_GRID_SIZE; ++gx) {
                float ox, oy, z_hit;
                dsMapGridToObjCenter((float)gx, (float)gy, &ox, &oy);
                for (u32 i = 0; i + 2 < sg->vertexCount; i += 3) {
                    const float* v0 = &sg->verts[(i + 0) * 5];
                    const float* v1 = &sg->verts[(i + 1) * 5];
                    const float* v2 = &sg->verts[(i + 2) * 5];
                    if (pointInTriXY(ox, oy, v0, v1, v2, &z_hit)) {
                        g_dsMap.gridType[gy][gx]   = DS_TILE_STAIRS;
                        g_dsMap.gridHeight[gy][gx] = z_hit;
                    }
                }
            }
        }
    }
}

bool dsMapCanMove(int from_gx, int from_gy, int to_gx, int to_gy) {
    if (to_gx < 0 || to_gx >= DS_GRID_SIZE || to_gy < 0 || to_gy >= DS_GRID_SIZE) {
        return false;
    }
    DsTileType destType = g_dsMap.gridType[to_gy][to_gx];
    if (destType == DS_TILE_VOID || destType == DS_TILE_SOLID) {
        return false;
    }
    if (npcIsAt(to_gx, to_gy)) {
        return false;
    }

    DsTileType fromType = g_dsMap.gridType[from_gy][from_gx];
    if (fromType == DS_TILE_STAIRS || destType == DS_TILE_STAIRS) {
        return true;
    }

    float h0 = g_dsMap.gridHeight[from_gy][from_gx];
    float h1 = g_dsMap.gridHeight[to_gy][to_gx];
    return fabsf(h0 - h1) <= 0.6f;
}

bool dsMapIsWildGrass(int gx, int gy) {
    if (gx < 0 || gx >= DS_GRID_SIZE || gy < 0 || gy >= DS_GRID_SIZE) {
        return false;
    }
    return g_dsMap.gridHasGrass[gy][gx];
}

float dsMapGetWorldPos(float grid_x, float grid_y, float* out_x, float* out_y, float* out_z) {
    float ox, oy;
    dsMapGridToObjCenter(grid_x, grid_y, &ox, &oy);

    int gx = (int)floorf(grid_x + 0.5f);
    int gy = (int)floorf(grid_y + 0.5f);
    if (gx < 0) gx = 0; else if (gx >= DS_GRID_SIZE) gx = DS_GRID_SIZE - 1;
    if (gy < 0) gy = 0; else if (gy >= DS_GRID_SIZE) gy = DS_GRID_SIZE - 1;

    float obj_z = g_dsMap.gridHeight[gy][gx];

    float stair_z;
    bool on_stair = false;
    if (g_dsMap.stairsGroup >= 0) {
        const DsMapGroupData* sg = &g_dsMap.groups[g_dsMap.stairsGroup];
        for (u32 i = 0; i + 2 < sg->vertexCount; i += 3) {
            const float* v0 = &sg->verts[(i + 0) * 5];
            const float* v1 = &sg->verts[(i + 1) * 5];
            const float* v2 = &sg->verts[(i + 2) * 5];
            if (pointInTriXY(ox, oy, v0, v1, v2, &stair_z)) {
                obj_z = stair_z + 0.35f;
                on_stair = true;
                break;
            }
        }
    }
    if (!on_stair && g_dsMap.gridType[gy][gx] == DS_TILE_STAIRS) {
        obj_z += 0.35f;
    }

    float wx = (ox    - g_dsMap.centerX) * g_dsMap.scale;
    float wy = (obj_z - g_dsMap.minZ)    * g_dsMap.scale;
    float wz = -(oy   - g_dsMap.centerY) * g_dsMap.scale;

    if (out_x) *out_x = wx;
    if (out_y) *out_y = wy;
    if (out_z) *out_z = wz;
    return wy;
}
