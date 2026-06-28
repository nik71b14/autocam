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

## Comandi

### `voxelize` — voxelizza una mesh STL

Carica una mesh STL, la voxelizza sulla GPU e salva il risultato come oggetto voxel `.bin`.

```
voxelize voxelize <input.stl> [--out <file.bin>] [--res <float>] [--mem-mb <int>]
```

| Opzione      | Default                         | Descrizione                                   |
|--------------|---------------------------------|-----------------------------------------------|
| `<input.stl>`| — (obbligatorio, posizionale)   | Mesh STL da voxelizzare.                       |
| `--out`      | `test/<nome-stl>.bin`           | File `.bin` di output.                          |
| `--res`      | `RESOLUTION` (0.1)              | Dimensione del voxel in unità oggetto.          |
| `--mem-mb`   | `DEFAULT_MEM_MB` (512)          | Budget di memoria GPU in MB.                    |

Esempi:
```
voxelize voxelize models/cube100.stl
voxelize voxelize models/hemispheric_mill_10.stl --out test/mill.bin --res 0.05
```

---

### `simulate` — carving lungo un toolpath G-code

Carica un toolpath G-code, un workpiece e un utensile (entrambi `.bin`), poi fa avanzare l'utensile
lungo il percorso sottraendolo dal workpiece a ogni passo. Opzionalmente salva il risultato e/o lo
visualizza.

```
voxelize simulate --gcode <f.gcode> --workpiece <w.bin> --tool <t.bin>
                  [--out <r.bin>] [--step <float>] [--perspective] [--no-view] [--verbose]
```

| Opzione        | Default                                  | Descrizione                                         |
|----------------|------------------------------------------|-----------------------------------------------------|
| `--gcode`      | `GCODE_PATH` (`gcode/square_600.gcode`)  | Toolpath G-code.                                    |
| `--workpiece`  | `DEFAULT_WORKPIECE_BIN`                   | Workpiece voxelizzato `.bin`.                       |
| `--tool`       | `DEFAULT_TOOL_BIN`                        | Utensile voxelizzato `.bin`.                        |
| `--out`        | (nessuno)                                | Se presente, salva il workpiece lavorato in `.bin`. |
| `--step`       | `2.0`                                     | Avanzamento per passo (unità voxel, non mm — TODO). |
| `--perspective`| (off → ortografica)                      | Usa proiezione prospettica invece dell'ortografica. |
| `--no-view`    | (off → mostra il viewer)                 | Esegue headless, senza aprire finestre (batch).     |
| `--verbose`    | (off)                                     | Stampa ogni comando G-code interpretato.            |

Esempi:
```
# carving con i default + visualizzazione
voxelize simulate --gcode gcode/square_600.gcode \
                  --workpiece test/workpiece_100_100_50.bin \
                  --tool test/hemispheric_mill_10.bin

# headless: produce solo il risultato su file (utile per generare dataset)
voxelize simulate --gcode gcode/pocket.gcode --out test/pocket_result.bin --no-view
```

---

### `view` — visualizza un oggetto voxel `.bin`

Carica un oggetto voxel `.bin` e lo mostra con il viewer raymarching.

```
voxelize view <file.bin> [--ortho]
```

| Opzione      | Default                       | Descrizione                              |
|--------------|-------------------------------|------------------------------------------|
| `<file.bin>` | — (obbligatorio, posizionale) | Oggetto voxel da visualizzare.            |
| `--ortho`    | (off → prospettica)           | Usa proiezione ortografica.               |

Esempio:
```
voxelize view test/workpiece_100_100_50.bin --ortho
```

---

### `help`

```
voxelize help        # oppure: voxelize --help, oppure nessun argomento
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
