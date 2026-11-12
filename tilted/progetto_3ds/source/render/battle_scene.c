#include "battle_scene.h"
#include "battle_bg_t3x.h"
#include "battle_base_enemy_t3x.h"
#include "battle_base_player_t3x.h"
#include "battle_opponents_t3x.h"
#include "battle_player_t3x.h"

// ============================================================================
// CONFIGURAZIONE SPRITE NEMICO (UCCELLINO)
// ============================================================================
// Dimensioni dello sprite originale nel file data/battle_opponents.png (128x256)
#define OPPONENT_RAW_W        95.0f
#define OPPONENT_RAW_H        135.0f
#define OPPONENT_TEX_W        128.0f
#define OPPONENT_TEX_H        256.0f

// Posizione sulla pedana avversaria
#define ENEMY_BASE_CENTER_X   285.0f  // Centro orizzontale della pedana d'erba
#define ENEMY_GROUND_Y        118.0f  // Punto d'appoggio dei piedi sulla pedana

// ============================================================================
// SCALA DELL'UCCELLINO: MODIFICA QUESTO VALORE PER INGRANDIRE O RIMPICCIOLIRE!
// ============================================================================
// Valori consigliati:
//   0.45f = molto piccolo (~43 x 61 px)
//   0.52f = piccolo / cucciolo (~49 x 70 px)  <-- ATTUALE (stile Starly in D/P)
//   0.60f = medio (~57 x 81 px)
//   0.70f = grande (~67 x 95 px)
//   1.00f = dimensione originale (95 x 135 px)
#define OPPONENT_SCALE        0.52f

static bool        s_ready = false;
static C3D_Tex     s_texBg;
static C3D_Tex     s_texBaseEnemy;
static C3D_Tex     s_texBasePlayer;
static C3D_Tex     s_texOpponents;
static C3D_Tex     s_texPlayer;
static TileVertex* s_vbo = NULL;

static void setQuad(TileVertex* v, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
    // Schermo 400x240 con proiezione ortografica (Mtx_OrthoTilt):
    // y0 = top (min y), y1 = bottom (max y)
    // Texture: v1 = top (1.0f), v0 = bottom (0.0f)
    v[0] = (TileVertex){ x0, y1, 0.5f, u0, v0, 0.0f, 0.0f };
    v[1] = (TileVertex){ x1, y1, 0.5f, u1, v0, 0.0f, 0.0f };
    v[2] = (TileVertex){ x1, y0, 0.5f, u1, v1, 0.0f, 0.0f };

    v[3] = (TileVertex){ x0, y1, 0.5f, u0, v0, 0.0f, 0.0f };
    v[4] = (TileVertex){ x1, y0, 0.5f, u1, v1, 0.0f, 0.0f };
    v[5] = (TileVertex){ x0, y0, 0.5f, u0, v1, 0.0f, 0.0f };
}

static void updateOpponentQuad(TileVertex* v) {
    float opp_w = OPPONENT_RAW_W * OPPONENT_SCALE;
    float opp_h = OPPONENT_RAW_H * OPPONENT_SCALE;
    float opp_x0 = ENEMY_BASE_CENTER_X - (opp_w * 0.5f);
    float opp_x1 = opp_x0 + opp_w;
    float opp_y1 = ENEMY_GROUND_Y;
    float opp_y0 = opp_y1 - opp_h;

    float opp_u0 = 0.0f;
    float opp_u1 = OPPONENT_RAW_W / OPPONENT_TEX_W;
    float opp_v0 = 1.0f - (OPPONENT_RAW_H / OPPONENT_TEX_H);
    float opp_v1 = 1.0f;

    setQuad(v, opp_x0, opp_y0, opp_x1, opp_y1, opp_u0, opp_v0, opp_u1, opp_v1);
}

static void getAtlasUV(int slot, float* u0, float* v0, float* u1, float* v1) {
    // Atlas 256x256 con 4 quadranti da 128x128:
    // slot 0: top-left     (u: 0.0..0.5, v: 0.5..1.0)
    // slot 1: top-right    (u: 0.5..1.0, v: 0.5..1.0)
    // slot 2: bottom-left  (u: 0.0..0.5, v: 0.0..0.5)
    // slot 3: bottom-right (u: 0.5..1.0, v: 0.0..0.5)
    switch (slot) {
        case 1:
            *u0 = 0.5f; *u1 = 1.0f;
            *v0 = 0.5f; *v1 = 1.0f;
            break;
        case 2:
            *u0 = 0.0f; *u1 = 0.5f;
            *v0 = 0.0f; *v1 = 0.5f;
            break;
        case 3:
            *u0 = 0.5f; *u1 = 1.0f;
            *v0 = 0.0f; *v1 = 0.5f;
            break;
        case 0:
        default:
            *u0 = 0.0f; *u1 = 0.5f;
            *v0 = 0.5f; *v1 = 1.0f;
            break;
    }
}

void battleSceneInit(void) {
    if (s_ready) return;

    // 1. Carica texture di sfondo
    Tex3DS_Texture t_bg = Tex3DS_TextureImport(battle_bg_t3x, battle_bg_t3x_size, &s_texBg, NULL, false);
    if (t_bg) Tex3DS_TextureFree(t_bg);
    C3D_TexSetFilter(&s_texBg, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&s_texBg, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // 2. Carica cerchio base avversario
    Tex3DS_Texture t_be = Tex3DS_TextureImport(battle_base_enemy_t3x, battle_base_enemy_t3x_size, &s_texBaseEnemy, NULL, false);
    if (t_be) Tex3DS_TextureFree(t_be);
    C3D_TexSetFilter(&s_texBaseEnemy, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&s_texBaseEnemy, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // 3. Carica cerchio base giocatore
    Tex3DS_Texture t_bp = Tex3DS_TextureImport(battle_base_player_t3x, battle_base_player_t3x_size, &s_texBasePlayer, NULL, false);
    if (t_bp) Tex3DS_TextureFree(t_bp);
    C3D_TexSetFilter(&s_texBasePlayer, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&s_texBasePlayer, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // 4. Carica texture Pokemon avversario
    Tex3DS_Texture t_opp = Tex3DS_TextureImport(battle_opponents_t3x, battle_opponents_t3x_size, &s_texOpponents, NULL, false);
    if (t_opp) Tex3DS_TextureFree(t_opp);
    C3D_TexSetFilter(&s_texOpponents, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&s_texOpponents, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // 5. Carica atlas Pokemon giocatore
    Tex3DS_Texture t_ply = Tex3DS_TextureImport(battle_player_t3x, battle_player_t3x_size, &s_texPlayer, NULL, false);
    if (t_ply) Tex3DS_TextureFree(t_ply);
    C3D_TexSetFilter(&s_texPlayer, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&s_texPlayer, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // 6. Alloca VBO lineare per i 5 quad (30 vertici)
    s_vbo = (TileVertex*)linearAlloc(30 * sizeof(TileVertex));
    if (s_vbo) {
        // Quad 0 (vertici 0..5): Sfondo schermo intero 400x240
        setQuad(&s_vbo[0], 0.0f, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Quad 1 (vertici 6..11): Cerchio base nemico (in alto a destra)
        setQuad(&s_vbo[6], 200.0f, 93.0f, 370.0f, 137.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Quad 2 (vertici 12..17): Pokemon selvatico (posizionato sulla base nemica con scala proporzionale)
        updateOpponentQuad(&s_vbo[12]);

        // Quad 3 (vertici 18..23): Cerchio base giocatore (in basso a sinistra, a filo con il bordo inferiore dello schermo)
        setQuad(&s_vbo[18], -30.0f, 196.0f, 250.0f, 254.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Quad 4 (vertici 24..29): Pokemon del giocatore (posizionato a filo inferiore sulla base)
        float pu0, pv0, pu1, pv1;
        getAtlasUV(0, &pu0, &pv0, &pu1, &pv1);
        setQuad(&s_vbo[24], 55.0f, 135.0f, 165.0f, 245.0f, pu0, pv0, pu1, pv1);

        GSPGPU_FlushDataCache(s_vbo, 30 * sizeof(TileVertex));
    }

    s_ready = true;
}

void battleSceneDraw(int uLoc_projection, int uLoc_view, int wild_mon_index, int player_mon_index) {
    if (!s_ready || !s_vbo) return;

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

    // 4. Aggiorna UV e quad in base ai Pokemon correnti
    updateOpponentQuad(&s_vbo[12]);

    float pu0, pv0, pu1, pv1;
    getAtlasUV(player_mon_index, &pu0, &pv0, &pu1, &pv1);
    setQuad(&s_vbo[24], 55.0f, 135.0f, 165.0f, 245.0f, pu0, pv0, pu1, pv1);

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

    // Disegna livello 2: Sprite Pokemon selvatico
    C3D_TexBind(0, &s_texOpponents);
    C3D_DrawArrays(GPU_TRIANGLES, 12, 6);

    // Disegna livello 3: Cerchio base giocatore
    C3D_TexBind(0, &s_texBasePlayer);
    C3D_DrawArrays(GPU_TRIANGLES, 18, 6);

    // Disegna livello 4: Sprite Pokemon giocatore (spalle)
    C3D_TexBind(0, &s_texPlayer);
    C3D_DrawArrays(GPU_TRIANGLES, 24, 6);

    // Ripristina depth test per il rendering successivo del mondo 3D
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
}

void battleSceneExit(void) {
    if (s_ready) {
        C3D_TexDelete(&s_texBg);
        C3D_TexDelete(&s_texBaseEnemy);
        C3D_TexDelete(&s_texBasePlayer);
        C3D_TexDelete(&s_texOpponents);
        C3D_TexDelete(&s_texPlayer);
        s_ready = false;
    }
    if (s_vbo) {
        linearFree(s_vbo);
        s_vbo = NULL;
    }
}
