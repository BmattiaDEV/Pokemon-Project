#include "ds_map_internal.h"
#include "rhana_anim0_t3x.h"
#include "rhana_anim1_t3x.h"
#include "rhana_anim2_t3x.h"
#include "rhana_anim3_t3x.h"
#include "rhana_anim4_t3x.h"
#include "flower_overlay_anim0_t3x.h"
#include "flower_overlay_anim1_t3x.h"
#include "flower_overlay_anim2_t3x.h"
#include "flower_overlay_anim3_t3x.h"
#include "flower_overlay_anim4_t3x.h"

// Parametri onde 3D e scorrimento UV dell'acqua (sea_lm1, gruppo 11)
#define WATER_WAVE_SPEED      2.0f
#define WATER_WAVE_HEIGHT     2.0f
#define WATER_FLOW_SPEED_U    1.2f
#define WATER_FLOW_SPEED_V    0.8f
#define WATER_RIPPLE_AMP      0.15f

void dsMapAnimInit(void) {
    // 1. Carica i 5 frame animati dei fiori (terreno rhana + overlay in primo piano)
    const void* rhana_tex_data[FLOWER_ANIM_FRAMES] = {
        rhana_anim0_t3x, rhana_anim1_t3x, rhana_anim2_t3x, rhana_anim3_t3x, rhana_anim4_t3x
    };
    const u32 rhana_tex_size[FLOWER_ANIM_FRAMES] = {
        (u32)rhana_anim0_t3x_size, (u32)rhana_anim1_t3x_size, (u32)rhana_anim2_t3x_size,
        (u32)rhana_anim3_t3x_size, (u32)rhana_anim4_t3x_size
    };
    const void* flower_ov_data[FLOWER_ANIM_FRAMES] = {
        flower_overlay_anim0_t3x, flower_overlay_anim1_t3x, flower_overlay_anim2_t3x,
        flower_overlay_anim3_t3x, flower_overlay_anim4_t3x
    };
    const u32 flower_ov_size[FLOWER_ANIM_FRAMES] = {
        (u32)flower_overlay_anim0_t3x_size, (u32)flower_overlay_anim1_t3x_size, (u32)flower_overlay_anim2_t3x_size,
        (u32)flower_overlay_anim3_t3x_size, (u32)flower_overlay_anim4_t3x_size
    };

    g_dsMap.flowerAnimReady = true;
    for (int i = 0; i < FLOWER_ANIM_FRAMES; ++i) {
        if (!texLoadFromMem(rhana_tex_data[i], rhana_tex_size[i], &g_dsMap.rhanaTex[i], GPU_NEAREST, GPU_REPEAT)) {
            g_dsMap.flowerAnimReady = false;
        }
        if (!texLoadFromMem(flower_ov_data[i], flower_ov_size[i], &g_dsMap.flowerOverlayTex[i], GPU_NEAREST, GPU_CLAMP_TO_EDGE)) {
            g_dsMap.flowerAnimReady = false;
        }
    }
    g_dsMap.flowerAnimTicks = 0;

    // 2. Salva la geometria base dell'acqua per l'animazione 3D delle onde e lo scorrimento UV
    g_dsMap.waterBase        = NULL;
    g_dsMap.waterVertexCount = 0;
    g_dsMap.waterFirst       = 0;
    g_dsMap.waterAnimTime    = 0.0f;

    if (DS_MAP_WATER_GROUP < DS_MAP_GROUP_COUNT && g_dsMap.vbo) {
        g_dsMap.waterVertexCount = g_dsMap.groups[DS_MAP_WATER_GROUP].vertexCount;
        g_dsMap.waterFirst       = g_dsMap.first[DS_MAP_WATER_GROUP];
        if (g_dsMap.waterVertexCount > 0) {
            g_dsMap.waterBase = (WaterBaseVertex*)malloc(g_dsMap.waterVertexCount * sizeof(WaterBaseVertex));
            if (g_dsMap.waterBase) {
                for (u32 i = 0; i < g_dsMap.waterVertexCount; ++i) {
                    const TileVertex* d = &g_dsMap.vbo[g_dsMap.waterFirst + i];
                    g_dsMap.waterBase[i].y = d->y;
                    g_dsMap.waterBase[i].u = d->u;
                    g_dsMap.waterBase[i].v = d->v;
                }
            }
        }
    }
}

void dsMapAnimExit(void) {
    if (g_dsMap.flowerAnimReady) {
        for (int i = 0; i < FLOWER_ANIM_FRAMES; ++i) {
            C3D_TexDelete(&g_dsMap.rhanaTex[i]);
            C3D_TexDelete(&g_dsMap.flowerOverlayTex[i]);
        }
        g_dsMap.flowerAnimReady = false;
    }

    if (g_dsMap.waterBase) {
        free(g_dsMap.waterBase);
        g_dsMap.waterBase = NULL;
    }
    g_dsMap.waterVertexCount = 0;
    g_dsMap.waterFirst       = 0;
}

void dsMapAnimUpdate(void) {
    g_dsMap.waterAnimTime += 1.0f / 60.0f;
    g_dsMap.flowerAnimTicks++;
}

void dsMapAnimUpdateWaterVbo(void) {
    if (!g_dsMap.waterBase || g_dsMap.waterVertexCount == 0 || !g_dsMap.vbo) return;

    float t      = g_dsMap.waterAnimTime * WATER_WAVE_SPEED;
    float uShift = g_dsMap.waterAnimTime * WATER_FLOW_SPEED_U;
    float vShift = g_dsMap.waterAnimTime * WATER_FLOW_SPEED_V;

    for (u32 i = 0; i < g_dsMap.waterVertexCount; ++i) {
        TileVertex* d = &g_dsMap.vbo[g_dsMap.waterFirst + i];
        float base_y = g_dsMap.waterBase[i].y;
        float base_u = g_dsMap.waterBase[i].u;
        float base_v = g_dsMap.waterBase[i].v;

        float wave1 = sinf(t + d->x * 0.015f - d->z * 0.012f) * (WATER_WAVE_HEIGHT * 0.7f);
        float wave2 = cosf(t * 1.35f + d->x * 0.010f - d->z * 0.018f) * (WATER_WAVE_HEIGHT * 0.3f);
        d->y = base_y + wave1 + wave2;

        float uRipple = sinf(t * 1.5f + d->x * 0.02f) * WATER_RIPPLE_AMP;
        float vRipple = cosf(t * 1.2f - d->z * 0.02f) * WATER_RIPPLE_AMP;
        d->u = base_u + uShift + uRipple;
        d->v = base_v + vShift + vRipple;
    }

    GSPGPU_FlushDataCache(&g_dsMap.vbo[g_dsMap.waterFirst], g_dsMap.waterVertexCount * sizeof(TileVertex));
}

int dsMapAnimGetFlowerFrame(void) {
    // Oscillazione lenta destra/sinistra a 8 passi: 0 -> +1 -> +2 -> +1 -> 0 -> -1 -> -2 -> -1
    static const int s_flowerStepToFrame[8] = { 0, 1, 2, 1, 0, 3, 4, 3 };
    int step = (int)((g_dsMap.flowerAnimTicks / 14) % 8);
    return s_flowerStepToFrame[step];
}
