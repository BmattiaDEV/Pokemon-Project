#include "ds_map_internal.h"
#include "player.h"
#include "npc.h"
#include "dialogue.h"
#include "player_t3x.h"
#include "flower_overlay_t3x.h"
#include "grass_overlay_t3x.h"

// Istanza globale dello stato della mappa attualmente caricata
DsMapState g_dsMap = {
    .ready   = false,
    .scale   = 1.0f,
    .camDist = DS_CAM_DIST_START
};

void dsMapInit(void) {
    if (g_dsMap.ready) return;

    memset(&g_dsMap, 0, sizeof(g_dsMap));
    g_dsMap.scale   = 1.0f;
    g_dsMap.camDist = DS_CAM_DIST_START;

    // 1. Popola i descrittori dei materiali e costruisce il VBO lineare della mappa
    dsMapFillGroups(g_dsMap.groups);
    if (!dsMapBuilderBuildVbo()) {
        return;
    }

    // 2. Carica le texture dei 18 materiali del modello 3D
    for (u32 g = 0; g < DS_MAP_GROUP_COUNT; ++g) {
        texLoadFromMem(
            g_dsMap.groups[g].tex,
            g_dsMap.groups[g].texSize,
            &g_dsMap.tex[g],
            GPU_NEAREST,
            GPU_REPEAT
        );
    }

    // 3. Carica le texture del giocatore e degli overlay statici (fiori ed erba alta)
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

    // 5. Inizializza le animazioni della mappa (fiori oscillanti e onde 3D dell'acqua)
    dsMapAnimInit();

    // 6. Inizializza gli NPC della mappa
    npcInit();

    g_dsMap.ready = true;
}

void dsMapExit(void) {
    npcExit();
    dsMapAnimExit();

    if (g_dsMap.ready) {
        for (u32 g = 0; g < DS_MAP_GROUP_COUNT; ++g) {
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

void dsMapUpdate(u32 kHeld, u32 kDown) {
    // Controllo zoom telecamera (L/R per avvicinare/allontanare, X per ripristinare)
    if (kDown & KEY_X) g_dsMap.camDist = DS_CAM_DIST_START;
    if (kHeld & KEY_L) g_dsMap.camDist -= DS_ZOOM_STEP;
    if (kHeld & KEY_R) g_dsMap.camDist += DS_ZOOM_STEP;
    if (g_dsMap.camDist < DS_CAM_DIST_MIN) g_dsMap.camDist = DS_CAM_DIST_MIN;
    if (g_dsMap.camDist > DS_CAM_DIST_MAX) g_dsMap.camDist = DS_CAM_DIST_MAX;

    // Avanza timer animazioni mappa (acqua e fiori)
    dsMapAnimUpdate();

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

    npcUpdateAll();
}
