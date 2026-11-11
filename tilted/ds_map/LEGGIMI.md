# Mappa importata da DS Map Studio

Questa cartella serve a portare nel progetto 3DS una mappa esportata da DS Map Studio
(modello OBJ + texture PNG).

## Caratteristiche

- Il gioco gira interamente sul modello 3D importato da DS Map Studio.
- Griglia logica di collisione 32x32:
  - Terreno calpestabile (erba, spiaggia, fiori, ombra alberi)
  - Collisioni con ostacoli (rocce, scogliere, recinti/cartelli)
  - Tronco degli alberi solido con chiome calpestabili (il giocatore ci passa sotto e viene coperto dallo z-buffer)
  - Scale calpestabili con salita/discesa continua di quota
- Personaggio con animazione di camminata (D-pad o Circle Pad).
- Telecamera 3D a inseguimento con zoom regolabile.

## Struttura

```
ds_map/
  exportedmap/          export da DS Map Studio (Plat_full.obj + PNG)
  importa_mappa_ds.py   script di importazione
  LEGGIMI.md            questo file

progetto_3ds/source/map/ds_map.c        logica mappa DS (collisioni, disegno, telecamera)
progetto_3ds/source/map/ds_map.h
progetto_3ds/source/map/ds_map_model.h  GENERATO dallo script: vertici e texture
progetto_3ds/data/ds_map/*.png          GENERATO dallo script: texture (il Makefile le converte in .t3x)
```

## Come aggiornare la mappa

1. In DS Map Studio esporta il modello in OBJ, con le texture PNG.
2. Metti i file in `ds_map/exportedmap/`, sostituendo quelli vecchi.
   Deve esserci un solo file `.obj`. Il nome di ogni materiale nell'OBJ deve
   corrispondere al nome di un PNG (es. `ngrass_lm2` -> `ngrass_lm2.png`).
3. Dalla cartella `ds_map/` esegui:
   ```
   py importa_mappa_ds.py
   ```
4. Dalla cartella `progetto_3ds/` ricompila (con `DEVKITARM` impostato):
   ```
   make clean
   make
   ```

## Controlli nel gioco

| Tasto | Azione |
|---|---|
| D-pad / Circle pad | Movimento del personaggio |
| L / R | Zoom avanti / indietro telecamera |
| X | Reset dello zoom telecamera |
| START | Esce dal gioco |

## Impostazioni (in `progetto_3ds/source/map/ds_map.c`)

- `DS_FIT_SIZE`: grandezza della mappa a schermo.
- `DS_CAM_DIST_START`, `DS_CAM_DIST_MIN`, `DS_CAM_DIST_MAX`: distanza della telecamera.
- `DS_ZOOM_STEP`: sensibilità dello zoom.

L'inclinazione della telecamera è `CAM_ANGLE_DEG` in `source/core/common.h`.

## Come vengono convertite le coordinate

Nell'OBJ, X e Y sono orizzontali e Z è l'altezza. Nel gioco (asse Y in alto):
X resta X, l'altezza (Z) diventa Y, e -Y diventa Z. Il modello viene centrato
e portato a terra automaticamente, e scalato in modo da entrare nella grandezza
di `DS_FIT_SIZE`.
