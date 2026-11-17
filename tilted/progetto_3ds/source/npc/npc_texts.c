#include "npc_texts.h"
#include <stddef.h>

typedef struct {
    const NpcDialoguePage* pages;
    int                    pageCount;
} NpcTextEntry;

// --- Testi degli NPC ---

// NPC 0: Esploratore Blu vicino al sentiero fiorito
static const NpcDialoguePage s_npcText0[] = {
    {
        "Hey! Look at these orange flowers",
        "swaying gently in the breeze."
    },
    {
        "I love walking along this clearing",
        "before heading into the forest."
    }
};

// NPC 1: Ranger Verde vicino all'erba alta
static const NpcDialoguePage s_npcText1[] = {
    {
        "Watch out! Wild Pokemon like to",
        "hide inside the tall grass!"
    },
    {
        "Step in carefully if you want to",
        "spot one jumping out at you."
    }
};

// NPC 2: Avventuriero Oro (pronto per futuri NPC)
static const NpcDialoguePage s_npcText2[] = {
    {
        "You can hear the ocean waves down",
        "by the cliffs to the southwest."
    }
};

// NPC 3: Saggio Viola (pronto per futuri NPC)
static const NpcDialoguePage s_npcText3[] = {
    {
        "Trees can hide what lies behind them.",
        "Step close to peer through the leaves."
    }
};

static const NpcTextEntry s_npcTexts[] = {
    { s_npcText0, (int)(sizeof(s_npcText0) / sizeof(s_npcText0[0])) },
    { s_npcText1, (int)(sizeof(s_npcText1) / sizeof(s_npcText1[0])) },
    { s_npcText2, (int)(sizeof(s_npcText2) / sizeof(s_npcText2[0])) },
    { s_npcText3, (int)(sizeof(s_npcText3) / sizeof(s_npcText3[0])) },
};

static const NpcDialoguePage s_fallbackText[] = {
    { "...", "" }
};

const NpcDialoguePage* npcTextsGet(int text_id, int* out_pageCount) {
    int count = (int)(sizeof(s_npcTexts) / sizeof(s_npcTexts[0]));
    if (text_id >= 0 && text_id < count && s_npcTexts[text_id].pageCount > 0) {
        if (out_pageCount) *out_pageCount = s_npcTexts[text_id].pageCount;
        return s_npcTexts[text_id].pages;
    }

    if (out_pageCount) *out_pageCount = 1;
    return s_fallbackText;
}
