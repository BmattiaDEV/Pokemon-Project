#include "common.h"
#include "player.h"
#include "renderer.h"
#include "ds_map.h"
#include "encounter.h"
#include "audio.h"
#include "twilight_outpost_wav.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C3D_CullFace(GPU_CULL_NONE);

    // Avvia l'audio e la musica di sottofondo in loop
    audioInit();
    audioPlayLoopingWav(twilight_outpost_wav, twilight_outpost_wav_size);

    // Posiziona il giocatore al centro del prato della mappa DS (x = 18, y = 14)
    playerInit(18, 14);
    rendererInit();
    encounterInit();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        if (kDown & KEY_START) break;

        if (encounterGetState() == GAME_STATE_ENCOUNTER) {
            if (kDown & KEY_A) {
                encounterDismiss();
            }
            rendererDrawEncounterFrame(encounterGetWildMonIndex());
        } else {
            dsMapUpdate(kHeld, kDown);
            if (encounterGetState() == GAME_STATE_ENCOUNTER) {
                rendererDrawEncounterFrame(encounterGetWildMonIndex());
            } else {
                rendererDrawFrame();
            }
        }
    }

    rendererExit();
    audioExit();
    C3D_Fini();
    gfxExit();
    return 0;
}
