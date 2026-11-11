#include "renderer.h"
#include "common.h"
#include "vshader_shbin.h"
#include "ds_map.h"

static DVLB_s*           vshader_dvlb = NULL;
static shaderProgram_s   program;
static int               uLoc_projection;
static int               uLoc_view;
static C3D_Mtx           projection;
static C3D_RenderTarget* topScreen = NULL;

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

    // 5. Configura TexEnv e Alpha Blending
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    // 6. Carica la mappa importata da DS Map Studio (texture e vertici)
    dsMapInit();
}

void rendererDrawFrame(void) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_FrameDrawOn(topScreen);
    C3D_RenderTargetClear(topScreen, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    dsMapDrawFrame(&projection, uLoc_projection, uLoc_view);
    C3D_FrameEnd(0);
}

void rendererExit(void) {
    dsMapExit();
    shaderProgramFree(&program);
    if (vshader_dvlb) {
        DVLB_Free(vshader_dvlb);
        vshader_dvlb = NULL;
    }
}
