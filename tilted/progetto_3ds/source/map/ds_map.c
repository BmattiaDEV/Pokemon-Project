#include "ds_map_internal.h"
#include "player.h"
#include "npc.h"
#include "dialogue.h"
#include "transition.h"
#include "encounter.h"
#include "player_t3x.h"
#include "flower_overlay_t3x.h"
#include "grass_overlay_t3x.h"

typedef struct {
    DsMapId         fromMap;
    int             gx_min;
    int             gx_max;
    int             gy_min;
    int             gy_max;
    DsMapId         toMap;
    int             dest_base_x;
    int             dest_base_y;
    PlayerDirection dest_dir;
} DsWarpDef;

// Tabella collegamenti tra mappe (zone evento ombra fenter_lm6 vicino agli alberi)
static const DsWarpDef s_warps[] = {
    // Mappa 0 -> Mappa 1: entrando nell'arco d'ombra in alto (gx=21..22, gy=11)
    { DS_MAP_ROUTE,    21, 22, 11, 11, DS_MAP_CLEARING, 16, 26, DIR_UP },
    // Mappa 1 -> Mappa 0: tornando nell'arco d'ombra in basso (gx=16..17, gy=27)
    { DS_MAP_CLEARING, 16, 17, 27, 27, DS_MAP_ROUTE,    21, 12, DIR_DOWN },
};

// Istanza globale dello stato della mappa attualmente caricata
DsMapState g_dsMap = {
    .ready   = false,
    .mapId   = DS_MAP_ROUTE,
    .scale   = 1.0f,
    .camDist = DS_CAM_DIST_START
};

static bool dsMapLoad(DsMapId mapId) {
    float savedCamDist = (g_dsMap.camDist > 0.0f) ? g_dsMap.camDist : DS_CAM_DIST_START;

    memset(&g_dsMap, 0, sizeof(g_dsMap));
    g_dsMap.mapId            = mapId;
    g_dsMap.scale            = 1.0f;
    g_dsMap.camDist          = savedCamDist;
    g_dsMap.treeSingleGroup  = -1;
    g_dsMap.contTreeBotGroup = -1;
    g_dsMap.contTreeTopGroup = -1;
    g_dsMap.stairsGroup      = -1;
    g_dsMap.beachGroup       = -1;
    g_dsMap.waterGroup       = -1;
    g_dsMap.flowerAnimGroup  = -1;

    // 1. Seleziona i gruppi del modello per la mappa richiesta
    if (mapId == DS_MAP_CLEARING) {
        g_dsMap.groupCount = g_dsMap1GroupCount;
        dsMap1FillGroups(g_dsMap.groups);
    } else {
        g_dsMap.groupCount = g_dsMap0GroupCount;
        dsMap0FillGroups(g_dsMap.groups);
    }

    // Risolve gli indici dei gruppi per ruolo logico
    for (u32 g = 0; g < g_dsMap.groupCount; ++g) {
        switch (g_dsMap.groups[g].role) {
            case DS_MAT_TREE_SINGLE:  g_dsMap.treeSingleGroup  = (int)g; break;
            case DS_MAT_CONTTREE_BOT: g_dsMap.contTreeBotGroup = (int)g; break;
            case DS_MAT_CONTTREE_TOP: g_dsMap.contTreeTopGroup = (int)g; break;
            case DS_MAT_STAIRS:       g_dsMap.stairsGroup      = (int)g; break;
            case DS_MAT_BEACH:        g_dsMap.beachGroup       = (int)g; break;
            case DS_MAT_WATER_ANIM:   g_dsMap.waterGroup       = (int)g; break;
            case DS_MAT_FLOWER_ANIM:  g_dsMap.flowerAnimGroup  = (int)g; break;
            default: break;
        }
    }

    if (!dsMapBuilderBuildVbo()) {
        return false;
    }

    // 2. Carica le texture dei materiali del modello 3D corrente
    for (u32 g = 0; g < g_dsMap.groupCount; ++g) {
        texLoadFromMem(
            g_dsMap.groups[g].tex,
            g_dsMap.groups[g].texSize,
            &g_dsMap.tex[g],
            GPU_NEAREST,
            GPU_REPEAT
        );
    }

    // 3. Carica le texture del giocatore e degli overlay sui piedi
    g_dsMap.playerTexReady = texLoadFromMem(
        player_t3x, player_t3x_size, &g_dsMap.playerTex, GPU_NEAREST, GPU_CLAMP_TO_EDGE
    );
    g_dsMap.flowerTexReady = texLoadFromMem(
        flower_overlay_t3x, flower_overlay_t3x_size, &g_dsMap.flowerTex, GPU_NEAREST, GPU_CLAMP_TO_EDGE
    );
    g_dsMap.grassTexReady = texLoadFromMem(
        grass_overlay_t3x, grass_overlay_t3x_size, &g_dsMap.grassTex, GPU_NEAREST, GPU_CLAMP_TO_EDGE
    );

    // 4. Costruisce la griglia logica 32x32 di collisione, altezze e alberi 2x2
    dsMapBuilderBuildCollisionGrid();

    // 5. Inizializza le animazioni (acqua/fiori se presenti nella mappa) e gli NPC della mappa
    dsMapAnimInit();
    npcInit((int)mapId);

    g_dsMap.ready = true;
    return true;
}

void dsMapInit(void) {
    if (g_dsMap.ready) return;
    g_dsMap.camDist = DS_CAM_DIST_START;
    dsMapLoad(DS_MAP_ROUTE);
}

void dsMapExit(void) {
    npcExit();
    dsMapAnimExit();

    if (g_dsMap.ready) {
        for (u32 g = 0; g < g_dsMap.groupCount; ++g) {
            C3D_TexDelete(&g_dsMap.tex[g]);
        }
        if (g_dsMap.playerTexReady) {
            C3D_TexDelete(&g_dsMap.playerTex);
            g_dsMap.playerTexReady = false;
        }
        if (g_dsMap.flowerTexReady) {
            C3D_TexDelete(&g_dsMap.flowerTex);
            g_dsMap.flowerTexReady = false;
        }
        if (g_dsMap.grassTexReady) {
            C3D_TexDelete(&g_dsMap.grassTex);
            g_dsMap.grassTexReady = false;
        }
    }

    if (g_dsMap.vbo) {
        linearFree(g_dsMap.vbo);
        g_dsMap.vbo = NULL;
    }

    g_dsMap.ready = false;
}

typedef struct {
    DsMapId         toMap;
    int             dest_x;
    int             dest_y;
    PlayerDirection dest_dir;
} DsPendingWarp;

static DsPendingWarp s_pendingWarp;

static void executePendingWarp(void* user_data) {
    const DsPendingWarp* pw = (const DsPendingWarp*)user_data;
    if (!pw) return;

    dsMapExit();
    dsMapLoad(pw->toMap);
    playerInit(pw->dest_x, pw->dest_y);
    playerSetDirection(pw->dest_dir);
    encounterInit();
}

bool dsMapTryWarpAt(int gx, int gy) {
    if (transitionIsActive()) return true;

    int count = (int)(sizeof(s_warps) / sizeof(s_warps[0]));
    for (int i = 0; i < count; ++i) {
        const DsWarpDef* w = &s_warps[i];
        if (w->fromMap != g_dsMap.mapId) continue;
        if (gx >= w->gx_min && gx <= w->gx_max && gy >= w->gy_min && gy <= w->gy_max) {
            s_pendingWarp = (DsPendingWarp){
                .toMap    = w->toMap,
                .dest_x   = w->dest_base_x + (gx - w->gx_min),
                .dest_y   = w->dest_base_y + (gy - w->gy_min),
                .dest_dir = w->dest_dir
            };
            transitionStart(executePendingWarp, &s_pendingWarp);
            return true;
        }
    }
    return false;
}

void dsMapUpdate(u32 kHeld, u32 kDown) {
    // Controllo zoom telecamera (L/R per avvicinare/allontanare, X per ripristinare)
    if (kDown & KEY_X) g_dsMap.camDist = DS_CAM_DIST_START;
    if (kHeld & KEY_L) g_dsMap.camDist -= DS_ZOOM_STEP;
    if (kHeld & KEY_R) g_dsMap.camDist += DS_ZOOM_STEP;
    if (g_dsMap.camDist < DS_CAM_DIST_MIN) g_dsMap.camDist = DS_CAM_DIST_MIN;
    if (g_dsMap.camDist > DS_CAM_DIST_MAX) g_dsMap.camDist = DS_CAM_DIST_MAX;

    // Avanza timer animazioni mappa (acqua e fiori)
    dsMapAnimUpdate();

    if (transitionIsActive()) {
        transitionUpdate();
        return;
    }

    if (dialogueIsActive()) {
        dialogueUpdate(kDown);
    } else {
        if ((kDown & KEY_A) && !playerIsMoving()) {
            int pgx, pgy;
            playerGetTilePos(&pgx, &pgy);
            npcTryInteract(pgx, pgy, playerGetDirection());
        }
        if (!dialogueIsActive()) {
            playerUpdate(kDown, kHeld);
        }
    }

    if (!transitionIsActive()) {
        npcUpdateAll();
    }
}
