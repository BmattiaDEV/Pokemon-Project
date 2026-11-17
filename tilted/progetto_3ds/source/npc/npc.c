#include "npc.h"
#include "ds_map.h"
#include "npcs_t3x.h"

// Atlas data/npcs.png: 512x128, griglia 16x4 di sprite 32x32 (usate colonne 0..11)
// Per ogni direzione dir (0=DOWN, 1=UP, 2=LEFT, 3=RIGHT) ci sono 3 colonne consecutive:
//   col = dir * 3 + 0 -> idle
//   col = dir * 3 + 1 -> step 1
//   col = dir * 3 + 2 -> step 2
// Righe: sprite_id (0=Esploratore Blu, 1=Ranger Verde, 2=Avventuriero Oro, 3=Saggio Viola)
#define NPC_TEX_W       512.0f
#define NPC_TEX_H       128.0f
#define NPC_ATLAS_COLS  16
#define NPC_ATLAS_ROWS  4
#define NPC_MOVE_SPEED  (1.0f / 24.0f)

typedef struct {
    int             start_x;
    int             start_y;
    PlayerDirection dir;
    int             sprite_id;
    NpcMoveMode     move_mode;
    int             wander_radius; // 1 = max 3x3 centrato sul punto iniziale
} NpcSpawnDef;

// Tabella iniziale degli NPC sulla mappa (wander_radius = 1 limita il movimento a un 3x3 attorno allo spawn)
static const NpcSpawnDef s_initialNpcs[] = {
    { 16, 14, DIR_RIGHT, 0, NPC_MOVE_WANDER, 1 },
    { 20, 15, DIR_DOWN,  1, NPC_MOVE_WANDER, 1 },
};

static Npc         s_npcs[MAX_NPCS];
static int         s_npcCount = 0;
static C3D_Tex     s_npcTex;
static bool        s_npcTexReady = false;
static TileVertex* s_npcVbo = NULL;
static bool        s_vboDirty = true;

static int getNpcAnimStep(const Npc* npc) {
    if (!npc->moving) {
        return 0;
    }

    int first_step  = (npc->step_parity == 0) ? 1 : 2;
    int second_step = (npc->step_parity == 0) ? 2 : 1;

    if (npc->move_progress < 0.45f) {
        return first_step;
    } else if (npc->move_progress < 0.55f) {
        return 0;
    } else if (npc->move_progress < 0.95f) {
        return second_step;
    }
    return 0;
}

static void buildNpcVbo(float cam_x, float cam_y, float cam_z, float cam_dist) {
    if (!s_npcVbo || s_npcCount <= 0) return;

    float rad   = CAM_ANGLE_DEG * (M_PI / 180.0f);
    float sin_a = sinf(rad);
    float cos_a = cosf(rad);

    float z_bot_loc = 0.5f * sin_a + 6.0f * cos_a;
    float z_top_loc = (0.5f + PLAYER_RISE_Y) * sin_a + (6.0f - PLAYER_SLANT_Z) * cos_a;

    for (int i = 0; i < s_npcCount; ++i) {
        const Npc* npc = &s_npcs[i];
        int dir = (int)npc->dir;
        if (dir < 0 || dir > 3) dir = 0;

        int col = dir * 3 + getNpcAnimStep(npc);
        if (col < 0 || col >= NPC_ATLAS_COLS) col = 0;

        int row = npc->sprite_id;
        if (row < 0 || row >= NPC_ATLAS_ROWS) row = 0;

        float wx, wy, wz;
        dsMapGetWorldPos(npc->pos_x, npc->pos_y, &wx, &wy, &wz);

        float u_left   = ((float)col * PLAYER_SPRITE_W) / NPC_TEX_W;
        float u_right  = ((float)(col + 1) * PLAYER_SPRITE_W) / NPC_TEX_W;
        float v_top    = 1.0f - (((float)row * PLAYER_SPRITE_H) / NPC_TEX_H);
        float v_bottom = 1.0f - (((float)(row + 1) * PLAYER_SPRITE_H) / NPC_TEX_H);

        TileVertex* v = &s_npcVbo[i * 6];
        playerBuildBillboardQuad(v, wx, wy, wz, u_left, v_bottom, u_right, v_top);

        // Compensa la deformazione prospettica laterale/verticale fuori centro (inOff.xy)
        // così ogni NPC mantiene esattamente lo stesso angolo e proporzioni del personaggio del giocatore
        // senza ruotare verso la telecamera e senza alterare lo Z-buffer 3D.
        float dx = wx - cam_x;
        float dy = wy - cam_y;
        float dz = wz - cam_z;

        float x_anc = dx;
        float y_anc = dy * cos_a - dz * sin_a;
        float z_anc = dy * sin_a + dz * cos_a;

        float w_bot = cam_dist - (z_anc + z_bot_loc);
        float w_top = cam_dist - (z_anc + z_top_loc);

        if (w_bot > 1.0f) {
            float shear_scale = (w_top - w_bot) / w_bot;
            float ox_top = x_anc * shear_scale;
            float oy_top = y_anc * shear_scale;

            v[2].ox = ox_top; v[2].oy = oy_top;
            v[4].ox = ox_top; v[4].oy = oy_top;
            v[5].ox = ox_top; v[5].oy = oy_top;
        }
    }

    GSPGPU_FlushDataCache(s_npcVbo, (u32)(s_npcCount * 6) * sizeof(TileVertex));
    s_vboDirty = false;
}

void npcInit(void) {
    s_npcCount = 0;
    s_vboDirty = true;

    Tex3DS_Texture nt3x = Tex3DS_TextureImport(npcs_t3x, npcs_t3x_size, &s_npcTex, NULL, false);
    if (nt3x) {
        Tex3DS_TextureFree(nt3x);
        C3D_TexSetFilter(&s_npcTex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&s_npcTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        s_npcTexReady = true;
    }

    if (!s_npcVbo) {
        s_npcVbo = (TileVertex*)linearAlloc(MAX_NPCS * 6 * sizeof(TileVertex));
    }

    int count = (int)(sizeof(s_initialNpcs) / sizeof(s_initialNpcs[0]));
    for (int i = 0; i < count; ++i) {
        npcAdd(
            s_initialNpcs[i].start_x,
            s_initialNpcs[i].start_y,
            s_initialNpcs[i].dir,
            s_initialNpcs[i].sprite_id,
            s_initialNpcs[i].move_mode,
            s_initialNpcs[i].wander_radius
        );
    }
}

void npcExit(void) {
    if (s_npcTexReady) {
        C3D_TexDelete(&s_npcTex);
        s_npcTexReady = false;
    }
    if (s_npcVbo) {
        linearFree(s_npcVbo);
        s_npcVbo = NULL;
    }
    s_npcCount = 0;
}

int npcAdd(int gx, int gy, PlayerDirection dir, int sprite_id, NpcMoveMode move_mode, int wander_radius) {
    if (s_npcCount >= MAX_NPCS) return -1;
    if (wander_radius < 0) wander_radius = 0;

    int idx = s_npcCount++;
    s_npcs[idx] = (Npc){
        .home_x        = gx,
        .home_y        = gy,
        .wander_radius = wander_radius,
        .move_mode     = move_mode,
        .grid_x        = gx,
        .grid_y        = gy,
        .target_x      = gx,
        .target_y      = gy,
        .pos_x         = (float)gx,
        .pos_y         = (float)gy,
        .dir           = dir,
        .sprite_id     = sprite_id,
        .moving        = false,
        .move_progress = 0.0f,
        .step_parity   = 0,
        .wait_frames   = 30 + (idx * 25)
    };
    s_vboDirty = true;
    return idx;
}

void npcUpdateAll(void) {
    static const int s_dx[4] = { 0,  0, -1, 1 };
    static const int s_dy[4] = { 1, -1,  0, 0 };

    for (int i = 0; i < s_npcCount; ++i) {
        Npc* npc = &s_npcs[i];

        if (npc->moving) {
            npc->move_progress += NPC_MOVE_SPEED;
            if (npc->move_progress >= 1.0f) {
                npc->grid_x        = npc->target_x;
                npc->grid_y        = npc->target_y;
                npc->pos_x         = (float)npc->grid_x;
                npc->pos_y         = (float)npc->grid_y;
                npc->move_progress = 0.0f;
                npc->moving        = false;
                npc->step_parity  ^= 1;
                npc->wait_frames   = 45 + (rand() % 60);
            } else {
                npc->pos_x = (float)npc->grid_x + (float)(npc->target_x - npc->grid_x) * npc->move_progress;
                npc->pos_y = (float)npc->grid_y + (float)(npc->target_y - npc->grid_y) * npc->move_progress;
            }
            s_vboDirty = true;
            continue;
        }

        if (npc->move_mode != NPC_MOVE_WANDER) {
            continue;
        }

        if (npc->wait_frames > 0) {
            npc->wait_frames--;
            continue;
        }

        int d = rand() % 4;
        npc->dir = (PlayerDirection)d;
        s_vboDirty = true;

        int nx = npc->grid_x + s_dx[d];
        int ny = npc->grid_y + s_dy[d];

        // Limita il movimento al massimo in un'area 3x3 (wander_radius = 1) attorno al punto di partenza
        bool in_bounds = (abs(nx - npc->home_x) <= npc->wander_radius) &&
                         (abs(ny - npc->home_y) <= npc->wander_radius);

        if (in_bounds && !playerIsAt(nx, ny) && dsMapCanMove(npc->grid_x, npc->grid_y, nx, ny)) {
            npc->target_x      = nx;
            npc->target_y      = ny;
            npc->moving        = true;
            npc->move_progress = 0.0f;
        } else {
            npc->wait_frames = 25 + (rand() % 40);
        }
    }
}

int npcGetCount(void) {
    return s_npcCount;
}

Npc* npcGetAt(int gx, int gy) {
    for (int i = 0; i < s_npcCount; ++i) {
        if (s_npcs[i].grid_x == gx && s_npcs[i].grid_y == gy) {
            return &s_npcs[i];
        }
        if (s_npcs[i].moving && s_npcs[i].target_x == gx && s_npcs[i].target_y == gy) {
            return &s_npcs[i];
        }
    }
    return NULL;
}

bool npcIsAt(int gx, int gy) {
    return npcGetAt(gx, gy) != NULL;
}

bool npcShouldOverlayTile(int gx, int gy) {
    for (int i = 0; i < s_npcCount; ++i) {
        const Npc* npc = &s_npcs[i];
        if (characterShouldOverlayTile(
                gx, gy,
                npc->grid_x, npc->grid_y,
                npc->target_x, npc->target_y,
                npc->dir, npc->moving, npc->move_progress)) {
            return true;
        }
    }
    return false;
}

void npcDrawAll(float cam_x, float cam_y, float cam_z, float cam_dist) {
    if (!s_npcTexReady || !s_npcVbo || s_npcCount <= 0) return;

    buildNpcVbo(cam_x, cam_y, cam_z, cam_dist);

    C3D_BufInfo* buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, s_npcVbo, sizeof(TileVertex), 3, 0x210);

    C3D_TexBind(0, &s_npcTex);
    C3D_DrawArrays(GPU_TRIANGLES, 0, s_npcCount * 6);
}
