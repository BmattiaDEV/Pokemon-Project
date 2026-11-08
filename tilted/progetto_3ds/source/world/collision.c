#include "collision.h"
#include "world.h"

int collisionGetWalkableLevel(int x, int y) {
    if (x < 0 || x >= MAP_GEN_W || y < 0 || y >= MAP_GEN_H) {
        return COLLISION_BLOCKED_LEVEL;
    }

    // Scansioniamo dal livello più alto al più basso per trovare il terreno effettivo o eventuali pareti rocciose
    for (int level = MAP_GEN_MAX_LEVEL; level >= MAP_GEN_MIN_LEVEL; --level) {
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

    if (to_x < 0 || to_x >= MAP_GEN_W || to_y < 0 || to_y >= MAP_GEN_H) {
        return false;
    }

    // 1. Collisione con dislivelli d'altezza e pareti rocciose (montagne)
    int target_level = collisionGetWalkableLevel(to_x, to_y);
    if (target_level == COLLISION_BLOCKED_LEVEL || target_level != from_level) {
        return false;
    }

    // 2. Collisione con strutture solide (alberi alla stessa altezza del giocatore)
    // Sentieri (73..117), fiori (16..27) e chiome alte degli alberi sono attraversabili
    for (int level = MAP_GEN_MIN_LEVEL; level <= MAP_GEN_MAX_LEVEL; ++level) {
        int tid = worldGetRawTile(level, to_x, to_y);
        if (tid == -1) {
            continue;
        }

        if (isSolidTreeTile(tid)) {
            int k = worldGetTreeRowFromBottom(level, to_x, to_y, tid);
            int tree_base_y = to_y + k;
            int x_trunk = to_x + getTreeTrunkOffsetX(tid);
            if (x_trunk < 0) x_trunk = 0;
            if (x_trunk >= MAP_GEN_W) x_trunk = MAP_GEN_W - 1;

            int tree_ground_level = worldGetOverlayBaseLevel(level, x_trunk, tree_base_y);
            if (tree_ground_level == from_level) {
                return false;
            }
        }
    }

    return true;
}

