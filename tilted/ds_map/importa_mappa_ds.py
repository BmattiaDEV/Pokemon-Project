#!/usr/bin/env python3
"""
Importa una mappa esportata da DS Map Studio (OBJ + PNG) nel progetto 3DS.

Uso:
    py importa_mappa_ds.py                    (legge la cartella "exportedmap" accanto allo script)
    py importa_mappa_ds.py percorso/cartella  (legge un'altra cartella di export)

Crea / aggiorna:
    progetto_3ds/source/map/ds_map_model.h   vertici per materiale + riferimenti alle texture
    progetto_3ds/data/ds_map/*.png           texture (il Makefile le converte in .t3x)

Il file .mtl non serve: il nome di ogni materiale (riga "usemtl") deve corrispondere
al nome di un PNG nella stessa cartella.
"""
import re
import shutil
import sys
from pathlib import Path

CARTELLA_SCRIPT = Path(__file__).resolve().parent
CARTELLA_PROGETTO = CARTELLA_SCRIPT.parent / "progetto_3ds"
OUTPUT_H = CARTELLA_PROGETTO / "source" / "map" / "ds_map_model.h"
CARTELLA_TEXTURE = CARTELLA_PROGETTO / "data" / "ds_map"


def nome_c(nome: str) -> str:
    """Identificatore C valido (stessa regola usata da bin2o per i nomi dei file)."""
    return re.sub(r"\W", "_", nome)


def leggi_obj(percorso: Path):
    """
    Legge l'OBJ e restituisce:
      - ordine:  lista dei materiali nell'ordine in cui compaiono (senza quelli vuoti)
      - gruppi:  materiale -> lista di vertici (x, y, z, u, v), già triangolati
    I poligoni con più di 3 angoli (quad, ecc.) vengono divisi in triangoli a ventaglio.
    """
    posizioni, uv = [], []
    gruppi = {}
    ordine = []
    corrente = None

    def angolo(token):
        c = token.split("/")
        x, y, z = posizioni[int(c[0]) - 1]
        u, v = uv[int(c[1]) - 1] if len(c) > 1 and c[1] else (0.0, 0.0)
        return (x, y, z, u, v)

    with open(percorso, encoding="utf-8", errors="replace") as f:
        for riga in f:
            p = riga.split()
            if not p:
                continue
            if p[0] == "v":
                posizioni.append(tuple(float(s) for s in p[1:4]))
            elif p[0] == "vt":
                uv.append(tuple(float(s) for s in p[1:3]))
            elif p[0] == "usemtl":
                corrente = p[1]
                if corrente not in gruppi:
                    gruppi[corrente] = []
            elif p[0] == "f":
                if corrente is None:
                    sys.exit("Errore: trovata una faccia prima di un 'usemtl' nell'OBJ.")
                angoli = [angolo(t) for t in p[1:]]
                for i in range(1, len(angoli) - 1):
                    gruppi[corrente] += [angoli[0], angoli[i], angoli[i + 1]]

    ordine = [m for m in gruppi if gruppi[m]]
    return ordine, gruppi


def scrivi_header(ordine, gruppi, percorso: Path):
    righe = [
        "#pragma once",
        "",
        "// Generato automaticamente da importa_mappa_ds.py. Non modificare a mano.",
        "// Vertici per materiale: x y z (coordinate OBJ, Z = altezza) e u v (texture).",
        "",
        '#include "ds_map.h"',
        "",
    ]
    for m in ordine:
        righe.append(f'#include "{nome_c(m)}_t3x.h"')
    righe.append("")

    for m in ordine:
        righe.append(f"// materiale: {m} ({len(gruppi[m])} vertici)")
        righe.append(f"static const float ds_{nome_c(m)}[] = {{")
        for x, y, z, u, v in gruppi[m]:
            righe.append(f"    {x!r}f, {y!r}f, {z!r}f, {u!r}f, {v!r}f,")
        righe.append("};")
        righe.append("")

    righe.append(f"#define DS_MAP_GROUP_COUNT {len(ordine)}")
    righe.append("")
    righe.append("static void dsMapFillGroups(DsMapGroupData g[DS_MAP_GROUP_COUNT]) {")
    for i, m in enumerate(ordine):
        n = nome_c(m)
        righe.append(f"    // {m}")
        righe.append(f"    g[{i}].verts = ds_{n};")
        righe.append(f"    g[{i}].vertexCount = {len(gruppi[m])};")
        righe.append(f"    g[{i}].tex = {n}_t3x;")
        righe.append(f"    g[{i}].texSize = {n}_t3x_size;")
    righe.append("}")

    percorso.parent.mkdir(parents=True, exist_ok=True)
    with open(percorso, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(righe) + "\n")


def main():
    cartella = Path(sys.argv[1]) if len(sys.argv) > 1 else CARTELLA_SCRIPT / "exportedmap"
    if not cartella.is_dir():
        sys.exit(f"Errore: cartella non trovata: {cartella}")

    obj = sorted(cartella.glob("*.obj"))
    if len(obj) != 1:
        sys.exit(f"Errore: attesi 1 file .obj in {cartella}, trovati {len(obj)}.")

    ordine, gruppi = leggi_obj(obj[0])
    if not ordine:
        sys.exit("Errore: l'OBJ non contiene facce.")

    mancanti = [m for m in ordine if not (cartella / f"{m}.png").is_file()]
    if mancanti:
        sys.exit("Errore: mancano i PNG per questi materiali: " + ", ".join(mancanti))

    nomi = [nome_c(m) for m in ordine]
    if len(set(nomi)) != len(nomi):
        sys.exit("Errore: due materiali hanno lo stesso nome dopo la sanificazione.")

    # Texture: ricopia solo quelle usate (le vecchie vengono rimosse)
    CARTELLA_TEXTURE.mkdir(parents=True, exist_ok=True)
    for vecchia in CARTELLA_TEXTURE.glob("*.png"):
        vecchia.unlink()
    for m in ordine:
        shutil.copyfile(cartella / f"{m}.png", CARTELLA_TEXTURE / f"{m}.png")

    scrivi_header(ordine, gruppi, OUTPUT_H)

    totale = sum(len(gruppi[m]) // 3 for m in ordine)
    print(f"Creato {OUTPUT_H.relative_to(CARTELLA_SCRIPT.parent)}")
    print(f"  {totale} triangoli in {len(ordine)} materiali, texture copiate in data/ds_map/")


if __name__ == "__main__":
    main()
