// ============================================================================
// FILE GENERATO AUTOMATICAMENTE DAL POKÉDEX TELEGRAM BOT CMS (3DS devkitARM)
// Non modificare manualmente: usa /nuovo, /modifica o /esporta sul Bot!
// ============================================================================
#pragma once

#include <stdint.h>
#include <stddef.h>

#define POKEDEX_TOTAL_SPECIES 1

// ID univoci delle specie nel Pokédex
typedef enum {
    POKEMON_NONE = 0,
    POKEMON_SPARVELOCE = 1,
    POKEMON_ID_MAX
} PokemonId;

// Tipi elementali
typedef enum {
    PKM_TYPE_NONE = 0,
    PKM_TYPE_NORMAL,
    PKM_TYPE_FIRE,
    PKM_TYPE_WATER,
    PKM_TYPE_GRASS,
    PKM_TYPE_ELECTRIC,
    PKM_TYPE_ICE,
    PKM_TYPE_FIGHTING,
    PKM_TYPE_POISON,
    PKM_TYPE_GROUND,
    PKM_TYPE_FLYING,
    PKM_TYPE_PSYCHIC,
    PKM_TYPE_BUG,
    PKM_TYPE_ROCK,
    PKM_TYPE_GHOST,
    PKM_TYPE_DRAGON,
    PKM_TYPE_DARK,
    PKM_TYPE_STEEL,
    PKM_TYPE_FAIRY
} PokemonType;

// Curve di crescita esperienza
typedef enum {
    GROWTH_FAST = 0,
    GROWTH_MEDIUM_FAST,
    GROWTH_MEDIUM_SLOW,
    GROWTH_SLOW,
    GROWTH_ERRATIC,
    GROWTH_FLUCTUATING
} PokemonGrowthRate;

// Gruppi Uova
typedef enum {
    EGG_GROUP_NONE = 0,
    EGG_GROUP_MONSTER,
    EGG_GROUP_WATER_1,
    EGG_GROUP_BUG,
    EGG_GROUP_FLYING,
    EGG_GROUP_FIELD,
    EGG_GROUP_FAIRY,
    EGG_GROUP_GRASS,
    EGG_GROUP_HUMAN_LIKE,
    EGG_GROUP_MINERAL,
    EGG_GROUP_AMORPHOUS,
    EGG_GROUP_DRAGON,
    EGG_GROUP_UNDISCOVERED
} PokemonEggGroup;

// Statistiche base della specie (1..255)
typedef struct {
    uint8_t hp;
    uint8_t attack;
    uint8_t defense;
    uint8_t sp_attack;
    uint8_t sp_defense;
    uint8_t speed;
} PokemonBaseStats;

// Definizione completa di una specie nel Pokédex 3DS
typedef struct {
    uint16_t          dex_id;
    const char*       name;
    const char*       description;
    PokemonType       type1;
    PokemonType       type2;
    PokemonGrowthRate growth_rate;
    PokemonEggGroup   egg_group;
    PokemonBaseStats  stats;
    const char*       sprite_folder;
    const char*       sprite_zip;
    const char*       sprite_front_file;
    const char*       sprite_back_file;
    const char*       sprite_icon_file;
    const char*       t3x_front_header;
    const char*       t3x_back_header;
    const char*       t3x_icon_header;
    uint16_t          sprite_width;
    uint16_t          sprite_height;
} PokemonSpecies;

// Tabella globale delle specie e funzioni di accesso
extern const PokemonSpecies g_PokedexData[POKEDEX_TOTAL_SPECIES];
const PokemonSpecies* pokedexGetById(uint16_t dex_id);
const PokemonSpecies* pokedexGetByIndex(size_t index);
