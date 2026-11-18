#pragma once

#include "ds_map.h"

// Mappa 0: percorso costiero ed entrata della foresta
extern const u32 g_dsMap0GroupCount;
void dsMap0FillGroups(DsMapGroupData g[DS_MAX_MAP_GROUPS]);

// Mappa 1: radura con sentieri di sabbia e aiuole recintate
extern const u32 g_dsMap1GroupCount;
void dsMap1FillGroups(DsMapGroupData g[DS_MAX_MAP_GROUPS]);
