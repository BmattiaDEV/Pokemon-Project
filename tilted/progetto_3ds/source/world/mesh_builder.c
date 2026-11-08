#include "mesh_builder.h"
#include "world.h"

#define TOTAL_SORT_ROWS (MAP_GEN_H + 6)

static TileVertex* vbo_data = NULL;
static u32 vbo_count = 0;
static u32 row_ground_end[TOTAL_SORT_ROWS];

static void emitQuad(float x_left, float x_right,
                     float y_tl, float y_tr, float y_br, float y_bl,
                     float z_top, float z_bot,
                     float u_left, float u_right, float v_top, float v_bottom,
                     float oy_top, float oy_bot) {
    if (y_tr == y_bl && y_tl != y_br) {
        // Diagonale da Top-Left (TL) a Bottom-Right (BR)
        vbo_data[vbo_count++] = (TileVertex){x_left,  y_bl, z_bot, u_left,  v_bottom, 0.0f, oy_bot};
        vbo_data[vbo_count++] = (TileVertex){x_right, y_br, z_bot, u_right, v_bottom, 0.0f, oy_bot};
        vbo_data[vbo_count++] = (TileVertex){x_left,  y_tl, z_top, u_left,  v_top,    0.0f, oy_top};

        vbo_data[vbo_count++] = (TileVertex){x_right, y_br, z_bot, u_right, v_bottom, 0.0f, oy_bot};
        vbo_data[vbo_count++] = (TileVertex){x_right, y_tr, z_top, u_right, v_top,    0.0f, oy_top};
        vbo_data[vbo_count++] = (TileVertex){x_left,  y_tl, z_top, u_left,  v_top,    0.0f, oy_top};
    } else {
        // Diagonale da Bottom-Left (BL) a Top-Right (TR)
        vbo_data[vbo_count++] = (TileVertex){x_left,  y_bl, z_bot, u_left,  v_bottom, 0.0f, oy_bot};
        vbo_data[vbo_count++] = (TileVertex){x_right, y_br, z_bot, u_right, v_bottom, 0.0f, oy_bot};
        vbo_data[vbo_count++] = (TileVertex){x_right, y_tr, z_top, u_right, v_top,    0.0f, oy_top};

        vbo_data[vbo_count++] = (TileVertex){x_left,  y_bl, z_bot, u_left,  v_bottom, 0.0f, oy_bot};
        vbo_data[vbo_count++] = (TileVertex){x_right, y_tr, z_top, u_right, v_top,    0.0f, oy_top};
        vbo_data[vbo_count++] = (TileVertex){x_left,  y_tl, z_top, u_left,  v_top,    0.0f, oy_top};
    }
}

void meshBuilderBuild(void) {
    int max_tiles = MAP_GEN_W * MAP_GEN_H * (MAP_GEN_NUM_LEVELS + 2);
    // Riserviamo +6 vertici in fondo al VBO per il quad dinamico del giocatore
    vbo_data = (TileVertex*)linearAlloc((max_tiles * 6 + 6) * sizeof(TileVertex));
    vbo_count = 0;
    if (!vbo_data) return;

    float half_w = TILE_W * 0.5f; // 8.0f
    float half_h = TILE_H * 0.5f; // 8.0f

    // Centriamo la mappa 16x16 attorno a (0, 0, 0)
    float map_half_w = (MAP_GEN_W * TILE_W) * 0.5f;
    float map_half_d = (MAP_GEN_H * TILE_H) * 0.5f;

    int base_ground_level = worldGetBaseGroundLevel();

    // Costruzione buffer ordinata per profondità Z (dalla riga più lontana alla più vicina alla telecamera)
    for (int row_z = 0; row_z < TOTAL_SORT_ROWS; ++row_z) {
        // 1. Terreno (erba/montagne) e overlay piatti al suolo (sentieri, piazzole, fiori) sulla riga `row_z`
        if (row_z < MAP_GEN_H) {
            int y = row_z;
            for (int level = MAP_GEN_MIN_LEVEL; level <= MAP_GEN_MAX_LEVEL; ++level) {
                for (int x = 0; x < MAP_GEN_W; ++x) {
                    int tile_id = worldGetEffectiveTile(level, x, y);
                    if (tile_id < 0 || tile_id >= TOTAL_TILES) {
                        continue;
                    }

                    float cx = ((float)x * TILE_W + half_w) - map_half_w;
                    float cz = ((float)y * TILE_H + half_h) - map_half_d;

                    float x_left  = cx - half_w;
                    float x_right = cx + half_w;
                    float z_top   = cz - half_h;
                    float z_bot   = cz + half_h;

                    // Se un overlay (es. fiori) è piazzato direttamente dentro una montagna sullo stesso layer `level`,
                    // disegniamo prima la base d'erba dell'altopiano a quota `level` sotto l'overlay
                    if (isOverlayTile(tile_id) && worldIsInsideMountainPlateau(level, x, y)) {
                        int gf = ((y % 2) * 8) + (x % 2);
                        int gcol = gf % TILESET_COLS;
                        int grow = gf / TILESET_COLS;
                        float gul = (float)gcol * TILE_W / TEX_W;
                        float gur = (float)(gcol + 1) * TILE_W / TEX_W;
                        float gvt = 1.0f - ((float)grow * TILE_H / TEX_H);
                        float gvb = 1.0f - ((float)(grow + 1) * TILE_H / TEX_H);
                        float gy_tl = worldGetCornerHeight(level, x,     y);
                        float gy_tr = worldGetCornerHeight(level, x + 1, y);
                        float gy_br = worldGetCornerHeight(level, x + 1, y + 1);
                        float gy_bl = worldGetCornerHeight(level, x,     y + 1);
                        emitQuad(x_left, x_right, gy_tl, gy_tr, gy_br, gy_bl, z_top, z_bot, gul, gur, gvt, gvb, 0.0f, 0.0f);
                    }

                    // Se un albero è stato piazzato direttamente sul piano base, disegniamo comunque l'erba sotto
                    if (isTreeTile(tile_id)) {
                        if (level == base_ground_level) {
                            tile_id = ((y % 2) * 8) + (x % 2);
                        } else {
                            continue;
                        }
                    }

                    int col = tile_id % TILESET_COLS;
                    int row = tile_id / TILESET_COLS;

                    float u_left   = (float)col * TILE_W / TEX_W;
                    float u_right  = (float)(col + 1) * TILE_W / TEX_W;
                    float v_top    = 1.0f - ((float)row * TILE_H / TEX_H);
                    float v_bottom = 1.0f - ((float)(row + 1) * TILE_H / TEX_H);

                    int height_level = isOverlayTile(tile_id) ? worldGetOverlayBaseLevel(level, x, y) : level;
                    float y_tl = worldGetCornerHeight(height_level, x,     y);
                    float y_tr = worldGetCornerHeight(height_level, x + 1, y);
                    float y_br = worldGetCornerHeight(height_level, x + 1, y + 1);
                    float y_bl = worldGetCornerHeight(height_level, x,     y + 1);

                    emitQuad(x_left, x_right, y_tl, y_tr, y_br, y_bl, z_top, z_bot, u_left, u_right, v_top, v_bottom, 0.0f, 0.0f);
                }
            }
        }

        // Salviamo il punto di split dopo il terreno della riga `row_z` e prima degli alberi con base in `row_z`
        row_ground_end[row_z] = vbo_count;

        // 2. Alberi inclinati in 3D verso la telecamera con base del tronco a `tree_base_y == row_z`
        for (int target_trunk_x = 0; target_trunk_x < MAP_GEN_W; ++target_trunk_x) {
            for (int level = MAP_GEN_MIN_LEVEL; level <= MAP_GEN_MAX_LEVEL; ++level) {
                for (int y = 0; y < MAP_GEN_H; ++y) {
                    for (int x = 0; x < MAP_GEN_W; ++x) {
                        int tile_id = worldGetRawTile(level, x, y);
                        if (!isTreeTile(tile_id)) {
                            continue;
                        }

                        int k = worldGetTreeRowFromBottom(level, x, y, tile_id);
                        int tree_base_y = y + k;
                        if (tree_base_y != row_z) {
                            continue;
                        }

                        int x_trunk = x + getTreeTrunkOffsetX(tile_id);
                        if (x_trunk < 0) x_trunk = 0;
                        if (x_trunk >= MAP_GEN_W) x_trunk = MAP_GEN_W - 1;
                        if (x_trunk != target_trunk_x) {
                            continue;
                        }

                        int height_level = worldGetOverlayBaseLevel(level, x_trunk, tree_base_y);
                        float base_y = (float)height_level * LEVEL_HEIGHT;

                        float k_bot = (float)k;
                        float k_top = (float)(k + 1);

                        float cx = ((float)x * TILE_W + half_w) - map_half_w;
                        float cz = ((float)y * TILE_H + half_h) - map_half_d;

                        float x_left  = cx - half_w;
                        float x_right = cx + half_w;

                        // Inclinazione 3D dell'albero verso la telecamera:
                        // la base del tronco (k=0) poggia esattamente a terra, mentre la chioma sale in +Y e avanza in +Z
                        float y_bl = base_y + k_bot * TREE_RISE_Y;
                        float y_br = base_y + k_bot * TREE_RISE_Y;
                        float y_tl = base_y + k_top * TREE_RISE_Y;
                        float y_tr = base_y + k_top * TREE_RISE_Y;

                        float z_bot = (cz + half_h) + k_bot * TREE_PULL_Z;
                        float z_top = (cz - half_h) + k_top * TREE_PULL_Z;

                        int col = tile_id % TILESET_COLS;
                        int row = tile_id / TILESET_COLS;

                        float u_left   = (float)col * TILE_W / TEX_W;
                        float u_right  = (float)(col + 1) * TILE_W / TEX_W;
                        float v_top    = 1.0f - ((float)row * TILE_H / TEX_H);
                        float v_bottom = 1.0f - ((float)(row + 1) * TILE_H / TEX_H);

                        emitQuad(x_left, x_right, y_tl, y_tr, y_br, y_bl, z_top, z_bot, u_left, u_right, v_top, v_bottom, 0.0f, 0.0f);
                    }
                }
            }
        }
    }

    memset(&vbo_data[vbo_count], 0, 6 * sizeof(TileVertex));
    GSPGPU_FlushDataCache(vbo_data, (vbo_count + 6) * sizeof(TileVertex));
}

void meshBuilderFree(void) {
    if (vbo_data) {
        linearFree(vbo_data);
        vbo_data = NULL;
    }
    vbo_count = 0;
}

TileVertex* meshBuilderGetVbo(void) {
    return vbo_data;
}

u32 meshBuilderGetVertexCount(void) {
    return vbo_count;
}

u32 meshBuilderGetRowGroundEnd(int row_z) {
    if (row_z < 0) return 0;
    if (row_z >= TOTAL_SORT_ROWS) return vbo_count;
    return row_ground_end[row_z];
}

void meshBuilderUpdatePlayerVertices(const TileVertex player_verts[6]) {
    if (!vbo_data) return;
    memcpy(&vbo_data[vbo_count], player_verts, 6 * sizeof(TileVertex));
    GSPGPU_FlushDataCache(&vbo_data[vbo_count], 6 * sizeof(TileVertex));
}
