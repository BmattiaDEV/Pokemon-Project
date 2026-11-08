#include "world.h"

int worldGetBaseGroundLevel(void) {
    return (0 >= MAP_GEN_MIN_LEVEL && 0 <= MAP_GEN_MAX_LEVEL) ? 0 : MAP_GEN_MIN_LEVEL;
}

int worldGetRawTile(int level, int x, int y) {
    if (level < MAP_GEN_MIN_LEVEL || level > MAP_GEN_MAX_LEVEL) return -1;
    if (x < 0 || x >= MAP_GEN_W || y < 0 || y >= MAP_GEN_H) return -1;
    return mapLevels[level - MAP_GEN_MIN_LEVEL][y][x];
}

bool worldIsInsideMountainPlateau(int level, int x, int y) {
    if (level <= worldGetBaseGroundLevel()) return false;

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
    for (int xx = x + 1; xx < MAP_GEN_W; ++xx) {
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

int worldGetTreeRowFromBottom(int level, int x, int y, int tile_id) {
    if (tile_id == 21 || tile_id == 29 || tile_id == 22 || tile_id == 30) {
        int nx = x + getTreeTrunkOffsetX(tile_id);
        int neighbor = worldGetRawTile(level, nx, y);
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
    for (int l = level + 1; l <= MAP_GEN_MAX_LEVEL; ++l) {
        if (worldGetRawTile(l, x, y) != -1) {
            return true;
        }
    }
    return false;
}

bool worldHasTerrainAbove(int level, int x, int y) {
    for (int l = level + 1; l <= MAP_GEN_MAX_LEVEL; ++l) {
        int t = worldGetRawTile(l, x, y);
        if ((t != -1 && !isOverlayTile(t)) || worldIsInsideMountainPlateau(l, x, y)) {
            return true;
        }
    }
    return false;
}

int worldGetEffectiveTile(int level, int x, int y) {
    int tid = worldGetRawTile(level, x, y);
    if (tid != -1) return tid;

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
    if (cx < 0 || cx >= MAP_GEN_W || cy < 0 || cy >= MAP_GEN_H) return false;

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
    if (x < 0) x = 0;
    if (x >= MAP_GEN_W) x = MAP_GEN_W - 1;
    if (y < 0) y = 0;
    if (y >= MAP_GEN_H) y = MAP_GEN_H - 1;

    if (worldIsInsideMountainPlateau(level, x, y)) {
        return level;
    }

    for (int l = level - 1; l >= MAP_GEN_MIN_LEVEL; --l) {
        int t = worldGetRawTile(l, x, y);
        if ((t != -1 && !isOverlayTile(t)) || worldIsInsideMountainPlateau(l, x, y)) {
            return l;
        }
    }
    return worldGetBaseGroundLevel();
}

