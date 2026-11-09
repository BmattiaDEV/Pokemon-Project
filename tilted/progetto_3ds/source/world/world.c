#include "world.h"

typedef struct {
    int width;
    int height;
    int min_level;
    int max_level;
    int num_levels;
    int num_layers;
    const int* layer_heights;
    const int* const* layers;
} MapDescriptor;

// --- Mappa 1: Mappa 8x8 piana di sola erba con 1 livello d'altezza (altezza 0) ---
static const int mapPlain8x8_Altezza0[8][8] = {
    {0, 1, 0, 1, 0, 1, 0, 1},
    {8, 9, 8, 9, 8, 9, 8, 9},
    {0, 1, 0, 1, 0, 1, 0, 1},
    {8, 9, 8, 9, 8, 9, 8, 9},
    {0, 1, 0, 1, 0, 1, 0, 1},
    {8, 9, 8, 9, 8, 9, 8, 9},
    {0, 1, 0, 1, 0, 1, 0, 1},
    {8, 9, 8, 9, 8, 9, 8, 9}
};

static const int mapPlain8x8_Heights[1] = { 0 };
static const int* const mapPlain8x8_Layers[1] = { &mapPlain8x8_Altezza0[0][0] };

static const MapDescriptor g_maps[MAP_COUNT] = {
    [MAP_MAIN] = {
        .width         = MAP_GEN_W,
        .height        = MAP_GEN_H,
        .min_level     = MAP_GEN_MIN_LEVEL,
        .max_level     = MAP_GEN_MAX_LEVEL,
        .num_levels    = MAP_GEN_NUM_LEVELS,
        .num_layers    = MAP_GEN_NUM_LAYERS,
        .layer_heights = mapLayerHeights,
        .layers        = mapLayers
    },
    [MAP_PLAIN_8X8] = {
        .width         = 8,
        .height        = 8,
        .min_level     = 0,
        .max_level     = 0,
        .num_levels    = 1,
        .num_layers    = 1,
        .layer_heights = mapPlain8x8_Heights,
        .layers        = mapPlain8x8_Layers
    }
};

static MapId g_current_map_id = MAP_MAIN;
static const MapDescriptor* g_current_map = &g_maps[MAP_MAIN];

void worldSetCurrentMap(MapId map_id) {
    if (map_id < 0 || map_id >= MAP_COUNT) return;
    g_current_map_id = map_id;
    g_current_map    = &g_maps[map_id];
}

MapId worldGetCurrentMapId(void) {
    return g_current_map_id;
}

int worldGetMapWidth(void) {
    return g_current_map->width;
}

int worldGetMapHeight(void) {
    return g_current_map->height;
}

int worldGetMinLevel(void) {
    return g_current_map->min_level;
}

int worldGetMaxLevel(void) {
    return g_current_map->max_level;
}

int worldGetNumLevels(void) {
    return g_current_map->num_levels;
}

int worldGetNumLayers(void) {
    return g_current_map->num_layers;
}

int worldGetLayerHeight(int layer_idx) {
    if (layer_idx < 0 || layer_idx >= g_current_map->num_layers) return 0;
    return g_current_map->layer_heights[layer_idx];
}

int worldGetBaseGroundLevel(void) {
    int min_l = g_current_map->min_level;
    int max_l = g_current_map->max_level;
    return (0 >= min_l && 0 <= max_l) ? 0 : min_l;
}

int worldGetLayerRawTile(int layer_idx, int x, int y) {
    if (layer_idx < 0 || layer_idx >= g_current_map->num_layers) return -1;
    if (x < 0 || x >= g_current_map->width || y < 0 || y >= g_current_map->height) return -1;
    return g_current_map->layers[layer_idx][y * g_current_map->width + x];
}

// Restituisce true se `layer_idx` è il primo sub-layer associato alla propria altezza
static bool isPrimarySubLayerForHeight(int layer_idx) {
    if (layer_idx <= 0) return true;
    return (g_current_map->layer_heights[layer_idx] != g_current_map->layer_heights[layer_idx - 1]);
}

int worldGetRawTile(int level, int x, int y) {
    if (level < g_current_map->min_level || level > g_current_map->max_level) return -1;
    if (x < 0 || x >= g_current_map->width || y < 0 || y >= g_current_map->height) return -1;

    int first_overlay = -1;
    for (int i = 0; i < g_current_map->num_layers; ++i) {
        if (g_current_map->layer_heights[i] != level) continue;
        int t = g_current_map->layers[i][y * g_current_map->width + x];
        if (t != -1) {
            // Diamo priorità ai tile di terreno/montagna rispetto agli overlay (sentieri/alberi) sullo stesso livello
            if (!isOverlayTile(t)) {
                return t;
            }
            if (first_overlay == -1) {
                first_overlay = t;
            }
        }
    }
    return first_overlay;
}

bool worldIsInsideMountainPlateau(int level, int x, int y) {
    if (level <= worldGetBaseGroundLevel()) return false;
    int w = g_current_map->width;

    bool has_left_wall = false;
    for (int xx = x - 1; xx >= 0; --xx) {
        int t = worldGetRawTile(level, xx, y);
        if (t == 41 || t == 49 || t == 57 || t == 65) {
            has_left_wall = true;
            break;
        }
        if (t == 44 || t == 52 || t == 60 || t == 68 || t == -1) {
            break;
        }
    }
    if (!has_left_wall) return false;

    bool has_right_wall = false;
    for (int xx = x + 1; xx < w; ++xx) {
        int t = worldGetRawTile(level, xx, y);
        if (t == 44 || t == 52 || t == 60 || t == 68) {
            has_right_wall = true;
            break;
        }
        if (t == 41 || t == 49 || t == 57 || t == 65 || t == -1) {
            break;
        }
    }
    return has_right_wall;
}

int worldGetTreeRowFromBottom(int layer_idx, int x, int y, int tile_id) {
    if (tile_id == 21 || tile_id == 29 || tile_id == 22 || tile_id == 30) {
        int nx = x + getTreeTrunkOffsetX(tile_id);
        int level = worldGetLayerHeight(layer_idx);
        int neighbor = worldGetLayerRawTile(layer_idx, nx, y);

        // Se la punta fogliare è su un sub-layer separato, cerchiamo anche negli altri sub-layer della stessa altezza
        if (neighbor == -1) {
            for (int i = 0; i < g_current_map->num_layers; ++i) {
                if (g_current_map->layer_heights[i] == level) {
                    int cand = worldGetLayerRawTile(i, nx, y);
                    if (cand >= 120 && cand <= 199) {
                        neighbor = cand;
                        break;
                    }
                }
            }
        }

        if (neighbor >= 120 && neighbor <= 151) {
            return 18 - (neighbor / TILESET_COLS);
        }
        if (neighbor >= 152 && neighbor <= 199) {
            return 24 - (neighbor / TILESET_COLS);
        }
        return (tile_id == 21 || tile_id == 22) ? 2 : 1;
    }
    int row = tile_id / TILESET_COLS;
    if (row >= 15 && row <= 18) {
        return 18 - row; // 0..3
    }
    if (row >= 19 && row <= 24) {
        return 24 - row; // 0..5
    }
    return 0;
}

bool worldHasTileAbove(int level, int x, int y) {
    for (int i = 0; i < g_current_map->num_layers; ++i) {
        if (g_current_map->layer_heights[i] > level) {
            if (worldGetLayerRawTile(i, x, y) != -1) {
                return true;
            }
        }
    }
    return false;
}

bool worldHasTerrainAbove(int level, int x, int y) {
    for (int l = level + 1; l <= g_current_map->max_level; ++l) {
        int t = worldGetRawTile(l, x, y);
        if ((t != -1 && !isOverlayTile(t)) || worldIsInsideMountainPlateau(l, x, y)) {
            return true;
        }
    }
    return false;
}

int worldGetEffectiveLayerTile(int layer_idx, int x, int y) {
    int tid = worldGetLayerRawTile(layer_idx, x, y);
    if (tid != -1) return tid;

    // Solo il sub-layer principale di un'altezza riempie eventuali buchi con erba (evita doppi quad di erba)
    if (!isPrimarySubLayerForHeight(layer_idx)) {
        return -1;
    }

    int level = worldGetLayerHeight(layer_idx);
    int grass_fill = ((y % 2) * 8) + (x % 2); // 0, 1, 8, 9 (pattern 2x2 dell'erba chiara)

    // Sul piano base manteniamo il terreno continuo se un livello superiore disegna qualcosa sopra questa cella
    if (level == worldGetBaseGroundLevel() && worldHasTileAbove(level, x, y)) {
        return grass_fill;
    }

    // Se il livello immediatamente superiore ha un bordo roccioso di una montagna più alta,
    // garantiamo una base d'erba piatta su questo altopiano
    int above = worldGetRawTile(level + 1, x, y);
    if (above != -1 && isCliffBorderTile(above)) {
        return grass_fill;
    }

    return -1;
}

bool worldDoesCellRaiseCorner(int level, int cx, int cy, int corner_dx, int corner_dy) {
    if (cx < 0 || cx >= g_current_map->width || cy < 0 || cy >= g_current_map->height) return false;

    int tid = worldGetRawTile(level, cx, cy);
    if (tid == -1 || isOverlayTile(tid)) {
        if (worldIsInsideMountainPlateau(level, cx, cy)) {
            return true;
        }
        return worldHasTerrainAbove(level, cx, cy);
    }

    switch (tid) {
        case 41: return (corner_dx == 1 && corner_dy == 1); // Top-Left cliff: alza solo BR
        case 42:
        case 43: return (corner_dy == 1);                   // Top cliff: alza BL, BR
        case 44: return (corner_dx == 0 && corner_dy == 1); // Top-Right cliff: alza solo BL
        case 49:
        case 57: return (corner_dx == 1);                   // Left cliff: alza TR, BR
        case 52:
        case 60: return (corner_dx == 0);                   // Right cliff: alza TL, BL
        case 65: return (corner_dx == 1 && corner_dy == 0); // Bottom-Left cliff: alza solo TR
        case 66:
        case 67: return (corner_dy == 0);                   // Bottom cliff: alza TL, TR
        case 68: return (corner_dx == 0 && corner_dy == 0); // Bottom-Right cliff: alza solo TL
        default:
            return true;
    }
}

float worldGetCornerHeight(int level, int gx, int gy) {
    if (worldDoesCellRaiseCorner(level, gx - 1, gy - 1, 1, 1) ||
        worldDoesCellRaiseCorner(level, gx,     gy - 1, 0, 1) ||
        worldDoesCellRaiseCorner(level, gx - 1, gy,     1, 0) ||
        worldDoesCellRaiseCorner(level, gx,     gy,     0, 0)) {
        return (float)level * LEVEL_HEIGHT;
    }
    return (float)(level - 1) * LEVEL_HEIGHT;
}

int worldGetOverlayBaseLevel(int level, int x, int y) {
    int w = g_current_map->width;
    int h = g_current_map->height;
    if (x < 0) x = 0;
    if (x >= w) x = w - 1;
    if (y < 0) y = 0;
    if (y >= h) y = h - 1;

    if (worldIsInsideMountainPlateau(level, x, y)) {
        return level;
    }

    for (int l = level - 1; l >= g_current_map->min_level; --l) {
        int t = worldGetRawTile(l, x, y);
        if ((t != -1 && !isOverlayTile(t)) || worldIsInsideMountainPlateau(l, x, y)) {
            return l;
        }
    }
    return worldGetBaseGroundLevel();
}
