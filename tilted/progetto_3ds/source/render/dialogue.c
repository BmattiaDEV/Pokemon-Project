#include "dialogue.h"
#include "npc_texts.h"
#include "dialogue_box_t3x.h"
#include "ui_font_t3x.h"

// Parametri layout schermo superiore (400x240) e atlas texture
#define DLG_BOX_TEX_W       512.0f
#define DLG_BOX_TEX_H        64.0f
#define DLG_BOX_W           384.0f
#define DLG_BOX_H            54.0f
#define DLG_BOX_X             8.0f
#define DLG_BOX_Y           180.0f

#define DLG_TEXT_X           (DLG_BOX_X + 18.0f)
#define DLG_LINE1_Y          (DLG_BOX_Y + 10.0f)
#define DLG_LINE2_Y          (DLG_BOX_Y + 27.0f)

#define DLG_ARROW_SRC_X     400.0f
#define DLG_ARROW_SRC_Y       0.0f
#define DLG_ARROW_W          16.0f
#define DLG_ARROW_H          16.0f
#define DLG_ARROW_X          (DLG_BOX_X + DLG_BOX_W - 28.0f)
#define DLG_ARROW_Y          (DLG_BOX_Y + DLG_BOX_H - 22.0f)

#define FONT_TEX_W          256.0f
#define FONT_TEX_H          128.0f
#define FONT_CELL_SIZE       16.0f
#define FONT_COLS            16

#define DLG_MAX_GLYPH_QUADS 128
#define DLG_VBO_VERTS       ((2 + DLG_MAX_GLYPH_QUADS) * 6)
#define DLG_CHARS_PER_FRAME   1

// Larghezze proporzionali in pixel per ASCII 32..127 in data/ui_font.png
static const u8 s_glyphWidths[96] = {
    5, 3, 7, 7, 7, 7, 7, 4, 5, 5, 7, 7, 4, 6, 4, 7,
    7, 6, 8, 8, 8, 8, 8, 8, 8, 8, 4, 4, 6, 6, 6, 8,
    7, 8, 8, 8, 8, 8, 8, 8, 8, 5, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 7, 5, 7, 7,
    4, 7, 7, 7, 7, 7, 6, 7, 7, 3, 6, 7, 3, 9, 7, 7,
    7, 7, 6, 7, 6, 7, 7, 9, 7, 7, 7, 5, 3, 5, 7, 7
};

static bool                    s_ready = false;
static C3D_Tex                 s_boxTex;
static C3D_Tex                 s_fontTex;
static TileVertex*             s_vbo = NULL;

static bool                    s_active = false;
static const NpcDialoguePage*  s_pages = NULL;
static int                     s_pageCount = 0;
static int                     s_pageIndex = 0;
static int                     s_charsShown = 0;
static int                     s_totalPageChars = 0;
static u32                     s_animTicks = 0;

static int getPageTotalChars(const NpcDialoguePage* page) {
    int len1 = page->line1 ? (int)strlen(page->line1) : 0;
    int len2 = page->line2 ? (int)strlen(page->line2) : 0;
    return len1 + len2;
}

static void addTextLineQuads(const char* text, float start_x, float start_y, int max_chars, u32* io_quadCount) {
    if (!text || max_chars <= 0) return;

    float cur_x = start_x;
    for (int i = 0; text[i] != '\0' && i < max_chars; ++i) {
        if (*io_quadCount >= DLG_MAX_GLYPH_QUADS) break;

        unsigned char ch = (unsigned char)text[i];
        int idx = (ch >= 32 && ch < 128) ? ((int)ch - 32) : ('?' - 32);

        if (ch != ' ') {
            int col = idx % FONT_COLS;
            int row = idx / FONT_COLS;

            float px0 = (float)col * FONT_CELL_SIZE;
            float py0 = (float)row * FONT_CELL_SIZE;
            float px1 = px0 + FONT_CELL_SIZE;
            float py1 = py0 + FONT_CELL_SIZE;

            float u0 = px0 / FONT_TEX_W;
            float u1 = px1 / FONT_TEX_W;
            float v1 = 1.0f - (py0 / FONT_TEX_H);
            float v0 = 1.0f - (py1 / FONT_TEX_H);

            u32 vertBase = (2 + *io_quadCount) * 6;
            gfxSetQuad2D(&s_vbo[vertBase], cur_x, start_y, cur_x + FONT_CELL_SIZE, start_y + FONT_CELL_SIZE, u0, v0, u1, v1);
            (*io_quadCount)++;
        }

        cur_x += (float)s_glyphWidths[idx];
    }
}

void dialogueInit(void) {
    if (s_ready) return;

    texLoadFromMem(dialogue_box_t3x, dialogue_box_t3x_size, &s_boxTex, GPU_NEAREST, GPU_CLAMP_TO_EDGE);
    texLoadFromMem(ui_font_t3x, ui_font_t3x_size, &s_fontTex, GPU_NEAREST, GPU_CLAMP_TO_EDGE);

    s_vbo = (TileVertex*)linearAlloc(DLG_VBO_VERTS * sizeof(TileVertex));
    s_active = false;
    s_ready = (s_vbo != NULL);
}

void dialogueExit(void) {
    if (s_ready) {
        C3D_TexDelete(&s_boxTex);
        C3D_TexDelete(&s_fontTex);
        s_ready = false;
    }
    if (s_vbo) {
        linearFree(s_vbo);
        s_vbo = NULL;
    }
    s_active = false;
}

void dialogueStart(int text_id) {
    int count = 0;
    const NpcDialoguePage* pages = npcTextsGet(text_id, &count);
    if (!pages || count <= 0) return;

    s_pages          = pages;
    s_pageCount      = count;
    s_pageIndex      = 0;
    s_charsShown     = 0;
    s_totalPageChars = getPageTotalChars(&s_pages[0]);
    s_animTicks      = 0;
    s_active         = true;
}

bool dialogueIsActive(void) {
    return s_active;
}

void dialogueUpdate(u32 kDown) {
    if (!s_active || !s_pages) return;

    s_animTicks++;

    if (s_charsShown < s_totalPageChars) {
        if (kDown & (KEY_A | KEY_B)) {
            // Il giocatore accelera la comparsa completando subito la pagina corrente
            s_charsShown = s_totalPageChars;
        } else {
            s_charsShown += DLG_CHARS_PER_FRAME;
            if (s_charsShown > s_totalPageChars) {
                s_charsShown = s_totalPageChars;
            }
        }
        return;
    }

    if (kDown & (KEY_A | KEY_B)) {
        s_pageIndex++;
        if (s_pageIndex >= s_pageCount) {
            s_active = false;
            s_pages = NULL;
            s_pageCount = 0;
        } else {
            s_charsShown     = 0;
            s_totalPageChars = getPageTotalChars(&s_pages[s_pageIndex]);
        }
    }
}

void dialogueDraw(int uLoc_projection, int uLoc_view) {
    if (!s_ready || !s_active || !s_pages || !s_vbo) return;

    // 1. Quad 0: finestra del box dialoghi in basso sullo schermo superiore (384x54)
    {
        float u0 = 0.0f;
        float u1 = DLG_BOX_W / DLG_BOX_TEX_W;
        float v1 = 1.0f;
        float v0 = 1.0f - (DLG_BOX_H / DLG_BOX_TEX_H);
        gfxSetQuad2D(&s_vbo[0], DLG_BOX_X, DLG_BOX_Y, DLG_BOX_X + DLG_BOX_W, DLG_BOX_Y + DLG_BOX_H, u0, v0, u1, v1);
    }

    // 2. Quad 1: triangolino animato "premi A" quando la pagina è completa
    bool pageComplete = (s_charsShown >= s_totalPageChars);
    if (pageComplete) {
        int bounce = ((s_animTicks / 16) % 2 == 0) ? 0 : 2;
        float ay = DLG_ARROW_Y + (float)bounce;
        float u0 = DLG_ARROW_SRC_X / DLG_BOX_TEX_W;
        float u1 = (DLG_ARROW_SRC_X + DLG_ARROW_W) / DLG_BOX_TEX_W;
        float v1 = 1.0f - (DLG_ARROW_SRC_Y / DLG_BOX_TEX_H);
        float v0 = 1.0f - ((DLG_ARROW_SRC_Y + DLG_ARROW_H) / DLG_BOX_TEX_H);
        gfxSetQuad2D(&s_vbo[6], DLG_ARROW_X, ay, DLG_ARROW_X + DLG_ARROW_W, ay + DLG_ARROW_H, u0, v0, u1, v1);
    }

    // 3. Quad 2..N: caratteri visibili di riga 1 e riga 2
    const NpcDialoguePage* page = &s_pages[s_pageIndex];
    int len1 = page->line1 ? (int)strlen(page->line1) : 0;
    int line1Chars = (s_charsShown < len1) ? s_charsShown : len1;
    int line2Chars = (s_charsShown > len1) ? (s_charsShown - len1) : 0;

    u32 glyphQuads = 0;
    addTextLineQuads(page->line1, DLG_TEXT_X, DLG_LINE1_Y, line1Chars, &glyphQuads);
    addTextLineQuads(page->line2, DLG_TEXT_X, DLG_LINE2_Y, line2Chars, &glyphQuads);

    GSPGPU_FlushDataCache(s_vbo, (2 + glyphQuads) * 6 * sizeof(TileVertex));

    // 4. Proiezione ortografica 2D (400x240) sopra la scena 3D
    C3D_Mtx orthoProj;
    Mtx_OrthoTilt(&orthoProj, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, true);

    C3D_Mtx orthoView;
    Mtx_Identity(&orthoView);

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &orthoProj);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_view, &orthoView);

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    C3D_BufInfo* buf = C3D_GetBufInfo();
    BufInfo_Init(buf);
    BufInfo_Add(buf, s_vbo, sizeof(TileVertex), 3, 0x210);

    C3D_TexBind(0, &s_boxTex);
    C3D_DrawArrays(GPU_TRIANGLES, 0, pageComplete ? 12 : 6);

    if (glyphQuads > 0) {
        C3D_TexBind(0, &s_fontTex);
        C3D_DrawArrays(GPU_TRIANGLES, 12, (int)(glyphQuads * 6));
    }

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
}
