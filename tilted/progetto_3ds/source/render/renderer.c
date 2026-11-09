#include "renderer.h"
#include "common.h"
#include "mesh_builder.h"
#include "player.h"
#include "vshader_shbin.h"
#include "tileset_t3x.h"
#include "player_t3x.h"

static DVLB_s* vshader_dvlb = NULL;
static shaderProgram_s program;
static int uLoc_projection;
static int uLoc_view;
static C3D_Mtx projection;
static C3D_Mtx view;
static C3D_Tex tileset_tex;
static C3D_Tex player_tex;
static C3D_RenderTarget* topScreen = NULL;

void rendererReloadMap(void) {
    meshBuilderFree();
    meshBuilderBuild();

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, meshBuilderGetVbo(), sizeof(TileVertex), 3, 0x210);
}

void rendererInit(void) {
    // 0. Crea il RenderTarget per lo schermo superiore (240x400)
    topScreen = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(topScreen, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    // 1. Carica il Vertex Shader precompilato (.shbin)
    vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    C3D_BindProgram(&program);

    // 2. Uniforms
    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
    uLoc_view       = shaderInstanceGetUniformLocation(program.vertexShader, "view");

    // 3. Attributi dei vertici
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // position (x, y, z)
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // texcoord (u, v)
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2); // offset (ox, oy)

    // 4. Proiezione prospettica 3D (far plane esteso a 4000.0f)
    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(42.0f), C3D_AspectRatioTop, 5.0f, 4000.0f, false);

    // 5. Carica la Texture del Tileset (.t3x) in VRAM
    Tex3DS_Texture t3x_map = Tex3DS_TextureImport(tileset_t3x, tileset_t3x_size, &tileset_tex, NULL, false);
    if (t3x_map) {
        Tex3DS_TextureFree(t3x_map);
    }
    C3D_TexSetFilter(&tileset_tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&tileset_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // 6. Carica la Texture dello Spritesheet del Giocatore (.t3x) in VRAM
    Tex3DS_Texture t3x_player = Tex3DS_TextureImport(player_t3x, player_t3x_size, &player_tex, NULL, false);
    if (t3x_player) {
        Tex3DS_TextureFree(t3x_player);
    }
    C3D_TexSetFilter(&player_tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&player_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // 7. Configura TexEnv e Alpha Blending
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    // 8. Costruisci il buffer geometrico della mappa iniziale e registra il VBO
    rendererReloadMap();
}

void rendererDrawFrame(void) {
    // Aggiorna i 6 vertici del giocatore in fondo al VBO
    TileVertex player_verts[6];
    playerBuildQuad(player_verts);
    meshBuilderUpdatePlayerVertices(player_verts);

    // Aggiorna la telecamera per seguire fluidamente la posizione 3D del giocatore
    float px, py, pz;
    playerGetWorldPos(&px, &py, &pz);

    float rad   = CAM_ANGLE_DEG * (M_PI / 180.0f);
    float eye_y = CAM_DIST * sinf(rad);
    float eye_z = CAM_DIST * cosf(rad);

    Mtx_LookAt(&view,
        FVec3_New(px, py + eye_y, pz + eye_z),
        FVec3_New(px, py,         pz),
        FVec3_New(0.0f, 1.0f, 0.0f),
        false
    );

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_FrameDrawOn(topScreen);
    C3D_RenderTargetClear(topScreen, C3D_CLEAR_ALL, CLEAR_COLOR, 0);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_view, &view);

    u32 vbo_count = meshBuilderGetVertexCount();
    u32 split_idx = meshBuilderGetRowGroundEnd(playerGetSortRow());
    if (split_idx > vbo_count) {
        split_idx = vbo_count;
    }

    // 1. Disegna il terreno fino alla riga del giocatore e gli alberi dietro al giocatore
    if (split_idx > 0) {
        C3D_TexBind(0, &tileset_tex);
        C3D_DrawArrays(GPU_TRIANGLES, 0, split_idx);
    }

    // 2. Disegna il giocatore
    C3D_TexBind(0, &player_tex);
    C3D_DrawArrays(GPU_TRIANGLES, vbo_count, 6);

    // 3. Disegna gli alberi davanti al giocatore (chiome/tronchi con row_z >= player_sort_row) e le righe più vicine alla telecamera
    if (vbo_count > split_idx) {
        C3D_TexBind(0, &tileset_tex);
        C3D_DrawArrays(GPU_TRIANGLES, split_idx, vbo_count - split_idx);
    }

    C3D_FrameEnd(0);
}

void rendererExit(void) {
    meshBuilderFree();
    shaderProgramFree(&program);
    if (vshader_dvlb) {
        DVLB_Free(vshader_dvlb);
        vshader_dvlb = NULL;
    }
    C3D_TexDelete(&player_tex);
    C3D_TexDelete(&tileset_tex);
}
