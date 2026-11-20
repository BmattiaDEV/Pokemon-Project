// ============================================================================
// FILE GENERATO AUTOMATICAMENTE DAL POKÉDEX TELEGRAM BOT CMS (3DS devkitARM)
// ============================================================================
#include "pokedex_data.h"

const PokemonSpecies g_PokedexData[POKEDEX_TOTAL_SPECIES] = {
    // #001 - Sparveloce (Cartella Sprite: sprites/uccello/)
    {
        .dex_id            = POKEMON_SPARVELOCE,
        .name              = "Sparveloce",
        .description       = "Un rapido Pokémon uccello che pattuglia le foreste dall'alto con vista acutissima.",
        .type1             = PKM_TYPE_NORMAL,
        .type2             = PKM_TYPE_FLYING,
        .growth_rate       = GROWTH_MEDIUM_SLOW,
        .egg_group         = EGG_GROUP_FLYING,
        .stats             = { .hp = 45, .attack = 60, .defense = 40, .sp_attack = 35, .sp_defense = 40, .speed = 75 },
        .sprite_folder     = "sprites/uccello",
        .sprite_zip        = "uccello.zip",
        .sprite_front_file = "uccello_front.png",
        .sprite_back_file  = "uccello_back.png",
        .sprite_icon_file  = "uccello_sprite.png",
        .t3x_front_header  = "uccello_front_t3x.h",
        .t3x_back_header   = "uccello_back_t3x.h",
        .t3x_icon_header   = "uccello_sprite_t3x.h",
        .sprite_width      = 1024,
        .sprite_height     = 2048
    }
};

const PokemonSpecies* pokedexGetById(uint16_t dex_id) {
    for (size_t i = 0; i < POKEDEX_TOTAL_SPECIES; i++) {
        if (g_PokedexData[i].dex_id == dex_id) {
            return &g_PokedexData[i];
        }
    }
    return NULL;
}

const PokemonSpecies* pokedexGetByIndex(size_t index) {
    if (index >= POKEDEX_TOTAL_SPECIES) return NULL;
    return &g_PokedexData[index];
}
