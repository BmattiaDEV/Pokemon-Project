#include "graphics.h"
#include "../world/map_builder.h"
#include "../world/map.h"

// Inclusione del file binario convertito dalla texture tileset
#include "outdoor_bin.h" 

#define MAX_VERTS 50000

// Struttura del singolo vertice 3D (Posizione, UV Texture, Colore RGB)
typedef struct {
    float x, y, z;
    float u, v;
    float r, g, b;
} Vertex;

// Variabili globali del modulo grafico
static Vertex* vbo = NULL;
static int vertCount = 0;
int mapVertCount = 0; // Usato per sapere quanti vertici disegnare

static C3D_Tex outdoorTex;

// Funzione interna per aggiungere un vertice nel VBO
void addVert(float x, float y, float z, float u, float v, const float* c) {
    if (vertCount >= MAX_VERTS) return;
    Vertex* p = &vbo[vertCount++];
    p->x = x; p->y = y; p->z = z; 
    p->u = u; p->v = v;
    p->r = c[0]; p->g = c[1]; p->b = c[2];
}

// Algoritmo Morton per il caricamento delle texture ottimizzato per la GPU del 3DS
static u32 morton(u32 x, u32 y) {
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}

// Funzione per caricare un file binario grezzo dentro una C3D_Tex del 3DS
static int loadTexture(C3D_Tex* tex, const u8* bin_data, u32 bin_size, int w, int h) {
    if (!C3D_TexInit(tex, w, h, GPU_RGBA8)) return 0;
    u8* dst = (u8*)tex->data;
    memset(dst, 0, w * h * 4);
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if ((u32)((y * w + x) * 4 + 3) >= bin_size) continue;
            const u8* s = &bin_data[(y * w + x) * 4];
            u32 tile = (u32)(y >> 3) * (w >> 3) + (u32)(x >> 3);
            u32 off = (tile * 64 + morton(x & 7, y & 7)) * 4;
            // Inversione canali colore RGBA per il 3DS
            dst[off + 0] = s[3]; 
            dst[off + 1] = s[2]; 
            dst[off + 2] = s[1]; 
            dst[off + 3] = s[0];
        }
    }
    GSPGPU_FlushDataCache(dst, w * h * 4);
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST); 
    C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    return 1;
}

// Configura l'ambiente texture della GPU per usare l'immagine al posto dei colori puri
static void useTexture(void) {
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    C3D_AlphaTest(true, GPU_GREATER, 0x80); 
}

// --- INIZIALIZZAZIONE GRAFICA (Chiamata all'avvio) ---
bool graphicsInit(void) {
    // 1. Allochiamo lo spazio nella RAM lineare per i vertici 3D
    vbo = (Vertex*)linearAlloc(MAX_VERTS * sizeof(Vertex));
    if (!vbo) return false;

    // 2. Chiamiamo il "muratore" (map_builder) per calcolare la geometria della mappa
    costruisciMappa3D();
    
    // 3. Salviamo quanti vertici totali sono stati generati
    mapVertCount = vertCount;

    // 4. Colleghiamo il VBO alla pipeline grafica del 3DS
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, vbo, sizeof(Vertex), 3, 0x210); // 3 attributi: pos(3), uv(2), color(3)

    // 5. Carichiamo la texture del tileset (outdoor.bin generato da python)
    if (!loadTexture(&outdoorTex, outdoor_bin, outdoor_bin_size, 64, 128)) {
        return false;
    }

    return true;
}

// --- PULIZIA MEMORIA (Chiamata alla chiusura) ---
void graphicsFree(void) {
    C3D_TexDelete(&outdoorTex);
    if (vbo) linearFree(vbo);
}

// RENDER STATICO DELLA MAPPA
void graphicsRenderOverworld(C3D_RenderTarget* target, int uLoc_projection, int uLoc_modelView, int uLoc_uvoff, C3D_Mtx* projection, float wx, float wz, int tx, int ty, int frame) {
    
    // 1. Coordinate fisse per inquadrare tutta la mappa (16x12)
    float cx = 8.0f;  // Centro mappa (X)
    float cy = 0.0f;  // Altezza pavimento
    float cz = 6.0f;  // Centro mappa (Z)

    C3D_Mtx view;
    Mtx_Identity(&view);
    Mtx_Translate(&view, 0.0f, 0.0f, -22.0f, true); // Allontana la telecamera per vederla tutta
    Mtx_RotateX(&view, C3D_AngleFromDegrees(45.0f), true); // Inclinata dall'alto
    Mtx_Translate(&view, -cx, -cy, -cz, true); // Punta al centro

// 2. Pulizia Schermo
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x202838FF, 0);
    C3D_FrameDrawOn(target);
    
    // Mostra tutte le facce
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    // --- NUOVO: ATTIVA LA TRASPARENZA (Alpha Blending) ---
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0); // Scarta i pixel invisibili
    
    // LA RIGA MAGICA: Disabilita il face culling per far apparire i pavimenti
    C3D_CullFace(GPU_CULL_NONE);
    
    // Assicurati che il test di profondità sia GEQUAL per evitare sfarfallii
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    
    // 3. Setup Shaders
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &view);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_uvoff, 0.0f, 0.0f, 0.0f, 0.0f);
    
    // 4. Render
    useTexture();
    C3D_TexBind(0, &outdoorTex);
    C3D_DrawArrays(GPU_TRIANGLES, 0, mapVertCount);
}