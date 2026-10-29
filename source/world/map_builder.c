#include "map_builder.h"
#include "map.h"
#include "../../data/map/map_data.h"
#include "../render/graphics.h" // Per poter chiamare addVert()

typedef struct {
    float u0, v0, u1, v1;
} CacheRitagli;

static void creaPianoOrizzontaleTexture(const float punti_3D[4][3], float sinistra_u, float alto_v, float destra_u, float basso_v, const float* colore_luce) {
    static const int indice_vertici[6] = { 0, 1, 2, 0, 2, 3 };
    float u_angoli[4] = { sinistra_u, destra_u, destra_u, sinistra_u };
    float v_angoli[4] = { alto_v, alto_v, basso_v, basso_v }; 
    for (int i = 0; i < 6; i++) {
        int indice = indice_vertici[i];
        // Inviamo il vertice calcolato al motore grafico!
        addVert(punti_3D[indice][0], punti_3D[indice][1], punti_3D[indice][2], u_angoli[indice], v_angoli[indice], colore_luce);
    }
}

static void creaPareteVerticaleTexture(float x_inizio, float z_inizio, float x_fine, float z_fine, float y_basso, float y_alto, float sinistra_u, float alto_v, float destra_u, float basso_v, const float* colore_luce) {
    if (y_basso >= y_alto) return; 
    const float vertici_muro[4][3] = { { x_inizio, y_basso, z_inizio }, { x_fine, y_basso, z_fine }, { x_fine, y_alto, z_fine }, { x_inizio, y_alto, z_inizio } };
    creaPianoOrizzontaleTexture(vertici_muro, sinistra_u, alto_v, destra_u, basso_v, colore_luce);
}

void costruisciMappa3D(void) {
    static const float bianco[3] = { 1.0f, 1.0f, 1.0f };
    
    // La nostra Cache UV per le texture
    CacheRitagli cache[8];
    for (int i = 0; i < 8; i++) {
        int col = i % 2; 
        int row = i / 2; 
        cache[i].u0 = col * 0.5f;
        cache[i].u1 = cache[i].u0 + 0.5f;
        if (FLIP_V) {
            cache[i].v0 = 1.0f - (row * 0.25f);
            cache[i].v1 = 1.0f - ((row + 1) * 0.25f);
        } else {
            cache[i].v0 = row * 0.25f;
            cache[i].v1 = (row + 1) * 0.25f;
        }
    }

    for (int riga_z = 0; riga_z < MAP_H; riga_z++) {
        for (int colonna_x = 0; colonna_x < MAP_W; colonna_x++) {
            float x_ovest = (float)colonna_x, x_est = x_ovest + 1.0f;
            float z_nord = (float)riga_z,     z_sud = z_nord + 1.0f;
            float altezza_pavimento = getTileHeight(colonna_x, riga_z);
            
            int id_texture = mapTypes[riga_z][colonna_x] - '0';
            CacheRitagli uv = cache[id_texture]; 
            
            // Pavimento
            const float punti_pavimento[4][3] = { { x_ovest, altezza_pavimento, z_nord }, { x_est, altezza_pavimento, z_nord }, { x_est, altezza_pavimento, z_sud }, { x_ovest, altezza_pavimento, z_sud } };
            creaPianoOrizzontaleTexture(punti_pavimento, uv.u0, uv.v0, uv.u1, uv.v1, bianco);

            // Muri (Texture '0' -> grass_1)
            CacheRitagli muro_uv = cache[0];
            float alt_sud = getTileHeight(colonna_x, riga_z + 1);
            creaPareteVerticaleTexture(x_ovest, z_sud, x_est, z_sud, alt_sud, altezza_pavimento, muro_uv.u0, muro_uv.v0, muro_uv.u1, muro_uv.v1, bianco);
            
            float alt_nord = getTileHeight(colonna_x, riga_z - 1);
            creaPareteVerticaleTexture(x_est, z_nord, x_ovest, z_nord, alt_nord, altezza_pavimento, muro_uv.u0, muro_uv.v0, muro_uv.u1, muro_uv.v1, bianco);
            
            float alt_est = getTileHeight(colonna_x + 1, riga_z);
            creaPareteVerticaleTexture(x_est, z_sud, x_est, z_nord, alt_est, altezza_pavimento, muro_uv.u0, muro_uv.v0, muro_uv.u1, muro_uv.v1, bianco);
            
            float alt_ovest = getTileHeight(colonna_x - 1, riga_z);
            creaPareteVerticaleTexture(x_ovest, z_nord, x_ovest, z_sud, alt_ovest, altezza_pavimento, muro_uv.u0, muro_uv.v0, muro_uv.u1, muro_uv.v1, bianco);
        }
    }
}