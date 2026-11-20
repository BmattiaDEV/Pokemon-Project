package pokedexbot;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

/**
 * FASE 6: Traduzione automatica dal database JSON a codice C per Nintendo 3DS (devkitARM).
 * Genera:
 * - pokedex_data.h (costanti, ID enum, strutture dati con l'intera cartella sprite: front, back, icon)
 * - pokedex_data.c (array completo dei Pokémon, statistiche, descrizioni e puntatori a tutti gli sprite della cartella)
 */
public class CSourceGenerator {

    public static class FileGenerati {
        private final Path headerPath;
        private final Path sourcePath;
        private final String headerContent;
        private final String sourceContent;

        public FileGenerati(Path headerPath, Path sourcePath, String headerContent, String sourceContent) {
            this.headerPath = headerPath;
            this.sourcePath = sourcePath;
            this.headerContent = headerContent;
            this.sourceContent = sourceContent;
        }

        public Path getHeaderPath() { return headerPath; }
        public Path getSourcePath() { return sourcePath; }
        public String getHeaderContent() { return headerContent; }
        public String getSourceContent() { return sourceContent; }
    }

    /**
     * Genera pokedex_data.h e pokedex_data.c a partire dalla lista di Pokémon del database JSON.
     */
    public static FileGenerati generaFileC(List<Pokemon> listaPokemon, Path outputDir) throws IOException {
        Files.createDirectories(outputDir);

        String headerContent = generaHeaderH(listaPokemon);
        String sourceContent = generaSourceC(listaPokemon);

        Path headerPath = outputDir.resolve("pokedex_data.h");
        Path sourcePath = outputDir.resolve("pokedex_data.c");

        Files.writeString(headerPath, headerContent, StandardCharsets.UTF_8);
        Files.writeString(sourcePath, sourceContent, StandardCharsets.UTF_8);

        return new FileGenerati(headerPath, sourcePath, headerContent, sourceContent);
    }

    private static String generaHeaderH(List<Pokemon> listaPokemon) {
        StringBuilder sb = new StringBuilder();
        sb.append("// ============================================================================\n");
        sb.append("// FILE GENERATO AUTOMATICAMENTE DAL POKÉDEX TELEGRAM BOT CMS (3DS devkitARM)\n");
        sb.append("// Non modificare manualmente: usa /nuovo, /modifica o /esporta sul Bot!\n");
        sb.append("// ============================================================================\n");
        sb.append("#pragma once\n\n");
        sb.append("#include <stdint.h>\n");
        sb.append("#include <stddef.h>\n\n");

        sb.append("#define POKEDEX_TOTAL_SPECIES ").append(listaPokemon.size()).append("\n\n");

        // Enum ID Pokémon
        sb.append("// ID univoci delle specie nel Pokédex\n");
        sb.append("typedef enum {\n");
        sb.append("    POKEMON_NONE = 0,\n");
        for (Pokemon p : listaPokemon) {
            sb.append(String.format("    POKEMON_%s = %d,\n", p.getCIdentifierUpper(), p.getId()));
        }
        sb.append("    POKEMON_ID_MAX\n");
        sb.append("} PokemonId;\n\n");

        // Enum Tipi Pokémon
        sb.append("// Tipi elementali\n");
        sb.append("typedef enum {\n");
        sb.append("    PKM_TYPE_NONE = 0,\n");
        sb.append("    PKM_TYPE_NORMAL,\n");
        sb.append("    PKM_TYPE_FIRE,\n");
        sb.append("    PKM_TYPE_WATER,\n");
        sb.append("    PKM_TYPE_GRASS,\n");
        sb.append("    PKM_TYPE_ELECTRIC,\n");
        sb.append("    PKM_TYPE_ICE,\n");
        sb.append("    PKM_TYPE_FIGHTING,\n");
        sb.append("    PKM_TYPE_POISON,\n");
        sb.append("    PKM_TYPE_GROUND,\n");
        sb.append("    PKM_TYPE_FLYING,\n");
        sb.append("    PKM_TYPE_PSYCHIC,\n");
        sb.append("    PKM_TYPE_BUG,\n");
        sb.append("    PKM_TYPE_ROCK,\n");
        sb.append("    PKM_TYPE_GHOST,\n");
        sb.append("    PKM_TYPE_DRAGON,\n");
        sb.append("    PKM_TYPE_DARK,\n");
        sb.append("    PKM_TYPE_STEEL,\n");
        sb.append("    PKM_TYPE_FAIRY\n");
        sb.append("} PokemonType;\n\n");

        // Enum Curva di crescita
        sb.append("// Curve di crescita esperienza\n");
        sb.append("typedef enum {\n");
        sb.append("    GROWTH_FAST = 0,\n");
        sb.append("    GROWTH_MEDIUM_FAST,\n");
        sb.append("    GROWTH_MEDIUM_SLOW,\n");
        sb.append("    GROWTH_SLOW,\n");
        sb.append("    GROWTH_ERRATIC,\n");
        sb.append("    GROWTH_FLUCTUATING\n");
        sb.append("} PokemonGrowthRate;\n\n");

        // Enum Gruppo Uova
        sb.append("// Gruppi Uova\n");
        sb.append("typedef enum {\n");
        sb.append("    EGG_GROUP_NONE = 0,\n");
        sb.append("    EGG_GROUP_MONSTER,\n");
        sb.append("    EGG_GROUP_WATER_1,\n");
        sb.append("    EGG_GROUP_BUG,\n");
        sb.append("    EGG_GROUP_FLYING,\n");
        sb.append("    EGG_GROUP_FIELD,\n");
        sb.append("    EGG_GROUP_FAIRY,\n");
        sb.append("    EGG_GROUP_GRASS,\n");
        sb.append("    EGG_GROUP_HUMAN_LIKE,\n");
        sb.append("    EGG_GROUP_MINERAL,\n");
        sb.append("    EGG_GROUP_AMORPHOUS,\n");
        sb.append("    EGG_GROUP_DRAGON,\n");
        sb.append("    EGG_GROUP_UNDISCOVERED\n");
        sb.append("} PokemonEggGroup;\n\n");

        // Struct Statistiche Base
        sb.append("// Statistiche base della specie (1..255)\n");
        sb.append("typedef struct {\n");
        sb.append("    uint8_t hp;\n");
        sb.append("    uint8_t attack;\n");
        sb.append("    uint8_t defense;\n");
        sb.append("    uint8_t sp_attack;\n");
        sb.append("    uint8_t sp_defense;\n");
        sb.append("    uint8_t speed;\n");
        sb.append("} PokemonBaseStats;\n\n");

        // Struct Specie Pokémon (con tutti i file della cartella Sprite: front, back, icon)
        sb.append("// Definizione completa di una specie nel Pokédex 3DS\n");
        sb.append("typedef struct {\n");
        sb.append("    uint16_t          dex_id;\n");
        sb.append("    const char*       name;\n");
        sb.append("    const char*       description;\n");
        sb.append("    PokemonType       type1;\n");
        sb.append("    PokemonType       type2;\n");
        sb.append("    PokemonGrowthRate growth_rate;\n");
        sb.append("    PokemonEggGroup   egg_group;\n");
        sb.append("    PokemonBaseStats  stats;\n");
        sb.append("    const char*       sprite_folder;\n");
        sb.append("    const char*       sprite_zip;\n");
        sb.append("    const char*       sprite_front_file;\n");
        sb.append("    const char*       sprite_back_file;\n");
        sb.append("    const char*       sprite_icon_file;\n");
        sb.append("    const char*       t3x_front_header;\n");
        sb.append("    const char*       t3x_back_header;\n");
        sb.append("    const char*       t3x_icon_header;\n");
        sb.append("    uint16_t          sprite_width;\n");
        sb.append("    uint16_t          sprite_height;\n");
        sb.append("} PokemonSpecies;\n\n");

        sb.append("// Tabella globale delle specie e funzioni di accesso\n");
        sb.append("extern const PokemonSpecies g_PokedexData[POKEDEX_TOTAL_SPECIES];\n");
        sb.append("const PokemonSpecies* pokedexGetById(uint16_t dex_id);\n");
        sb.append("const PokemonSpecies* pokedexGetByIndex(size_t index);\n");

        return sb.toString();
    }

    private static String generaSourceC(List<Pokemon> listaPokemon) {
        StringBuilder sb = new StringBuilder();
        sb.append("// ============================================================================\n");
        sb.append("// FILE GENERATO AUTOMATICAMENTE DAL POKÉDEX TELEGRAM BOT CMS (3DS devkitARM)\n");
        sb.append("// ============================================================================\n");
        sb.append("#include \"pokedex_data.h\"\n\n");

        sb.append("const PokemonSpecies g_PokedexData[POKEDEX_TOTAL_SPECIES] = {\n");
        for (int i = 0; i < listaPokemon.size(); i++) {
            Pokemon p = listaPokemon.get(i);
            String folder = (p.getSpriteFolderName() == null || p.getSpriteFolderName().isEmpty())
                    ? p.getSlugLower()
                    : p.getSpriteFolderName();
            String zipFile = (p.getSpriteZipFileName() == null || p.getSpriteZipFileName().isEmpty())
                    ? folder + ".zip"
                    : p.getSpriteZipFileName();

            String frontFile = (p.getSpriteFrontFile() == null || p.getSpriteFrontFile().isEmpty())
                    ? folder + "_front.png"
                    : p.getSpriteFrontFile();
            String backFile = (p.getSpriteBackFile() == null || p.getSpriteBackFile().isEmpty())
                    ? folder + "_back.png"
                    : p.getSpriteBackFile();
            String iconFile = (p.getSpriteIconFile() == null || p.getSpriteIconFile().isEmpty())
                    ? folder + "_sprite.png"
                    : p.getSpriteIconFile();

            String t3xFront = frontFile.replaceAll("\\.png$", "") + "_t3x.h";
            String t3xBack = backFile.replaceAll("\\.png$", "") + "_t3x.h";
            String t3xIcon = iconFile.replaceAll("\\.png$", "") + "_t3x.h";

            sb.append(String.format("    // #%03d - %s (Cartella Sprite: sprites/%s/)\n", p.getId(), p.getNome(), folder));
            sb.append("    {\n");
            sb.append(String.format("        .dex_id            = POKEMON_%s,\n", p.getCIdentifierUpper()));
            sb.append(String.format("        .name              = \"%s\",\n", escapeCString(p.getNome())));
            sb.append(String.format("        .description       = \"%s\",\n", escapeCString(p.getDescrizione())));
            sb.append(String.format("        .type1             = %s,\n", mapTipoToCEnum(p.getTipo1())));
            sb.append(String.format("        .type2             = %s,\n", mapTipoToCEnum(p.getTipo2())));
            sb.append(String.format("        .growth_rate       = %s,\n", mapCurvaToCEnum(p.getCurvaCrescita())));
            sb.append(String.format("        .egg_group         = %s,\n", mapGruppoUovaToCEnum(p.getGruppoUova())));
            sb.append(String.format("        .stats             = { .hp = %d, .attack = %d, .defense = %d, .sp_attack = %d, .sp_defense = %d, .speed = %d },\n",
                    p.getPs(), p.getAttacco(), p.getDifesa(), p.getAttaccoSpeciale(), p.getDifesaSpeciale(), p.getVelocita()));
            sb.append(String.format("        .sprite_folder     = \"sprites/%s\",\n", escapeCString(folder)));
            sb.append(String.format("        .sprite_zip        = \"%s\",\n", escapeCString(zipFile)));
            sb.append(String.format("        .sprite_front_file = \"%s\",\n", escapeCString(frontFile)));
            sb.append(String.format("        .sprite_back_file  = \"%s\",\n", escapeCString(backFile)));
            sb.append(String.format("        .sprite_icon_file  = \"%s\",\n", escapeCString(iconFile)));
            sb.append(String.format("        .t3x_front_header  = \"%s\",\n", escapeCString(t3xFront)));
            sb.append(String.format("        .t3x_back_header   = \"%s\",\n", escapeCString(t3xBack)));
            sb.append(String.format("        .t3x_icon_header   = \"%s\",\n", escapeCString(t3xIcon)));
            sb.append(String.format("        .sprite_width      = %d,\n", p.getSpriteWidth()));
            sb.append(String.format("        .sprite_height     = %d\n", p.getSpriteHeight()));
            sb.append("    }").append(i < listaPokemon.size() - 1 ? ",\n" : "\n");
        }
        sb.append("};\n\n");

        sb.append("const PokemonSpecies* pokedexGetById(uint16_t dex_id) {\n");
        sb.append("    for (size_t i = 0; i < POKEDEX_TOTAL_SPECIES; i++) {\n");
        sb.append("        if (g_PokedexData[i].dex_id == dex_id) {\n");
        sb.append("            return &g_PokedexData[i];\n");
        sb.append("        }\n");
        sb.append("    }\n");
        sb.append("    return NULL;\n");
        sb.append("}\n\n");

        sb.append("const PokemonSpecies* pokedexGetByIndex(size_t index) {\n");
        sb.append("    if (index >= POKEDEX_TOTAL_SPECIES) return NULL;\n");
        sb.append("    return &g_PokedexData[index];\n");
        sb.append("}\n");

        return sb.toString();
    }

    private static String escapeCString(String input) {
        if (input == null) return "";
        return input
                .replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\r", "")
                .replace("\n", " ");
    }

    private static String mapTipoToCEnum(String tipo) {
        if (tipo == null) return "PKM_TYPE_NONE";
        switch (tipo.trim().toLowerCase()) {
            case "normale":    return "PKM_TYPE_NORMAL";
            case "fuoco":      return "PKM_TYPE_FIRE";
            case "acqua":      return "PKM_TYPE_WATER";
            case "erba":       return "PKM_TYPE_GRASS";
            case "elettro":    return "PKM_TYPE_ELECTRIC";
            case "ghiaccio":   return "PKM_TYPE_ICE";
            case "lotta":      return "PKM_TYPE_FIGHTING";
            case "veleno":     return "PKM_TYPE_POISON";
            case "terra":      return "PKM_TYPE_GROUND";
            case "volante":    return "PKM_TYPE_FLYING";
            case "psico":      return "PKM_TYPE_PSYCHIC";
            case "coleottero": return "PKM_TYPE_BUG";
            case "roccia":     return "PKM_TYPE_ROCK";
            case "spettro":    return "PKM_TYPE_GHOST";
            case "drago":      return "PKM_TYPE_DRAGON";
            case "buio":       return "PKM_TYPE_DARK";
            case "acciaio":    return "PKM_TYPE_STEEL";
            case "folletto":   return "PKM_TYPE_FAIRY";
            default:           return "PKM_TYPE_NONE";
        }
    }

    private static String mapCurvaToCEnum(String curva) {
        if (curva == null) return "GROWTH_MEDIUM_FAST";
        switch (curva.trim().toLowerCase()) {
            case "rapida":       return "GROWTH_FAST";
            case "media-rapida": return "GROWTH_MEDIUM_FAST";
            case "media-lenta":  return "GROWTH_MEDIUM_SLOW";
            case "lenta":        return "GROWTH_SLOW";
            case "erratica":     return "GROWTH_ERRATIC";
            case "fluttuante":   return "GROWTH_FLUCTUATING";
            default:             return "GROWTH_MEDIUM_FAST";
        }
    }

    private static String mapGruppoUovaToCEnum(String gruppo) {
        if (gruppo == null) return "EGG_GROUP_FIELD";
        switch (gruppo.trim().toLowerCase()) {
            case "mostro":      return "EGG_GROUP_MONSTER";
            case "acqua 1":     return "EGG_GROUP_WATER_1";
            case "coleottero":  return "EGG_GROUP_BUG";
            case "volante":     return "EGG_GROUP_FLYING";
            case "campo":       return "EGG_GROUP_FIELD";
            case "magico":      return "EGG_GROUP_FAIRY";
            case "erba":        return "EGG_GROUP_GRASS";
            case "umanoide":    return "EGG_GROUP_HUMAN_LIKE";
            case "minerale":    return "EGG_GROUP_MINERAL";
            case "amorfo":      return "EGG_GROUP_AMORPHOUS";
            case "drago":       return "EGG_GROUP_DRAGON";
            case "sconosciuto": return "EGG_GROUP_UNDISCOVERED";
            default:            return "EGG_GROUP_FIELD";
        }
    }
}
