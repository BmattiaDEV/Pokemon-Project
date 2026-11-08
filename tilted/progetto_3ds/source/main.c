#include "common.h"
#include "player.h"
#include "renderer.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C3D_CullFace(GPU_CULL_NONE);

    // Posiziona il giocatore al centro del sentiero (x = 8, y = 8)
    playerInit(8, 8);
    rendererInit();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        if (kDown & KEY_START) break;

        playerUpdate(kDown, kHeld);
        rendererDrawFrame();
    }

    rendererExit();
    C3D_Fini();
    gfxExit();
    return 0;
}
