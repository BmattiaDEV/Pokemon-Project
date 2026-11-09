#include "events.h"
#include "renderer.h"

// Tabella degli eventi di cambio mappa:
// - Sulla mappa principale 16x16 (MAP_MAIN), all'ultima X (x = 15) in corrispondenza dei tile del sentiero
//   (righe z/y = 4, 5, 6, 7, coprendo sia l'indice 0-based sia le righe 5, 6, 7 del CSV), passa alla mappa 8x8 piana (MAP_PLAIN_8X8).
// - Sulla mappa 8x8 (MAP_PLAIN_8X8), camminando sul bordo sinistro (x = 0, z/y = 3..5) si torna sul sentiero della mappa 16x16.
static const WorldEvent g_events[] = {
    { MAP_MAIN,      15, 4, 0, EVENT_WARP_MAP, MAP_PLAIN_8X8, 1, 4, DIR_RIGHT },
    { MAP_MAIN,      15, 5, 0, EVENT_WARP_MAP, MAP_PLAIN_8X8, 1, 4, DIR_RIGHT },
    { MAP_MAIN,      15, 6, 0, EVENT_WARP_MAP, MAP_PLAIN_8X8, 1, 5, DIR_RIGHT },
    { MAP_MAIN,      15, 7, 0, EVENT_WARP_MAP, MAP_PLAIN_8X8, 1, 5, DIR_RIGHT },

    // Ritorno dalla mappa 8x8 alla mappa principale 16x16
    { MAP_PLAIN_8X8,  0, 3, 0, EVENT_WARP_MAP, MAP_MAIN,     14, 5, DIR_LEFT  },
    { MAP_PLAIN_8X8,  0, 4, 0, EVENT_WARP_MAP, MAP_MAIN,     14, 5, DIR_LEFT  },
    { MAP_PLAIN_8X8,  0, 5, 0, EVENT_WARP_MAP, MAP_MAIN,     14, 6, DIR_LEFT  }
};

#define NUM_EVENTS (sizeof(g_events) / sizeof(g_events[0]))

bool eventsCheckOnStep(int x, int y, int level) {
    MapId current_map = worldGetCurrentMapId();

    for (unsigned int i = 0; i < NUM_EVENTS; ++i) {
        const WorldEvent* ev = &g_events[i];
        if (ev->source_map == current_map &&
            ev->x == x &&
            ev->y == y &&
            ev->level == level) {

            if (ev->type == EVENT_WARP_MAP) {
                worldSetCurrentMap(ev->target_map);
                rendererReloadMap();
                playerInit(ev->target_x, ev->target_y);
                playerSetDirection(ev->target_dir);
                return true;
            }
        }
    }
    return false;
}

