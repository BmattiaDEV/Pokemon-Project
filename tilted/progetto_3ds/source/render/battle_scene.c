#include "battle_scene.h"
#include "battle_bg_t3x.h"
#include "battle_base_enemy_t3x.h"
#include "battle_base_player_t3x.h"
#include "battle_opponents_t3x.h"
#include "battle_player_t3x.h"

// ============================================================================
// CONFIGURAZIONE SPRITESHEET E ANIMAZIONE POKEMON NEMICO (16 FRAME IN LOOP)
// ============================================================================
// Texture data/battle_opponents.png: 1024x1024, griglia 4x4 di celle da 256x256
// Ogni frame dell'animazione occupa un riquadro 160x160 nell'angolo in alto a sinistra della cella
#define OPPONENT_TEX_W        1024.0f
#define OPPONENT_TEX_H        1024.0f
#define OPPONENT_CELL_STRIDE  256.0f
#define OPPONENT_FRAME_W      160.0f
#define OPPONENT_FRAME_H      160.0f

// Punto di ancoraggio del corpo e delle zampe dentro il riquadro 160x160
#define OPPONENT_ANCHOR_X     81.5f   // 34 + (95 / 2): centro del corpo
#define OPPONENT_ANCHOR_Y     151.0f  // 16 + 135: pianta delle zampe

// Posizione sulla pedana avversaria
#define ENEMY_BASE_CENTER_X   285.0f  // Centro orizzontale della pedana d'erba
#define ENEMY_GROUND_Y        118.0f  // Punto d'appoggio dei piedi sulla pedana

// ============================================================================
// SCALA E VELOCITA' DELL'UCCELLINO: MODIFICA QUESTI VALORI PER PERSONALIZZARE!
// ============================================================================
// OPPONENT_SCALE:
//   0.45f = molto piccolo (~43 x 61 px)
//   0.52f = piccolo / cucciolo (~49 x 70 px)  <-- ATTUALE (stile Starly in D/P)
//   0.60f = medio (~57 x 81 px)
//   0.70f = grande (~67 x 95 px)
#define OPPONENT_SCALE        0.52f

// Animazione 16 sprite: ms per singolo frame (85ms = ~12 FPS, ciclo completo ~1.36s)
#define OPPONENT_ANIM_FRAMES  16
#define OPPONENT_FRAME_MS     85ULL

static bool        s_ready = false;
static C3D_Tex     s_texBg;
static C3D_Tex     s_texBaseEnemy;
static C3D_Tex     s_texBasePlayer;
static C3D_Tex     s_texOpponents;
static C3D_Tex     s_texPlayer;
static TileVertex* s_vbo = NULL;
static u64         s_animStartMs = 0;
static u64         s_lastDrawMs  = 0;

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

static void updateOpponentQuad(TileVertex* v, int frame_idx) {
    int idx = frame_idx % OPPONENT_ANIM_FRAMES;
    if (idx < 0) idx += OPPONENT_ANIM_FRAMES;
    int col = idx % 4;
    int row = idx / 4;

    float frame_w  = OPPONENT_FRAME_W  * OPPONENT_SCALE;
    float frame_h  = OPPONENT_FRAME_H  * OPPONENT_SCALE;
    float anchor_x = OPPONENT_ANCHOR_X * OPPONENT_SCALE;
    float anchor_y = OPPONENT_ANCHOR_Y * OPPONENT_SCALE;

    float opp_x0 = ENEMY_BASE_CENTER_X - anchor_x;
    float opp_x1 = opp_x0 + frame_w;
    float opp_y0 = ENEMY_GROUND_Y - anchor_y;
    float opp_y1 = opp_y0 + frame_h;

    float px0 = col * OPPONENT_CELL_STRIDE;
    float py0 = row * OPPONENT_CELL_STRIDE;
    float px1 = px0 + OPPONENT_FRAME_W;
    float py1 = py0 + OPPONENT_FRAME_H;

    float opp_u0 = px0 / OPPONENT_TEX_W;
    float opp_u1 = px1 / OPPONENT_TEX_W;
    float opp_v1 = 1.0f - (py0 / OPPONENT_TEX_H);
    float opp_v0 = 1.0f - (py1 / OPPONENT_TEX_H);

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

        // Quad 2 (vertici 12..17): Pokemon selvatico (frame 0 iniziale)
        updateOpponentQuad(&s_vbo[12], 0);

        // Quad 3 (vertici 18..23): Cerchio base giocatore (in basso a sinistra, a filo con il bordo inferiore dello schermo)
        setQuad(&s_vbo[18], -30.0f, 196.0f, 250.0f, 254.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        // Quad 4 (vertici 24..29): Pokemon del giocatore (posizionato a filo inferiore sulla base)
        float pu0, pv0, pu1, pv1;
        getAtlasUV(0, &pu0, &pv0, &pu1, &pv1);
        setQuad(&s_vbo[24], 55.0f, 135.0f, 165.0f, 245.0f, pu0, pv0, pu1, pv1);

        GSPGPU_FlushDataCache(s_vbo, 30 * sizeof(TileVertex));
    }

    s_animStartMs = 0;
    s_lastDrawMs  = 0;
    s_ready = true;
}

void battleSceneDraw(int uLoc_projection, int uLoc_view, int wild_mon_index, int player_mon_index) {
    (void)wild_mon_index;
    if (!s_ready || !s_vbo) return;

    // 0. Calcola il frame corrente dell'animazione a 16 sprite in loop continuo
    // Se si entra in un nuovo incontro (> 250ms dall'ultimo frame), riparte pulito dal frame 0
    u64 now = osGetTime();
    if (s_animStartMs == 0 || (now - s_lastDrawMs) > 250ULL) {
        s_animStartMs = now;
    }
    s_lastDrawMs = now;
    int opp_frame = (int)(((now - s_animStartMs) / OPPONENT_FRAME_MS) % OPPONENT_ANIM_FRAMES);

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

    // 4. Aggiorna UV e quad in base al frame corrente e al Pokemon del giocatore
    updateOpponentQuad(&s_vbo[12], opp_frame);

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
