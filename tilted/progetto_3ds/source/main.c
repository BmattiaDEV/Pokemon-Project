#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "map_data.h"
#include "vshader_shbin.h"
#include "tileset_t3x.h"

#define CLEAR_COLOR 0x1F2430FF
#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

// Limiti dei livelli d'altezza (-32 -> +32, fino a 64 dislivelli / 65 livelli)
#define MIN_HEIGHT_LEVEL (-32)
#define MAX_HEIGHT_LEVEL (32)
#define MAX_HEIGHT_LAYERS 65

#if (MAP_GEN_MIN_LEVEL < MIN_HEIGHT_LEVEL) || (MAP_GEN_MAX_LEVEL > MAX_HEIGHT_LEVEL)
#error "I livelli della mappa superano il range supportato [-32, 32]."
#endif

// Costanti del Tileset (outdoor.png: 128x256, 8x16 blocchi da 16x16)
#define TILESET_COLS 8
#define TILESET_ROWS 16
#define TOTAL_TILES  (TILESET_COLS * TILESET_ROWS)
#define TILE_W 16.0f
#define TILE_H 16.0f
#define TEX_W (TILESET_COLS * TILE_W) // 128.0f
#define TEX_H (TILESET_ROWS * TILE_H) // 256.0f

// Configurazione angolo telecamera e altezza per singolo livello
#define CAM_ANGLE_DEG 45.0f  // Angolo di inclinazione della telecamera (in gradi)
#define CAM_DIST      280.0f // Distanza della telecamera dal centro della mappa
#define LEVEL_HEIGHT  12.0f  // Dislivello per ogni piano (livello L => quota L * LEVEL_HEIGHT)

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

typedef struct {
    float x, y, z;    // Posizione 3D del vertice nel mondo
    float u, v;       // Coordinate UV della texture
    float ox, oy;     // Offset 2D (0, 0)
} TileVertex;

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static int uLoc_view;
static C3D_Mtx projection;
static C3D_Mtx view;
static C3D_Tex tileset_tex;
static C3D_RenderTarget* topScreen;

static TileVertex* vbo_data = NULL;
static u32 vbo_count = 0;

// Restituisce true se il tile è un elemento piatto che si appoggia al terreno
// (sassolini/fiori 16..27 o sentieri/incroci/piazzole 73..117)
static bool isOverlayTile(int tile_id) {
    return ((tile_id >= 16 && tile_id <= 27) ||
            (tile_id >= 73 && tile_id <= 117));
}

// Restituisce true se il tile è un bordo roccioso/parete della montagna (blocco 4x4: 41..68 eccetto il centro 50,51,58,59)
static bool isCliffBorderTile(int tile_id) {
    return (tile_id == 41 || tile_id == 42 || tile_id == 43 || tile_id == 44 ||
            tile_id == 49 || tile_id == 52 ||
            tile_id == 57 || tile_id == 60 ||
            tile_id == 65 || tile_id == 66 || tile_id == 67 || tile_id == 68);
}

// Legge in sicurezza il tile grezzo dal livello `level` (tra MAP_GEN_MIN_LEVEL e MAP_GEN_MAX_LEVEL)
static int getRawTile(int level, int x, int y) {
    if (level < MAP_GEN_MIN_LEVEL || level > MAP_GEN_MAX_LEVEL) return -1;
    if (x < 0 || x >= MAP_GEN_W || y < 0 || y >= MAP_GEN_H) return -1;
    return mapLevels[level - MAP_GEN_MIN_LEVEL][y][x];
}

// Controlla se esiste un tile valido a qualsiasi livello superiore a `level` nella cella (x, y)
static bool hasTileAbove(int level, int x, int y) {
    for (int l = level + 1; l <= MAP_GEN_MAX_LEVEL; ++l) {
        if (getRawTile(l, x, y) != -1) {
            return true;
        }
    }
    return false;
}

// Controlla se esiste un tile di terreno/montagna (non-overlay) sopra `level` nella cella (x, y)
static bool hasTerrainAbove(int level, int x, int y) {
    for (int l = level + 1; l <= MAP_GEN_MAX_LEVEL; ++l) {
        int t = getRawTile(l, x, y);
        if (t != -1 && !isOverlayTile(t)) {
            return true;
        }
    }
    return false;
}

// Restituisce il tile effettivo da disegnare al livello `level`, riempiendo con erba
// eventuali buchi (-1) sotto i bordi della montagna o sotto gli overlay del livello successivo
static int getEffectiveTile(int level, int x, int y) {
    int tid = getRawTile(level, x, y);
    if (tid != -1) return tid;

    int grass_fill = ((y % 2) * 8) + (x % 2); // 0, 1, 8, 9 (pattern 2x2 dell'erba chiara)

    // Sul piano base (0 o il livello minimo della mappa), manteniamo il terreno continuo
    // se un livello superiore disegna qualcosa sopra questa cella
    int base_ground_level = (0 >= MAP_GEN_MIN_LEVEL && 0 <= MAP_GEN_MAX_LEVEL) ? 0 : MAP_GEN_MIN_LEVEL;
    if (level == base_ground_level && hasTileAbove(level, x, y)) {
        return grass_fill;
    }

    // Se il livello immediatamente superiore ha un bordo roccioso o un overlay,
    // garantiamo una base d'erba piatta su questo livello
    int above = getRawTile(level + 1, x, y);
    if (above != -1 && (isCliffBorderTile(above) || isOverlayTile(above))) {
        return grass_fill;
    }

    return -1;
}

// Determina se la cella (cx, cy) sul livello `level` solleva il proprio angolo (corner_dx, corner_dy)
// dove corner_dx in {0=left, 1=right} e corner_dy in {0=top, 1=bottom}
static bool doesCellRaiseCorner(int level, int cx, int cy, int corner_dx, int corner_dy) {
    if (cx < 0 || cx >= MAP_GEN_W || cy < 0 || cy >= MAP_GEN_H) return false;

    int tid = getRawTile(level, cx, cy);
    if (tid == -1 || isOverlayTile(tid)) {
        // Se la cella è stata lasciata vuota (-1) perché sopra c'è un ulteriore livello di montagna,
        // fa comunque parte dell'altopiano interno di questo livello
        return hasTerrainAbove(level, cx, cy);
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
            // Qualsiasi altro tile di terreno (es. erba 0..11, centro montagna 50,51,58,59) è piatto alla quota del livello
            return true;
    }
}

// Calcola l'altezza Y del vertice di griglia (gx, gy) per un dato livello `level` (da -32 a +32)
static float getCornerHeight(int level, int gx, int gy) {
    if (doesCellRaiseCorner(level, gx - 1, gy - 1, 1, 1) ||
        doesCellRaiseCorner(level, gx,     gy - 1, 0, 1) ||
        doesCellRaiseCorner(level, gx - 1, gy,     1, 0) ||
        doesCellRaiseCorner(level, gx,     gy,     0, 0)) {
        return (float)level * LEVEL_HEIGHT;
    }
    return (float)(level - 1) * LEVEL_HEIGHT;
}

// Trova il livello del terreno su cui poggia un tile overlay (fiori/path) posto sul layer `level`
static int getOverlayBaseLevel(int level, int x, int y) {
    for (int l = level - 1; l >= MAP_GEN_MIN_LEVEL; --l) {
        int t = getEffectiveTile(l, x, y);
        if (t != -1 && !isOverlayTile(t)) {
            return l;
        }
    }
    return level;
}

static void buildMapBuffer(void) {
    int max_tiles = MAP_GEN_W * MAP_GEN_H * MAP_GEN_NUM_LEVELS;
    vbo_data = (TileVertex*)linearAlloc(max_tiles * 6 * sizeof(TileVertex));
    vbo_count = 0;
    if (!vbo_data) return;

    float half_w = TILE_W * 0.5f; // 8.0f
    float half_h = TILE_H * 0.5f; // 8.0f

    // Centriamo la mappa 16x16 attorno a (0, 0, 0)
    float map_half_w = (MAP_GEN_W * TILE_W) * 0.5f;
    float map_half_d = (MAP_GEN_H * TILE_H) * 0.5f;

    for (int level = MAP_GEN_MIN_LEVEL; level <= MAP_GEN_MAX_LEVEL; ++level) {
        for (int y = 0; y < MAP_GEN_H; ++y) {
            for (int x = 0; x < MAP_GEN_W; ++x) {
                int tile_id = getEffectiveTile(level, x, y);
                if (tile_id < 0 || tile_id >= TOTAL_TILES) {
                    continue;
                }

                // Coordinate X e Z del tile 16x16
                float cx = ((float)x * TILE_W + half_w) - map_half_w;
                float cz = ((float)y * TILE_H + half_h) - map_half_d;

                float x_left  = cx - half_w;
                float x_right = cx + half_w;
                float z_top   = cz - half_h;
                float z_bot   = cz + half_h;

                // Coordinate UV del tile 16x16
                int col = tile_id % TILESET_COLS;
                int row = tile_id / TILESET_COLS;

                float u_left   = (float)col * TILE_W / TEX_W;
                float u_right  = (float)(col + 1) * TILE_W / TEX_W;
                float v_top    = 1.0f - ((float)row * TILE_H / TEX_H);
                float v_bottom = 1.0f - ((float)(row + 1) * TILE_H / TEX_H);

                // Calcolo dell'altezza 3D (Y) dei 4 vertici per qualsiasi livello in [-32, +32]
                int height_level = isOverlayTile(tile_id) ? getOverlayBaseLevel(level, x, y) : level;
                float y_tl = getCornerHeight(height_level, x,     y);
                float y_tr = getCornerHeight(height_level, x + 1, y);
                float y_br = getCornerHeight(height_level, x + 1, y + 1);
                float y_bl = getCornerHeight(height_level, x,     y + 1);

                // Scegliamo la diagonale del quad in modo simmetrico sugli angoli della montagna
                // (funziona a qualsiasi quota da -32 a +32)
                if (y_tr == y_bl && y_tl != y_br) {
                    // Diagonale da Top-Left (TL) a Bottom-Right (BR)
                    vbo_data[vbo_count++] = (TileVertex){x_left,  y_bl, z_bot, u_left,  v_bottom, 0.0f, 0.0f};
                    vbo_data[vbo_count++] = (TileVertex){x_right, y_br, z_bot, u_right, v_bottom, 0.0f, 0.0f};
                    vbo_data[vbo_count++] = (TileVertex){x_left,  y_tl, z_top, u_left,  v_top,    0.0f, 0.0f};

                    vbo_data[vbo_count++] = (TileVertex){x_right, y_br, z_bot, u_right, v_bottom, 0.0f, 0.0f};
                    vbo_data[vbo_count++] = (TileVertex){x_right, y_tr, z_top, u_right, v_top,    0.0f, 0.0f};
                    vbo_data[vbo_count++] = (TileVertex){x_left,  y_tl, z_top, u_left,  v_top,    0.0f, 0.0f};
                } else {
                    // Diagonale da Bottom-Left (BL) a Top-Right (TR)
                    vbo_data[vbo_count++] = (TileVertex){x_left,  y_bl, z_bot, u_left,  v_bottom, 0.0f, 0.0f};
                    vbo_data[vbo_count++] = (TileVertex){x_right, y_br, z_bot, u_right, v_bottom, 0.0f, 0.0f};
                    vbo_data[vbo_count++] = (TileVertex){x_right, y_tr, z_top, u_right, v_top,    0.0f, 0.0f};

                    vbo_data[vbo_count++] = (TileVertex){x_left,  y_bl, z_bot, u_left,  v_bottom, 0.0f, 0.0f};
                    vbo_data[vbo_count++] = (TileVertex){x_right, y_tr, z_top, u_right, v_top,    0.0f, 0.0f};
                    vbo_data[vbo_count++] = (TileVertex){x_left,  y_tl, z_top, u_left,  v_top,    0.0f, 0.0f};
                }
            }
        }
    }

    GSPGPU_FlushDataCache(vbo_data, vbo_count * sizeof(TileVertex));
}

static void initScene(void) {
    // 1. Carica il Vertex Shader
    vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    C3D_BindProgram(&program);

    // 2. Uniform
    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
    uLoc_view       = shaderInstanceGetUniformLocation(program.vertexShader, "view");

    // 3. Attributi dei vertici
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // position (x, y, z)
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // texcoord (u, v)
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2); // offset (0, 0)

    // 4. Telecamera 3D Prospettica inclinata di CAM_ANGLE_DEG
    float rad   = CAM_ANGLE_DEG * (M_PI / 180.0f);
    float eye_y = CAM_DIST * sinf(rad);
    float eye_z = CAM_DIST * cosf(rad);

    // Far plane esteso a 4000.0f per supportare l'intero range di 64 dislivelli (-32 -> +32)
    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(42.0f), C3D_AspectRatioTop, 5.0f, 4000.0f, false);
    Mtx_LookAt(&view,
        FVec3_New(0.0f, eye_y, eye_z),
        FVec3_New(0.0f, 0.0f, 0.0f),
        FVec3_New(0.0f, 1.0f, 0.0f),
        false
    );

    // 5. Carica la Texture del Tileset (.t3x)
    Tex3DS_Texture t3x = Tex3DS_TextureImport(tileset_t3x, tileset_t3x_size, &tileset_tex, NULL, false);
    if (t3x) {
        Tex3DS_TextureFree(t3x);
    }
    C3D_TexSetFilter(&tileset_tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&tileset_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // 6. Configura TexEnv e Alpha Blending
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    // 7. Costruisci il buffer della mappa
    buildMapBuffer();

    // 8. Registra il VBO
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, vbo_data, sizeof(TileVertex), 3, 0x210);
}

static void renderScene(void) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_FrameDrawOn(topScreen);
    C3D_RenderTargetClear(topScreen, C3D_CLEAR_ALL, CLEAR_COLOR, 0);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_view, &view);

    C3D_TexBind(0, &tileset_tex);

    if (vbo_count > 0) {
        C3D_DrawArrays(GPU_TRIANGLES, 0, vbo_count);
    }

    C3D_FrameEnd(0);
}

int main(int argc, char** argv) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C3D_CullFace(GPU_CULL_NONE);

    topScreen = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(topScreen, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    initScene();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        renderScene();
    }

    if (vbo_data) linearFree(vbo_data);
    shaderProgramFree(&program);
    DVLB_Free(vshader_dvlb);
    C3D_TexDelete(&tileset_tex);
    C3D_Fini();
    gfxExit();
    return 0;
}
