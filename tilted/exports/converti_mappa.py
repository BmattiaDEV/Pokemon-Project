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

OUTPUT = CARTELLA_PROGETTO / "source" / "map_data.h"

VUOTO = -1   # cella senza tile nei CSV di Tiled
PATTERN = re.compile(r"altezza_(\d+)", re.IGNORECASE)


def trova_livelli():
    """Cerca i file altezza_N.csv in CARTELLA_INPUT e restituisce {N: percorso}."""
    if not CARTELLA_INPUT.is_dir():
        sys.exit(f"Errore: la cartella di input non esiste: {CARTELLA_INPUT}")

    livelli = {}
    for file in sorted(CARTELLA_INPUT.glob("*.csv")):
        m = PATTERN.search(file.stem)
        if not m:
            continue
        n = int(m.group(1))
        if n in livelli:
            sys.exit(
                f"Errore: due file per l'altezza {n}: "
                f"{livelli[n].name} e {file.name}. Tieni solo uno."
            )
        livelli[n] = file

    if not livelli:
        sys.exit(f"Errore: nessun file altezza_N.csv trovato in {CARTELLA_INPUT}")
    if 0 not in livelli:
        sys.exit("Errore: manca altezza_0.csv")

    # I livelli devono essere consecutivi: 0, 1, 2, ...
    attesi = set(range(max(livelli) + 1))
    mancanti = attesi - set(livelli)
    if mancanti:
        sys.exit(f"Errore: mancano i livelli: {sorted(mancanti)}")

    return dict(sorted(livelli.items()))


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
    livelli = trova_livelli()
    griglie = {n: leggi(percorso) for n, percorso in livelli.items()}

    h = len(griglie[0])
    w = len(griglie[0][0])
    for n, g in griglie.items():
        if len(g) != h or len(g[0]) != w:
            sys.exit(
                f"Errore: altezza_{n} è {len(g[0])}x{len(g)} "
                f"ma altezza_0 è {w}x{h}"
            )

    blocchi = [
        "#pragma once",
        "",
        "// Generato automaticamente da converti_mappa.py. Non modificare a mano.",
        "//",
        f"// Convenzione: {VUOTO} = cella vuota, ogni altro valore = ID tile valido.",
        "// I tile dello stesso livello sono raggruppati in mapAltezzaN.",
        "",
        f"#define MAP_GEN_W {w}",
        f"#define MAP_GEN_H {h}",
        f"#define MAP_GEN_MAX_LEVEL {max(livelli)}",
        "",
    ]

    for n in livelli:
        blocchi.append(f"// Tile del livello {n} (altezza_{n}.csv), {VUOTO} = vuoto")
        blocchi.append(tabella(f"mapAltezza{n}", griglie[n]))

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join(blocchi), encoding="utf-8")
    elenco = ", ".join(str(n) for n in livelli)
    print(f"Creato {OUTPUT} ({w}x{h}, livelli: {elenco})")


if __name__ == "__main__":
    main()