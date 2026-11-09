#pragma once

#include "common.h"
#include "world.h"
#include "player.h"

typedef enum {
    EVENT_NONE = 0,
    EVENT_WARP_MAP
} EventType;

typedef struct {
    MapId source_map;
    int x;
    int y;
    int level;
    EventType type;
    MapId target_map;
    int target_x;
    int target_y;
    PlayerDirection target_dir;
} WorldEvent;

// Controlla ed esegue eventuali eventi ON_STEP quando il giocatore completa un passo sulla cella (x, y) a quota `level`
bool eventsCheckOnStep(int x, int y, int level);

