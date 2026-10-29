#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

#include "core/common.h"
#include "render/graphics.h"
#include "world/map.h"
#include "vshader_shbin.h" 

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

int main(void) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    consoleInit(GFX_BOTTOM, NULL);

    printf("Mappa 3D Statica\nPremi START per uscire.\n");

    C3D_RenderTarget* target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    DVLB_s* vshaderDvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgram_s program;
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vshaderDvlb->DVLE[0]);
    C3D_BindProgram(&program);

    int uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
    int uLoc_modelView  = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
    int uLoc_uvoff      = shaderInstanceGetUniformLocation(program.vertexShader, "uvoff");

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3);

    if (!graphicsInit()) {
        printf("Errore: Impossibile caricare la grafica.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
        }
        return 0;
    }

    C3D_Mtx projection;
    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(40.0f), C3D_AspectRatioTop, 0.1f, 100.0f, false);

    // CICLO PRINCIPALE PULITO - NESSUN INPUT NECESSARIO
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        
        // I valori del giocatore sono a 0 perché la camera ora è globale
        graphicsRenderOverworld(target, uLoc_projection, uLoc_modelView, uLoc_uvoff, &projection, 0, 0, 0, 0, 0);
        
        C3D_FrameEnd(0);
    }

    graphicsFree();
    shaderProgramFree(&program);
    DVLB_Free(vshaderDvlb);
    C3D_Fini();
    gfxExit();
    return 0;
}