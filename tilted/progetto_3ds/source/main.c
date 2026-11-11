#include "common.h"
#include "player.h"
#include "renderer.h"
#include "ds_map.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C3D_CullFace(GPU_CULL_NONE);

    // Posiziona il giocatore al centro del prato della mappa DS (x = 18, y = 14)
    playerInit(18, 14);
    rendererInit();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        if (kDown & KEY_START) break;

        dsMapUpdate(kHeld, kDown);
        rendererDrawFrame();
    }

    rendererExit();
    C3D_Fini();
    gfxExit();
    return 0;
}
