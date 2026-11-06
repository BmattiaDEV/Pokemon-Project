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

// Costanti del Tileset (outdoor.png: 128x256, 4x8 blocchi da 32x32)
#define TILESET_COLS 4
#define TILESET_ROWS 8
#define TILE_W 32.0f
#define TILE_H 32.0f
#define TEX_W (TILESET_COLS * TILE_W) // 128.0f
#define TEX_H (TILESET_ROWS * TILE_H) // 256.0f

// Configurazione angolo telecamera e altezza montagna
#define CAM_ANGLE_DEG 45.0f  // Angolo di inclinazione della telecamera (in gradi)
#define CAM_DIST      280.0f // Distanza della telecamera dal centro della mappa
#define LEVEL_HEIGHT  16.0f  // Altezza della montagna (puoi metterla anche a 50.0f)

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

// Restituisce true se il tile è un elemento piatto che si appoggia allo slot (fiori 4..5 o path 20..30)
static bool isOverlayTile(int tile_id) {
    return (tile_id == 4 || tile_id == 5 || (tile_id >= 20 && tile_id <= 30));
}

// Controlla se una cella è circondata o adiacente a tile del path (20..30)
static bool isAdjacentToPath(int cx, int cy) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx >= 0 && nx < MAP_GEN_W && ny >= 0 && ny < MAP_GEN_H) {
                int tid = mapAltezza1[ny][nx];
                if (tid >= 20 && tid <= 30) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Restituisce true solo se la cella (cx, cy) è la cima rialzata della montagna
static bool isCellRaised(int cx, int cy) {
    if (cx < 0 || cx >= MAP_GEN_W || cy < 0 || cy >= MAP_GEN_H) return false;
    int t1 = mapAltezza1[cy][cx];
    if (t1 == -1 || isOverlayTile(t1) || isAdjacentToPath(cx, cy)) {
        return false;
    }
    return (mapAltezza0[cy][cx] == -1);
}

// Calcola l'altezza Y di un vertice (gx, gy) SOLO per il livello 1
static float getCornerHeight(int gx, int gy) {
    if (isCellRaised(gx - 1, gy - 1) ||
        isCellRaised(gx,     gy - 1) ||
        isCellRaised(gx - 1, gy)     ||
        isCellRaised(gx,     gy)) {
        return LEVEL_HEIGHT;
    }
    return 0.0f;
}

static void buildMapBuffer(void) {
    int max_tiles = MAP_GEN_W * MAP_GEN_H * (MAP_GEN_MAX_LEVEL + 1);
    vbo_data = (TileVertex*)linearAlloc(max_tiles * 6 * sizeof(TileVertex));
    vbo_count = 0;

    float half_w = TILE_W * 0.5f; // 16.0f
    float half_h = TILE_H * 0.5f; // 16.0f

    // Centriamo la mappa 8x8 attorno a (0, 0, 0)
    float map_half_w = (MAP_GEN_W * TILE_W) * 0.5f;
    float map_half_d = (MAP_GEN_H * TILE_H) * 0.5f;

    for (int level = 0; level <= MAP_GEN_MAX_LEVEL; ++level) {
        const int (*current_map)[MAP_GEN_W] = NULL;
        switch (level) {
            case 0: current_map = mapAltezza0; break;
            case 1: current_map = mapAltezza1; break;
            default: continue;
        }

        for (int y = 0; y < MAP_GEN_H; ++y) {
            for (int x = 0; x < MAP_GEN_W; ++x) {
                int tile_id = current_map[y][x];

                // Sul livello 0 riempiamo eventuali celle -1 sotto la montagna o il path
                // con l'erba piatta (0), così il terreno sotto rimane continuo e piatto
                if (tile_id == -1) {
                    if (level == 0 && mapAltezza1[y][x] != -1) {
                        tile_id = 0;
                    } else {
                        continue;
                    }
                }

                // Coordinate X e Z di base del tile 32x32
                float cx = ((float)x * TILE_W + half_w) - map_half_w;
                float cz = ((float)y * TILE_H + half_h) - map_half_d;

                float x_left  = cx - half_w;
                float x_right = cx + half_w;
                float z_top   = cz - half_h;
                float z_bot   = cz + half_h;

                // Coordinate UV di base del tile 32x32
                int col = tile_id % TILESET_COLS;
                int row = tile_id / TILESET_COLS;

                float u_left   = (float)col * TILE_W / TEX_W;
                float u_right  = (float)(col + 1) * TILE_W / TEX_W;
                float v_top    = 1.0f - ((float)row * TILE_H / TEX_H);
                float v_bottom = 1.0f - ((float)(row + 1) * TILE_H / TEX_H);

                // Altezza 3D (Y) dei 4 vertici:
                // - Il livello 0 (il terreno sotto) resta SEMPRE piatto a Y = 0.0f!
                // - Solo il livello 1 si alza dove c'è la montagna.
                float y_tl = 0.0f, y_tr = 0.0f, y_br = 0.0f, y_bl = 0.0f;
                if (level == 1) {
                    y_tl = getCornerHeight(x,     y);
                    y_tr = getCornerHeight(x + 1, y);
                    y_br = getCornerHeight(x + 1, y + 1);
                    y_bl = getCornerHeight(x,     y + 1);

                    // Nei tile dei bordi della montagna (8..18), la roccia in outdoor.png
                    // occupa solo la metà interna (16px) del tile, mentre i 16px esterni
                    // sono trasparenti. Ritagliamo la metà trasparente esterna così la
                    // base della roccia tocca direttamente quota Y = 0.0f sul terreno piatto!
                    float u_mid = (u_left + u_right) * 0.5f;
                    float v_mid = (v_top + v_bottom) * 0.5f;

                    if (tile_id == 8 || tile_id == 12 || tile_id == 16) {
                        x_left = cx;
                        u_left = u_mid;
                    }
                    if (tile_id == 10 || tile_id == 14 || tile_id == 18) {
                        x_right = cx;
                        u_right = u_mid;
                    }
                    if (tile_id == 8 || tile_id == 9 || tile_id == 10) {
                        z_top = cz;
                        v_top = v_mid;
                    }
                    if (tile_id == 16 || tile_id == 17 || tile_id == 18) {
                        z_bot = cz;
                        v_bottom = v_mid;
                    }
                }

                // Scegliamo la diagonale del quad in modo simmetrico sugli angoli della montagna
                if ((y_tl > 0.0f && y_br == 0.0f && y_tr == 0.0f && y_bl == 0.0f) ||
                    (y_br > 0.0f && y_tl == 0.0f && y_tr == 0.0f && y_bl == 0.0f)) {
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

    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(42.0f), C3D_AspectRatioTop, 10.0f, 1000.0f, false);
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