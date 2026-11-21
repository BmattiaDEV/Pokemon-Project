#include "battle_scene.h"
#include "battle_bg_t3x.h"
#include "battle_base_enemy_t3x.h"
#include "battle_base_player_t3x.h"
#include "uccello_front_t3x.h"
#include "uccello_back_t3x.h"
#include "auremora_front_0_t3x.h"
#include "auremora_front_1_t3x.h"

// ============================================================================
// CONFIGURAZIONE SPRITESHEET E ANIMAZIONE (32 FRAME IN LOOP)
// ============================================================================
// data/uccello_front.png e data/uccello_back.png sono 512x1024 (griglia 4x8 da 128x128)
// data/auremora_front_0.png e auremora_front_1.png sono 1024x1024 (2 griglie 4x4 da 256x256, 1:1 a schermo)
#define BIRD_TEX_W           512.0f
#define BIRD_TEX_H           1024.0f
#define BIRD_CELL_STRIDE     128.0f
#define BIRD_FRAME_W         128.0f
#define BIRD_FRAME_H         128.0f
#define BIRD_ANIM_FRAMES     32
#define BIRD_FRAME_MS        75ULL

#define AUREMORA_TEX_W       1024.0f
#define AUREMORA_TEX_H       1024.0f
#define AUREMORA_CELL_STRIDE 256.0f
#define AUREMORA_FRAME_W     256.0f
#define AUREMORA_FRAME_H     256.0f

// Avversario (front, sulla pedana in alto a destra)
#define OPPONENT_ANCHOR_X      68.0f
#define OPPONENT_ANCHOR_Y      120.0f
#define AUREMORA_ANCHOR_X      128.0f
#define AUREMORA_ANCHOR_Y      216.0f
#define ENEMY_BASE_CENTER_X    285.0f
#define ENEMY_GROUND_Y         118.0f
#define AUREMORA_GROUND_Y      142.0f
#define OPPONENT_SCALE         1.0f
#define AUREMORA_SCALE         1.0f

// Giocatore (back, sulla pedana in basso a sinistra, a filo inferiore)
#define PLAYER_ANCHOR_X        60.5f
#define PLAYER_ANCHOR_Y        120.0f
#define PLAYER_BASE_CENTER_X   110.0f
#define PLAYER_GROUND_Y        239.0f
#define PLAYER_SCALE           1.4f

static bool        s_ready = false;
static C3D_Tex     s_texBg;
static C3D_Tex     s_texBaseEnemy;
static C3D_Tex     s_texBasePlayer;
static C3D_Tex     s_texOpponentFront;
static C3D_Tex     s_texAuremoraFront0;
static C3D_Tex     s_texAuremoraFront1;
static C3D_Tex     s_texPlayerBack;
static TileVertex* s_vbo = NULL;
static u64         s_animStartMs = 0;
static u64         s_lastDrawMs  = 0;

static void updateBirdSheetQuad(TileVertex* v, int frame_idx, float base_x, float ground_y, float anchor_x, float anchor_y, float scale) {
    int idx = frame_idx % BIRD_ANIM_FRAMES;
    if (idx < 0) idx += BIRD_ANIM_FRAMES;
    int col = idx % 4;
    int row = idx / 4;

    float frame_w = BIRD_FRAME_W * scale;
    float frame_h = BIRD_FRAME_H * scale;

    float x0 = base_x - (anchor_x * scale);
    float x1 = x0 + frame_w;
    float y0 = ground_y - (anchor_y * scale);
    float y1 = y0 + frame_h;

    float px0 = col * BIRD_CELL_STRIDE;
    float py0 = row * BIRD_CELL_STRIDE;
    float px1 = px0 + BIRD_FRAME_W;
    float py1 = py0 + BIRD_FRAME_H;

    float u0 = px0 / BIRD_TEX_W;
    float u1 = px1 / BIRD_TEX_W;
    float v1 = 1.0f - (py0 / BIRD_TEX_H);
    float v0 = 1.0f - (py1 / BIRD_TEX_H);

    gfxSetQuad2D(v, x0, y0, x1, y1, u0, v0, u1, v1);
}

static void updateAuremoraSheetQuad(TileVertex* v, int frame_idx, float base_x, float ground_y, float anchor_x, float anchor_y, float scale) {
    int idx = frame_idx % BIRD_ANIM_FRAMES;
    if (idx < 0) idx += BIRD_ANIM_FRAMES;
    int local_idx = idx % 16; // 0..15 dentro ciascuna texture 1024x1024 (4x4 celle da 256x256)
    int col = local_idx % 4;
    int row = local_idx / 4;

    float frame_w = AUREMORA_FRAME_W * scale;
    float frame_h = AUREMORA_FRAME_H * scale;

    float x0 = base_x - (anchor_x * scale);
    float x1 = x0 + frame_w;
    float y0 = ground_y - (anchor_y * scale);
    float y1 = y0 + frame_h;

    float px0 = col * AUREMORA_CELL_STRIDE;
    float py0 = row * AUREMORA_CELL_STRIDE;
    float px1 = px0 + AUREMORA_FRAME_W;
    float py1 = py0 + AUREMORA_FRAME_H;

    float u0 = px0 / AUREMORA_TEX_W;
    float u1 = px1 / AUREMORA_TEX_W;
    float v1 = 1.0f - (py0 / AUREMORA_TEX_H);
    float v0 = 1.0f - (py1 / AUREMORA_TEX_H);

    gfxSetQuad2D(v, x0, y0, x1, y1, u0, v0, u1, v1);
}

void battleSceneInit(void) {
    if (s_ready) return;

    texLoadFromMem(battle_bg_t3x, battle_bg_t3x_size, &s_texBg, GPU_LINEAR, GPU_CLAMP_TO_EDGE);
    texLoadFromMem(battle_base_enemy_t3x, battle_base_enemy_t3x_size, &s_texBaseEnemy, GPU_NEAREST, GPU_CLAMP_TO_EDGE);
    texLoadFromMem(battle_base_player_t3x, battle_base_player_t3x_size, &s_texBasePlayer, GPU_NEAREST, GPU_CLAMP_TO_EDGE);
    texLoadFromMem(uccello_front_t3x, uccello_front_t3x_size, &s_texOpponentFront, GPU_NEAREST, GPU_CLAMP_TO_EDGE);
    texLoadFromMem(auremora_front_0_t3x, auremora_front_0_t3x_size, &s_texAuremoraFront0, GPU_NEAREST, GPU_CLAMP_TO_EDGE);
    texLoadFromMem(auremora_front_1_t3x, auremora_front_1_t3x_size, &s_texAuremoraFront1, GPU_NEAREST, GPU_CLAMP_TO_EDGE);
    texLoadFromMem(uccello_back_t3x, uccello_back_t3x_size, &s_texPlayerBack, GPU_NEAREST, GPU_CLAMP_TO_EDGE);

    // Alloca VBO lineare per i 5 quad (30 vertici)
    s_vbo = (TileVertex*)linearAlloc(30 * sizeof(TileVertex));
    if (s_vbo) {
        // Quad 0 (vertici 0..5): Sfondo schermo intero 400x240
        gfxSetQuad2D(&s_vbo[0], 0.0f, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Quad 1 (vertici 6..11): Cerchio base nemico (in alto a destra)
        gfxSetQuad2D(&s_vbo[6], 200.0f, 93.0f, 370.0f, 137.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Quad 2 (vertici 12..17): Pokemon selvatico sempre uccello_front
        updateBirdSheetQuad(&s_vbo[12], 0, ENEMY_BASE_CENTER_X, ENEMY_GROUND_Y, OPPONENT_ANCHOR_X, OPPONENT_ANCHOR_Y, OPPONENT_SCALE);

        // Quad 3 (vertici 18..23): Cerchio base giocatore (in basso a sinistra, a filo inferiore)
        gfxSetQuad2D(&s_vbo[18], -30.0f, 196.0f, 250.0f, 254.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Quad 4 (vertici 24..29): Pokemon giocatore sempre uccello_back
        updateBirdSheetQuad(&s_vbo[24], 0, PLAYER_BASE_CENTER_X, PLAYER_GROUND_Y, PLAYER_ANCHOR_X, PLAYER_ANCHOR_Y, PLAYER_SCALE);

        GSPGPU_FlushDataCache(s_vbo, 30 * sizeof(TileVertex));
    }

    s_animStartMs = 0;
    s_lastDrawMs  = 0;
    s_ready = true;
}

void battleSceneDraw(int uLoc_projection, int uLoc_view, int wild_mon_index, int player_mon_index) {
    (void)player_mon_index;
    if (!s_ready || !s_vbo) return;

    // 0. Calcola i frame correnti delle animazioni a 32 sprite in loop continuo
    // Se si entra in un nuovo incontro (> 250ms dall'ultimo frame), riparte pulito dal frame 0
    u64 now = osGetTime();
    if (s_animStartMs == 0 || (now - s_lastDrawMs) > 250ULL) {
        s_animStartMs = now;
    }
    s_lastDrawMs = now;
    int front_frame = (int)(((now - s_animStartMs) / BIRD_FRAME_MS) % BIRD_ANIM_FRAMES);
    int back_frame  = (front_frame + (BIRD_ANIM_FRAMES / 2)) % BIRD_ANIM_FRAMES;

    // 1. Configura proiezione ortografica 2D (400x240) e matrice vista identità
    C3D_Mtx orthoProj;
    Mtx_OrthoTilt(&orthoProj, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, true);

    C3D_Mtx orthoView;
    Mtx_Identity(&orthoView);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &orthoProj);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_view, &orthoView);

    // 2. Disabilita depth test per il rendering 2D a livelli
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    // 3. Alpha blending e alpha test per le trasparenze
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    // 4. Aggiorna UV e quad per entrambi gli spritesheet
    if (wild_mon_index == 1) {
        updateAuremoraSheetQuad(&s_vbo[12], front_frame, ENEMY_BASE_CENTER_X, AUREMORA_GROUND_Y, AUREMORA_ANCHOR_X, AUREMORA_ANCHOR_Y, AUREMORA_SCALE);
    } else {
        updateBirdSheetQuad(&s_vbo[12], front_frame, ENEMY_BASE_CENTER_X, ENEMY_GROUND_Y, OPPONENT_ANCHOR_X, OPPONENT_ANCHOR_Y, OPPONENT_SCALE);
    }
    updateBirdSheetQuad(&s_vbo[24], back_frame, PLAYER_BASE_CENTER_X, PLAYER_GROUND_Y, PLAYER_ANCHOR_X, PLAYER_ANCHOR_Y, PLAYER_SCALE);

    GSPGPU_FlushDataCache(&s_vbo[12], 18 * sizeof(TileVertex));

    C3D_BufInfo* buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, s_vbo, sizeof(TileVertex), 3, 0x210);

    // Disegna livello 0: Sfondo (area prato / percorso)
    C3D_TexBind(0, &s_texBg);
    C3D_DrawArrays(GPU_TRIANGLES, 0, 6);

    // Disegna livello 1: Cerchio base nemico
    C3D_TexBind(0, &s_texBaseEnemy);
    C3D_DrawArrays(GPU_TRIANGLES, 6, 6);

    // Disegna livello 2: Pokemon avversario (50% uccello_front, 50% auremora_front 0/1)
    if (wild_mon_index == 1) {
        C3D_TexBind(0, (front_frame < 16) ? &s_texAuremoraFront0 : &s_texAuremoraFront1);
    } else {
        C3D_TexBind(0, &s_texOpponentFront);
    }
    C3D_DrawArrays(GPU_TRIANGLES, 12, 6);

    // Disegna livello 3: Cerchio base giocatore
    C3D_TexBind(0, &s_texBasePlayer);
    C3D_DrawArrays(GPU_TRIANGLES, 18, 6);

    // Disegna livello 4: Uccello giocatore (uccello_back)
    C3D_TexBind(0, &s_texPlayerBack);
    C3D_DrawArrays(GPU_TRIANGLES, 24, 6);

    // Ripristina depth test per il rendering successivo del mondo 3D
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
}

void battleSceneExit(void) {
    if (s_ready) {
        C3D_TexDelete(&s_texBg);
        C3D_TexDelete(&s_texBaseEnemy);
        C3D_TexDelete(&s_texBasePlayer);
        C3D_TexDelete(&s_texOpponentFront);
        C3D_TexDelete(&s_texAuremoraFront0);
        C3D_TexDelete(&s_texAuremoraFront1);
        C3D_TexDelete(&s_texPlayerBack);
        s_ready = false;
    }
    if (s_vbo) {
        linearFree(s_vbo);
        s_vbo = NULL;
    }
}
