# autocam — Manuale dei comandi

`autocam` è un simulatore voxel di fresatura CNC a 3 assi. L'eseguibile (`autocam`, in `release/`
e `debug/`) seleziona la funzionalità tramite un **sotto-comando a runtime** — non servono più
ricompilazioni o `#define` per cambiare modalità o parametri.

```
autocam <comando> [opzioni]
```

> **Come invocarlo.** È installato un wrapper in `~/.local/bin/autocam` (sul PATH) che esegue la
> build di release dalla radice del repo: così puoi digitare `autocam <comando>` **da qualsiasi
> cartella**. Poiché il wrapper entra nella radice del repo, i path relativi agli asset
> (`gcode/`, `test/`, `shaders/`) si risolvono lì; per file fuori dal repo usa path **assoluti**.
> In alternativa puoi sempre eseguire direttamente `./release/autocam <comando>` (release) o
> `./debug/autocam <comando>` (debug) dalla radice del repo.
>
> **Build di debug col wrapper.** Di default il wrapper esegue la release. Per usare la build di
> debug anteponi `--debug` (o `-d`) come **primo** argomento — viene consumato, non passato al
> programma: `autocam --debug simulate --gcode gcode/square_600.gcode …`
> (`--release`/`-r` forza esplicitamente la release).

I valori di default delle opzioni sono definiti in un unico posto: `include/main_params.hpp`.

---

## Scale, unità e sistemi di coordinate

Il mondo di autocam è in **millimetri**. Ogni oggetto voxel è una griglia intera `[0, resolutionXYZ)`
ancorata in quel mondo. Storicamente convivevano tre spazi che venivano confusi ai bordi; oggi le
conversioni sono centralizzate in un'unica fonte di verità: `include/coordinateSystem.hpp`
(`CoordinateSystem`), costruita dai metadati del `.bin`.

**Campi canonici** (in `VoxelizationParams`, scritti dal voxelizer):
- `resolution` — **mm per voxel** (`voxelSizeMm`).
- `resolutionXYZ` — dimensioni della griglia in voxel.
- `center` — centro del bounding box, in **mm**.
- Dimensione fisica = `resolutionXYZ * resolution`. Origine (angolo del voxel 0,0,0) = `center - extent/2`.

**Campi derivati** (interni, non usarli come input): `scale = 1/max(sizeX, sizeY)` (solo XY!) e
`zSpan` (estensione Z normalizzata). Servono solo al pass di slicing del voxelizer e al box del
raymarch shader.

> **Nota storica (correzione).** Una vecchia nota affermava `scale = 1/max(X,Y,Z)` con il risultato
> in `[0,1]³`. È **sbagliato**: lo `scale` usa solo `max(X,Y)` (la Z è esclusa), e l'oggetto
> normalizzato è **centrato** in `[-0.5,0.5]² × [-zSpan/2, +zSpan/2]`. Esempio: un utensile 10×10×50
> ha `scale = 1/10 = 0.1`, non `1/50`.

**I tre spazi e le conversioni:**
1. **mm mondo** — l'unità canonica (G-code in `mm`, viewer, assi, camera).
2. **indice-voxel** `[0, resolutionXYZ)` — il dato realmente memorizzato (colonne sparse di
   transizioni Z); tutto il carving GPU vive qui. `mm → voxel` = `round((mm - origin)/voxelSizeMm)`.
3. **normalizzato** `[-0.5,0.5]² × [-zSpan/2, zSpan/2]` — dettaglio interno (slicing + box shader);
   la `renderModelMatrix()` lo mappa alla scatola mm (scala **per-asse**, così i footprint
   rettangolari non vengono distorti).

**Carving e risoluzione.** I kernel GPU sommano le transizioni dell'utensile direttamente negli indici
dello stock, quindi sono fisicamente corretti **solo se utensile e workpiece condividono la
`voxelSizeMm`** (stessa `--res`). In modalità `--gcode-units mm` questo è obbligatorio (altrimenti
errore); in `voxel` è solo un warning. Per cambiare utensile o stock, **ri-voxelizza alla stessa
`--res`** (gli artefatti `.bin` sono rigenerabili a piacere con `autocam voxelize`).

**Footprint rettangolari.** Il voxelizer produce voxel **cubici** anche per stock non quadrati (es.
200×100×50): la griglia viene riempita correttamente, senza margini vuoti né voxel non cubici.

**Formato `.bin`.** I file voxel sono versionati e auto-descrittivi (magic `AVOX`, versione 1). Un
vecchio `.bin` senza header viene **rifiutato** con un messaggio che invita a rigenerarlo — non esiste
un lettore legacy. Layout: `[AVOX][u32 version][u32 paramsBytes][VoxelizationParams][u64 dataBytes]
[u64 prefixBytes][transizioni][prefix-sum]` (vedi `include/voxelFile.hpp`).

---

## Comandi

### `voxelize` — voxelizza una mesh STL

Carica una mesh STL, la voxelizza sulla GPU e salva il risultato come oggetto voxel `.bin`.

```
autocam voxelize <input.stl> [--out <file.bin>] [--res <float>] [--mem-mb <int>]
```

| Opzione      | Default                         | Descrizione                                   |
|--------------|---------------------------------|-----------------------------------------------|
| `<input.stl>`| — (obbligatorio, posizionale)   | Mesh STL da voxelizzare.                       |
| `--out`      | `test/<nome-stl>.bin`           | File `.bin` di output.                          |
| `--res`      | `RESOLUTION` (0.1)              | Dimensione del voxel in unità oggetto.          |
| `--mem-mb`   | `DEFAULT_MEM_MB` (512)          | Budget di memoria GPU in MB.                    |

Esempi:
```
autocam voxelize models/cube100.stl
autocam voxelize models/hemispheric_mill_10.stl --out test/mill.bin --res 0.05
```

---

### `simulate` — carving lungo un toolpath G-code

Carica un toolpath G-code, un workpiece e un utensile (entrambi `.bin`), poi fa avanzare l'utensile
lungo il percorso sottraendolo dal workpiece a ogni passo. Opzionalmente salva il risultato e/o lo
visualizza.

```
autocam simulate --gcode <f.gcode> --workpiece <w.bin> --tool <t.bin>
                 [--out <r.bin>] [--gcode-units mm|voxel] [--work-origin x,y,z]
                 [--step <float>] [--perspective] [--no-view] [--verbose]
                 [--mesh] [--out-mesh <file.stl>] [--mesh-step <int>]
```

| Opzione         | Default                                  | Descrizione                                          |
|-----------------|------------------------------------------|------------------------------------------------------|
| `--gcode`       | `GCODE_PATH` (`gcode/square_600.gcode`)  | Toolpath G-code.                                     |
| `--workpiece`   | `DEFAULT_WORKPIECE_BIN`                   | Workpiece voxelizzato `.bin`.                        |
| `--tool`        | `DEFAULT_TOOL_BIN`                        | Utensile voxelizzato `.bin`.                         |
| `--out`         | (nessuno)                                | Se presente, salva il workpiece lavorato in `.bin`.  |
| `--gcode-units` | `voxel`                                  | Interpretazione delle coordinate G-code: `mm` (mondo, fisicamente corretto) o `voxel` (indici griglia stock, legacy). Vedi sotto. |
| `--work-origin` | `0,0,0`                                  | Offset mm dell'origine G-code dal **centro** dello stock (solo `mm`). |
| `--step`        | `2.0`                                     | Avanzamento per passo del jog `--legacy` (in unità coerenti con `--gcode-units`). |
| `--perspective` | (off → ortografica)                      | Usa proiezione prospettica invece dell'ortografica.  |
| `--no-view`     | (off → mostra il viewer)                 | Esegue headless, senza aprire finestre (batch).      |
| `--verbose`     | (off)                                     | Stampa ogni comando G-code interpretato.             |
| `--mesh`        | (off → raymarch)                         | Mostra il risultato come **mesh** (marching cubes) invece del raymarcher. |
| `--out-mesh`    | (nessuno)                                | Salva la mesh marching-cubes come STL binario (funziona anche con `--no-view`). |
| `--mesh-step`   | `1`                                      | Sottocampiona la mesh: un voxel ogni N (più alto = mesh più leggera/veloce). |

> **Unità G-code e risoluzione.** In modalità `mm` (canonica) le coordinate sono millimetri mondo e
> vengono convertite in voxel dello stock; questo richiede che **utensile e workpiece siano stati
> voxelizzati alla stessa `--res`** (stessa `voxelSizeMm`) — altrimenti `simulate` rifiuta con un
> errore che invita a ri-voxelizzare. In modalità `voxel` (default, legacy) 1 unità G-code = 1 voxel
> stock; un mismatch di risoluzione produce solo un warning.
>
> Il sample `gcode/square_600.gcode` è **legacy in unità-voxel** ed è co-tarato con
> `test/hemispheric_mill_10.bin` (voxelizzato a `--res 0.0390625`, diverso dallo stock a `0.1`):
> per questo la run di default stampa il warning di mismatch — è atteso. Per una pipeline in `mm`
> ri-voxelizza **entrambi** alla stessa `--res` e scrivi il G-code in mm con profondità Z adatte
> all'utensile reale. Vedi "[Scale, unità e sistemi di coordinate](#scale-unità-e-sistemi-di-coordinate)".

Esempi:
```
# carving con i default + visualizzazione (G-code in unità-voxel, legacy; stampa il warning di mismatch)
autocam simulate --gcode gcode/square_600.gcode \
                 --workpiece test/workpiece_100_100_50.bin \
                 --tool test/hemispheric_mill_10.bin

# headless: produce solo il risultato su file (utile per generare dataset)
autocam simulate --gcode gcode/pocket.gcode --out test/pocket_result.bin --no-view

# risultato come mesh marching-cubes (sottocampionata) invece del raymarcher
autocam simulate --gcode gcode/square_600.gcode --mesh --mesh-step 4

# headless: esporta la mesh del risultato come STL (per dataset di mesh)
autocam simulate --gcode gcode/square_600.gcode --no-view --out-mesh carved.stl --mesh-step 4
```

---

### `view` — visualizza un oggetto voxel `.bin`

Carica un oggetto voxel `.bin` e lo mostra con il viewer raymarching (o come mesh con `--mesh`).

```
autocam view <file.bin> [--ortho] [--mesh] [--out-mesh <file.stl>] [--mesh-step <int>]
```

| Opzione       | Default                       | Descrizione                                                   |
|---------------|-------------------------------|---------------------------------------------------------------|
| `<file.bin>`  | — (obbligatorio, posizionale) | Oggetto voxel da visualizzare.                                |
| `--ortho`     | (off → prospettica)           | Usa proiezione ortografica (solo raymarcher).                 |
| `--mesh`      | (off → raymarch)              | Mostra una **mesh** (marching cubes) invece del raymarcher.   |
| `--out-mesh`  | (nessuno)                     | Salva la mesh come STL binario (funziona anche con `--no-view`).|
| `--mesh-step` | `1`                           | Sottocampiona la mesh: un voxel ogni N.                       |

Esempi:
```
autocam view test/workpiece_100_100_50.bin --ortho
autocam view test/cube100.bin --mesh                 # mesh marching-cubes a piena risoluzione
autocam view test/cube100.bin --out-mesh cube.stl --no-view   # solo export STL, headless
```

> **Visualizzazione a mesh (`--mesh` / `--out-mesh`).** Estrae una mesh a triangoli dal volume voxel
> con marching cubes (consuma direttamente il formato a transizioni; mesh e STL sono in **mm** mondo,
> coerenti con `CoordinateSystem`). L'estrazione è **su CPU**: a piena risoluzione (`--mesh-step 1`) un
> pezzo grande (es. 1000×1000×500) richiede decine di secondi e produce milioni di triangoli — usa
> `--mesh-step N` per una mesh più leggera/veloce in interattivo. (Ottimizzazione GPU = lavoro futuro.)

---

### `help`

```
autocam help        # oppure: autocam --help, oppure nessun argomento
```

---

## Sintassi delle opzioni

- Opzioni con valore: `--key value` **oppure** `--key=value`.
- Flag (senza valore): `--ortho`, `--perspective`, `--no-view`, `--verbose`, `--help`.
- Argomenti posizionali: il path di input (`.stl` per `voxelize`, `.bin` per `view`).

## Build ed esecuzione

Vedi anche `COMMANDS.md` (scorciatoie dell'editor). In sintesi:

```
# Release (g++ diretto via tasks.json):  ./release/autocam <comando> [opzioni]
# Debug:                                  ./debug/autocam <comando> [opzioni]
# Wrapper sul PATH (da qualsiasi cartella): autocam <comando> [opzioni]
```

I path relativi (`gcode/`, `test/`, `models/`, `shaders/`) sono risolti rispetto alla
directory di lavoro corrente: eseguire dalla radice del repository (o dalla cartella che
contiene `shaders/`, `gcode/`, `test/`).

> Nota ambiente (Wayland): l'app richiede un contesto OpenGL 4.6. Su una sessione Wayland
> serve la build Wayland di GLFW (`libglfw3-wayland`) per ottenere l'accelerazione hardware;
> con la build solo-X11 il contesto ripiega sul software (max 4.5) e la finestra non si crea.
