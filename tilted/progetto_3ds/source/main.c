#include "common.h"
#include "renderer.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C3D_CullFace(GPU_CULL_NONE);

    rendererInit();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        rendererDrawFrame();
    }

    rendererExit();
    C3D_Fini();
    gfxExit();
    return 0;
}
