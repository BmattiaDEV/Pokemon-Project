#include "transition.h"

#define TRANS_FADE_FRAMES 12

typedef enum {
    TRANS_IDLE = 0,
    TRANS_FADE_OUT,
    TRANS_HOLD_BLACK,
    TRANS_FADE_IN
} TransitionPhase;

static bool                  s_ready = false;
static TileVertex*           s_vbo = NULL;
static TransitionPhase       s_phase = TRANS_IDLE;
static int                   s_frame = 0;
static TransitionMidCallback s_callback = NULL;
static void*                 s_userData = NULL;

void transitionInit(void) {
    if (s_ready) return;

    s_vbo = (TileVertex*)linearAlloc(6 * sizeof(TileVertex));
    if (s_vbo) {
        gfxSetQuad2D(s_vbo, 0.0f, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        GSPGPU_FlushDataCache(s_vbo, 6 * sizeof(TileVertex));
    }

    s_phase    = TRANS_IDLE;
    s_frame    = 0;
    s_callback = NULL;
    s_userData = NULL;
    s_ready    = (s_vbo != NULL);
}

void transitionExit(void) {
    if (s_vbo) {
        linearFree(s_vbo);
        s_vbo = NULL;
    }
    s_phase    = TRANS_IDLE;
    s_callback = NULL;
    s_userData = NULL;
    s_ready    = false;
}

void transitionStart(TransitionMidCallback on_black, void* user_data) {
    if (!s_ready || s_phase != TRANS_IDLE) return;

    s_phase    = TRANS_FADE_OUT;
    s_frame    = 0;
    s_callback = on_black;
    s_userData = user_data;
}

bool transitionIsActive(void) {
    return (s_phase != TRANS_IDLE);
}

void transitionUpdate(void) {
    if (s_phase == TRANS_IDLE) return;

    if (s_phase == TRANS_FADE_OUT) {
        s_frame++;
        if (s_frame >= TRANS_FADE_FRAMES) {
            s_phase = TRANS_HOLD_BLACK;
        }
    } else if (s_phase == TRANS_HOLD_BLACK) {
        // Esegue il caricamento della nuova mappa mentre lo schermo è già completamente nero
        if (s_callback) {
            s_callback(s_userData);
            s_callback = NULL;
            s_userData = NULL;
        }
        s_phase = TRANS_FADE_IN;
        s_frame = 0;
    } else if (s_phase == TRANS_FADE_IN) {
        s_frame++;
        if (s_frame >= TRANS_FADE_FRAMES) {
            s_phase = TRANS_IDLE;
            s_frame = 0;
        }
    }
}

void transitionDraw(int uLoc_projection, int uLoc_view) {
    if (!s_ready || s_phase == TRANS_IDLE || !s_vbo) return;

    u8 alpha = 255;
    if (s_phase == TRANS_FADE_OUT) {
        int a = (s_frame * 255) / TRANS_FADE_FRAMES;
        if (a < 0) a = 0;
        if (a > 255) a = 255;
        alpha = (u8)a;
    } else if (s_phase == TRANS_FADE_IN) {
        int a = ((TRANS_FADE_FRAMES - s_frame) * 255) / TRANS_FADE_FRAMES;
        if (a < 0) a = 0;
        if (a > 255) a = 255;
        alpha = (u8)a;
    }

    if (alpha == 0) return;

    C3D_Mtx orthoProj;
    Mtx_OrthoTilt(&orthoProj, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, true);

    C3D_Mtx orthoView;
    Mtx_Identity(&orthoView);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &orthoProj);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_view, &orthoView);

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    C3D_BufInfo* buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, s_vbo, sizeof(TileVertex), 3, 0x210);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_TexEnvColor(env, ((u32)alpha << 24));

    C3D_DrawArrays(GPU_TRIANGLES, 0, 6);

    // Ripristina TexEnv(0) e depth test standard
    env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
}
