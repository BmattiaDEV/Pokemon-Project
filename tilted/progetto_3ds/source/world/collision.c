#include "collision.h"
#include "world.h"

int collisionGetWalkableLevel(int x, int y) {
    int map_w = worldGetMapWidth();
    int map_h = worldGetMapHeight();
    if (x < 0 || x >= map_w || y < 0 || y >= map_h) {
        return COLLISION_BLOCKED_LEVEL;
    }

    int min_l = worldGetMinLevel();
    int max_l = worldGetMaxLevel();

    // Scansioniamo dal livello più alto al più basso per trovare il terreno effettivo o eventuali pareti rocciose
    for (int level = max_l; level >= min_l; --level) {
        int tid = worldGetRawTile(level, x, y);

        // Le pareti inclinate della montagna (cliff borders) sono ostacoli insuperabili
        if (tid != -1 && isCliffBorderTile(tid)) {
            return COLLISION_BLOCKED_LEVEL;
        }

        // Se la cella si trova dentro l'altopiano di una montagna (anche se contiene fiori/overlay)
        // oppure contiene un tile solido di terreno (non-overlay), la sua quota calpestabile è `level`
        if (worldIsInsideMountainPlateau(level, x, y) || (tid != -1 && !isOverlayTile(tid))) {
            return level;
        }
    }

    return worldGetBaseGroundLevel();
}

bool collisionCanMove(int from_x, int from_y, int from_level, int to_x, int to_y) {
    (void)from_x;
    (void)from_y;

    int map_w = worldGetMapWidth();
    int map_h = worldGetMapHeight();
    if (to_x < 0 || to_x >= map_w || to_y < 0 || to_y >= map_h) {
        return false;
    }

    // 1. Collisione con dislivelli d'altezza e pareti rocciose (montagne)
    int target_level = collisionGetWalkableLevel(to_x, to_y);
    if (target_level == COLLISION_BLOCKED_LEVEL || target_level != from_level) {
        return false;
    }

    // 2. Collisione con strutture solide su qualsiasi sub-layer (alberi alla stessa altezza del giocatore)
    // Sentieri (73..117), fiori (16..27) e chiome alte degli alberi sono attraversabili
    int num_layers = worldGetNumLayers();
    for (int layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
        int tid = worldGetLayerRawTile(layer_idx, to_x, to_y);
        if (tid == -1) {
            continue;
        }

        if (isSolidTreeTile(tid)) {
            int level = worldGetLayerHeight(layer_idx);
            int k = worldGetTreeRowFromBottom(layer_idx, to_x, to_y, tid);
            int tree_base_y = to_y + k;
            int x_trunk = to_x + getTreeTrunkOffsetX(tid);
            if (x_trunk < 0) x_trunk = 0;
            if (x_trunk >= map_w) x_trunk = map_w - 1;

            int tree_ground_level = worldGetOverlayBaseLevel(level, x_trunk, tree_base_y);
            if (tree_ground_level == from_level) {
                return false;
            }
        }
    }

    return true;
}
