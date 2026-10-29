#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

// Includiamo i moduli della nostra architettura a cartelle
#include "core/common.h"
#include "render/graphics.h"
#include "world/map.h"

// Questo è il file shader generato in automatico dal Makefile
#include "vshader_shbin.h" 

// Impostazioni per il trasferimento dell'immagine allo schermo del 3DS
#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

int main(void) {
    // 1. INIZIALIZZAZIONE HARDWARE E CONSOLE (Schermo inferiore per il testo)
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    consoleInit(GFX_BOTTOM, NULL);

    printf("Caricamento Motore 3D...\n");

    // 2. CREAZIONE DELLO SCHERMO SUPERIORE 3D (Render Target)
    C3D_RenderTarget* target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    // 3. CARICAMENTO DELLO SHADER (Il "cervello" della GPU)
    DVLB_s* vshaderDvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgram_s program;
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vshaderDvlb->DVLE[0]);
    C3D_BindProgram(&program);

    // 4. COLLEGAMENTO VARIABILI DELLO SHADER
    int uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
    int uLoc_modelView  = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
    int uLoc_uvoff      = shaderInstanceGetUniformLocation(program.vertexShader, "uvoff");

    // 5. DEFINIZIONE DEI VERTICI (Spieghiamo alla GPU come leggere la struct Vertex)
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // X, Y, Z
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // U, V (Texture)
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3); // R, G, B (Colore)

    // 6. CHIAMIAMO IL NOSTRO MOTORE GRAFICO (graphicsInit)
    // Qui viene chiamata la fabbrica che calcola la mappa e carica il tileset!
    if (!graphicsInit()) {
        printf("Errore: Impossibile caricare la grafica.\nPremi START per uscire.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
            gfxFlushBuffers(); gspWaitForVBlank();
        }
        graphicsFree(); shaderProgramFree(&program);
        DVLB_Free(vshaderDvlb); C3D_Fini(); gfxExit();
        return 0;
    }

    // 7. IMPOSTAZIONE TELECAMERA (Prospettiva a 40 gradi)
    C3D_Mtx projection;
    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(40.0f), C3D_AspectRatioTop, 0.1f, 100.0f, false);

    // Posizione iniziale della telecamera per esplorare (al centro della mappa)
    float wx = 8.0f;
    float wz = 9.0f;

    printf("Mappa 3D Generata!\nUsa il D-Pad per volare.\nPremi START per uscire.\n");

    // ==========================================
    // CICLO PRINCIPALE DI GIOCO (60 FPS)
    // ==========================================
    while (aptMainLoop()) {
        
        // A) Leggi i pulsanti
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        // Esci dal gioco se premi START
        if (kDown & KEY_START) break;

        // B) Muovi la telecamera liberamente (Modalità Drone/Esplorazione)
        float velocita = 0.1f;
        if (kHeld & KEY_DUP)    wz -= velocita;
        if (kHeld & KEY_DDOWN)  wz += velocita;
        if (kHeld & KEY_DLEFT)  wx -= velocita;
        if (kHeld & KEY_DRIGHT) wx += velocita;

        // Calcoliamo su quale casella intera "vola" la telecamera
        int tx = (int)(wx + 0.5f);
        int ty = (int)(wz + 0.5f);

        // C) DISEGNA IL FRAME
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        
        // Chiama la nostra funzione di render per disegnare la mappa texturizzata!
        graphicsRenderOverworld(target, uLoc_projection, uLoc_modelView, uLoc_uvoff, &projection, wx, wz, tx, ty, 0);
        
        C3D_FrameEnd(0);
    }

    // ==========================================
    // CHIUSURA E PULIZIA DELLA MEMORIA
    // ==========================================
    graphicsFree();
    shaderProgramFree(&program);
    DVLB_Free(vshaderDvlb);
    C3D_Fini();
    gfxExit();
    
    return 0;
}