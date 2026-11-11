#include "encounter.h"
#include "ds_map.h"
#include <stdio.h>
#include <stdlib.h>

static GameState s_state = GAME_STATE_WORLD;
static int       s_movesToEncounter = -1; // -1 = inattivo (fuori dall'erba alta)
static bool      s_wasInGrass = false;
static int       s_wildMonIndex = 0;

static const char* const s_wildMonNames[4] = {
    "BIDOOF",
    "SHINX",
    "STARLY",
    "BUDEW"
};

int encounterGetWildMonIndex(void) {
    return s_wildMonIndex;
}

static void updateBottomScreen(void) {
    consoleClear();
    if (s_state == GAME_STATE_ENCOUNTER) {
        printf("\x1b[11;6H*** WILD POKEMON ENCOUNTER! ***\n");
        printf("\x1b[13;8HA wild %s appeared!\n", s_wildMonNames[s_wildMonIndex]);
        printf("\x1b[16;10HPress (A) to return...\n");
    } else if (s_movesToEncounter >= 0) {
        printf("\x1b[14;10Hmove to encounter: %d\n", s_movesToEncounter);
    }
}

void encounterInit(void) {
    srand((unsigned int)osGetTime());
    s_state = GAME_STATE_WORLD;
    s_movesToEncounter = -1;
    s_wasInGrass = false;
    consoleClear();
}

GameState encounterGetState(void) {
    return s_state;
}

void encounterOnStep(int gx, int gy) {
    bool inGrass = dsMapIsWildGrass(gx, gy);

    if (inGrass) {
        if (!s_wasInGrass || s_movesToEncounter < 0) {
            // Primo passo nell'erba alta: estrae numero casuale da 4 a 10
            s_movesToEncounter = 4 + (rand() % 7);
            s_wasInGrass = true;
            updateBottomScreen();
        } else {
            // Cella successiva di erba alta: decrementa il contatore
            s_movesToEncounter--;
            if (s_movesToEncounter <= 0) {
                s_movesToEncounter = 0;
                s_wildMonIndex = rand() % 4;
                s_state = GAME_STATE_ENCOUNTER;
            }
            updateBottomScreen();
        }
    } else {
        // Fuori dall'erba alta: il contatore si azzera e scompare dallo schermo inferiore
        if (s_wasInGrass || s_movesToEncounter >= 0) {
            s_movesToEncounter = -1;
            s_wasInGrass = false;
            consoleClear();
        }
    }
}

void encounterDismiss(void) {
    if (s_state != GAME_STATE_ENCOUNTER) return;

    s_state = GAME_STATE_WORLD;
    // Trovandosi ancora sulla casella d'erba al termine dell'incontro,
    // genera subito un nuovo contatore (4-10) per il prossimo incontro
    s_movesToEncounter = 4 + (rand() % 7);
    s_wasInGrass = true;
    updateBottomScreen();
}

