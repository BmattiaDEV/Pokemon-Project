import csv
import re
import sys
from pathlib import Path

# ---------- Impostazioni ----------
CARTELLA_SCRIPT = Path(__file__).resolve().parent

# Prende i CSV dalla sottocartella "exports" (o dalla cartella stessa se lo script è già dentro "exports")
if (CARTELLA_SCRIPT / "exports").is_dir():
    CARTELLA_INPUT = CARTELLA_SCRIPT / "exports"
else:
    CARTELLA_INPUT = CARTELLA_SCRIPT

# Cartella "progetto_3ds" sorella del genitore (../../progetto_3ds/source/map_data.h)
if (CARTELLA_SCRIPT.parent.parent / "progetto_3ds").is_dir():
    CARTELLA_PROGETTO = CARTELLA_SCRIPT.parent.parent / "progetto_3ds"
elif (CARTELLA_SCRIPT.parent / "progetto_3ds").is_dir():
    CARTELLA_PROGETTO = CARTELLA_SCRIPT.parent / "progetto_3ds"
else:
    CARTELLA_PROGETTO = CARTELLA_SCRIPT.parent.parent / "progetto_3ds"

OUTPUT = CARTELLA_PROGETTO / "source" / "map" / "map_data.h"

VUOTO = -1   # cella senza tile nei CSV di Tiled
MIN_LIVELLO = -32
MAX_LIVELLO = 32
MAX_LIVELLI_TOTALI = 65  # Supporta l'intero range da -32 a +32 (64 dislivelli)

# Riconosce sia "altezza_1.csv" che sub-layer alla stessa altezza come "altezza_1_alberi.csv", "altezza_1_path.csv"
PATTERN = re.compile(r"altezza_(-?\d+)(?:_([a-z0-9_]+))?$", re.IGNORECASE)


def nome_array_layer(n: int, suffisso: str) -> str:
    """Restituisce un identificatore C valido per il layer (es. mapAltezza1, mapAltezza1_alberi, mapAltezza_neg1)."""
    base = f"mapAltezza_neg{abs(n)}" if n < 0 else f"mapAltezza{n}"
    if suffisso:
        return f"{base}_{suffisso}"
    return base


def trova_layers():
    """
    Cerca i file altezza_N.csv e altezza_N_<suffisso>.csv in CARTELLA_INPUT.
    Restituisce una lista ordinata di tuple: (livello_int, suffisso_str, percorso_file).
    """
    if not CARTELLA_INPUT.is_dir():
        sys.exit(f"Errore: la cartella di input non esiste: {CARTELLA_INPUT}")

    layers = []
    visti = {}
    for file in sorted(CARTELLA_INPUT.glob("*.csv")):
        m = PATTERN.search(file.stem)
        if not m:
            continue
        n = int(m.group(1))
        suffisso = (m.group(2) or "").lower()
        if n < MIN_LIVELLO or n > MAX_LIVELLO:
            sys.exit(
                f"Errore: il livello {n} ({file.name}) è fuori dal range consentito "
                f"[{MIN_LIVELLO}, {MAX_LIVELLO}]."
            )
        chiave = (n, suffisso)
        if chiave in visti:
            sys.exit(
                f"Errore: due file per lo stesso layer altezza {n}"
                f"{'_' + suffisso if suffisso else ''}: "
                f"{visti[chiave].name} e {file.name}. Tieni solo uno."
            )
        visti[chiave] = file
        layers.append((n, suffisso, file))

    if not layers:
        sys.exit(f"Errore: nessun file altezza_N.csv trovato in {CARTELLA_INPUT}")

    livelli_distinti = sorted({n for n, _, _ in layers})
    if len(livelli_distinti) > MAX_LIVELLI_TOTALI:
        sys.exit(
            f"Errore: trovati {len(livelli_distinti)} livelli distinti, il massimo supportato è "
            f"{MAX_LIVELLI_TOTALI} ({MIN_LIVELLO} -> {MAX_LIVELLO})."
        )

    # I livelli d'altezza distinti devono essere consecutivi da min a max
    liv_min = livelli_distinti[0]
    liv_max = livelli_distinti[-1]
    attesi = set(range(liv_min, liv_max + 1))
    mancanti = attesi - set(livelli_distinti)
    if mancanti:
        sys.exit(f"Errore: mancano i livelli intermedi: {sorted(mancanti)}")

    # Ordina prima per altezza n, poi con il layer base (suffisso == "") prima dei sub-layer (es. "alberi")
    layers.sort(key=lambda item: (item[0], 0 if item[1] == "" else 1, item[1]))
    return layers


def leggi(percorso):
    righe = []
    with open(percorso, newline="", encoding="utf-8") as f:
        for riga in csv.reader(f):
            valori = [c.strip() for c in riga if c.strip() != ""]
            if valori:
                righe.append([int(v) for v in valori])

    if not righe:
        sys.exit(f"Errore: {percorso.name} è vuoto")

    larghezza = len(righe[0])
    for n, riga in enumerate(righe, start=1):
        if len(riga) != larghezza:
            sys.exit(
                f"Errore: {percorso.name}, riga {n} ha "
                f"{len(riga)} colonne invece di {larghezza}"
            )
    return righe


def tabella(nome, dati):
    corpo = ",\n".join(
        "    {" + ",".join(str(v) for v in riga) + "}" for riga in dati
    )
    return (
        f"static const int {nome}[MAP_GEN_H][MAP_GEN_W] __attribute__((unused)) = {{\n"
        f"{corpo}\n"
        f"}};\n"
    )


def main():
    layers = trova_layers()
    griglie = [(n, suffisso, file, leggi(file)) for n, suffisso, file in layers]

    _, _, primo_file, prima_griglia = griglie[0]
    h = len(prima_griglia)
    w = len(prima_griglia[0])
    for n, suffisso, file, g in griglie:
        if len(g) != h or len(g[0]) != w:
            sys.exit(
                f"Errore: {file.name} è {len(g[0])}x{len(g)} "
                f"ma {primo_file.name} è {w}x{h}"
            )

    livelli_distinti = sorted({n for n, _, _, _ in griglie})
    liv_min = livelli_distinti[0]
    liv_max = livelli_distinti[-1]
    num_livelli = len(livelli_distinti)
    num_layers = len(griglie)

    blocchi = [
        "#pragma once",
        "",
        "// Generato automaticamente da converti_mappa.py. Non modificare a mano.",
        "//",
        f"// Convenzione: {VUOTO} = cella vuota, ogni altro valore = ID tile valido.",
        f"// Range altezze supportato: [{MIN_LIVELLO}, {MAX_LIVELLO}].",
        "// Più sub-layer possono condividere la stessa altezza (es. altezza_1 + altezza_1_alberi).",
        "",
        f"#define MAP_ABS_MIN_LEVEL ({MIN_LIVELLO})",
        f"#define MAP_ABS_MAX_LEVEL ({MAX_LIVELLO})",
        f"#define MAP_GEN_W {w}",
        f"#define MAP_GEN_H {h}",
        f"#define MAP_GEN_MIN_LEVEL ({liv_min})",
        f"#define MAP_GEN_MAX_LEVEL ({liv_max})",
        f"#define MAP_GEN_NUM_LEVELS {num_livelli}",
        f"#define MAP_GEN_NUM_LAYERS {num_layers}",
        "",
    ]

    for n, suffisso, file, g in griglie:
        nome_c = nome_array_layer(n, suffisso)
        blocchi.append(f"// Tile del layer {file.name} (altezza {n}), {VUOTO} = vuoto")
        blocchi.append(tabella(nome_c, g))

    altezze_str = ", ".join(str(n) for n, _, _, _ in griglie)
    blocchi.append("// Altezza associata a ciascun layer in mapLayers[]")
    blocchi.append(
        f"static const int mapLayerHeights[MAP_GEN_NUM_LAYERS] __attribute__((unused)) = {{ {altezze_str} }};\n"
    )

    puntatori_flat = ",\n".join(
        f"    &{nome_array_layer(n, suffisso)}[0][0]" for n, suffisso, _, _ in griglie
    )
    blocchi.append("// Puntatori flat (row-major: layer[y * MAP_GEN_W + x]) a tutti i layer e sub-layer")
    blocchi.append(
        f"static const int* const mapLayers[MAP_GEN_NUM_LAYERS] __attribute__((unused)) = {{\n"
        f"{puntatori_flat}\n"
        f"}};\n"
    )

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join(blocchi), encoding="utf-8")
    elenco = ", ".join(file.stem for _, _, file, _ in griglie)
    print(f"Creato {OUTPUT} ({w}x{h}, {num_layers} layer: {elenco})")


if __name__ == "__main__":
    main()
