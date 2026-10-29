#pragma once
#include "../core/common.h"

float getTerrainHeight(int x, int z);
float getStructureHeight(int x, int z);
int isBlocked(int x, int z);
int isGrass(int x, int z);