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
| `--smooth`      | `0`                                      | Iterazioni di smoothing Taubin (CPU) sopra le normali lisce GPU (`0` = nessuna). |
| `--cpu`         | (off → GPU)                              | Forza il marching cubes su CPU (riferimento/fallback; il GPU è il default). |

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
autocam view <file.bin> [--ortho] [--mesh] [--out-mesh <file.stl>] [--mesh-step <int>] [--smooth <int>] [--cpu]
```

| Opzione       | Default                       | Descrizione                                                   |
|---------------|-------------------------------|---------------------------------------------------------------|
| `<file.bin>`  | — (obbligatorio, posizionale) | Oggetto voxel da visualizzare.                                |
| `--ortho`     | (off → prospettica)           | Usa proiezione ortografica (solo raymarcher).                 |
| `--mesh`      | (off → raymarch)              | Mostra una **mesh** (marching cubes su GPU) invece del raymarcher. |
| `--out-mesh`  | (nessuno)                     | Salva la mesh come STL binario (funziona anche con `--no-view`).|
| `--mesh-step` | `1`                           | Sottocampiona la mesh: un voxel ogni N.                       |
| `--smooth`    | `0`                           | Iterazioni di smoothing Taubin (CPU) sopra le normali lisce GPU. |
| `--cpu`       | (off → GPU)                   | Forza il marching cubes su CPU (riferimento/fallback).        |

Esempi:
```
autocam view test/workpiece_100_100_50.bin --ortho
autocam view test/cube100.bin --mesh                 # mesh GPU (normali lisce)
autocam view test/cyl_mill_12.bin --mesh --smooth 8  # + smoothing geometrico Taubin (CPU)
autocam view test/cube100.bin --out-mesh cube.stl --no-view --mesh-step 4   # export STL headless
```

> **Visualizzazione a mesh (`--mesh` / `--out-mesh`).** Estrae una mesh a triangoli dal volume voxel
> con marching cubes **su GPU** (di default; compute shader edge-indexed, vertici condivisi con
> **normali lisce da gradiente**). Mesh e STL sono in **mm** mondo, coerenti con `CoordinateSystem`.
> La vista interattiva di default non fa alcun readback (i buffer GPU vanno dritti al viewer).
> `--smooth N` (N>0) aggiunge N iterazioni di **Taubin** geometrico **su CPU** (richiede un readback);
> `--smooth 0` (default) usa solo le normali lisce, geometria esatta dei voxel. `--cpu` forza il
> marching cubes su CPU (path di riferimento, fa streaming e gestisce la piena risoluzione).
>
> **Memoria.** I buffer di lavoro GPU sono proporzionali al **volume** della griglia (virtuale, dopo
> `--mesh-step`). Quando superano un budget (~2 GiB), il marching cubes passa **automaticamente a
> elaborazione a blocchi (slab-Z)**: ogni slab usa memoria limitata e i risultati vengono concatenati,
> così anche griglie grandi (es. `--mesh-step 1` su 1000×1000×500) vengono renderizzate senza abortire.
> Lo slabbing limita il *working set* GPU, non la dimensione della mesh finale — per mesh più
> leggere/veloci in interattivo conviene comunque alzare `--mesh-step`. (Per `--cpu` il path resta CPU,
> a streaming di slice.)

---

### `fitness` — valuta la "bontà" di un toolpath (per l'algoritmo genetico)

Valutatore **headless** (nessuna finestra) pensato come funzione di fitness per un algoritmo
genetico che evolve i toolpath: carica workpiece, utensile, un **gene** G-code e la **geometria
target** (il pezzo finito, voxelizzato), esegue il carving sulla GPU e confronta il risultato con il
target, producendo delle **metriche grezze** e una **fitness scalare** (più **bassa** = migliore).

```
autocam fitness --gcode <f.gcode> --workpiece <w.bin> --tool <t.bin> --target <part.bin>
                [--config <fitness.conf>] [--gcode-units mm|voxel] [--work-origin x,y,z]
```

| Opzione         | Default            | Descrizione                                                          |
|-----------------|--------------------|---------------------------------------------------------------------|
| `--gcode`       | `GCODE_PATH`       | Il gene da valutare (programma G-code).                              |
| `--workpiece`   | `DEFAULT_WORKPIECE_BIN` | Grezzo di partenza voxelizzato `.bin`.                         |
| `--tool`        | `DEFAULT_TOOL_BIN` | Utensile voxelizzato `.bin`.                                        |
| `--target`      | — (**obbligatorio**) | **Pezzo finito** voxelizzato `.bin` da raggiungere.               |
| `--config`      | `fitness.conf`     | File dei pesi/soglie (vedi sotto). Se assente si usano i default.    |
| `--gcode-units` | `voxel`            | Interpretazione coordinate G-code: `mm` o `voxel` (come `simulate`). |
| `--work-origin` | `0,0,0`            | Offset mm dell'origine G-code dal centro dello stock (solo `mm`).    |

> **Allineamento del target.** Accuratezza = confronto voxel-per-voxel fra il pezzo carvato e il
> target, allineati nello **spazio mondo (mm)** tramite `CoordinateSystem`. Perciò il target **deve
> avere la stessa `voxelSizeMm` dello stock** (stessa `--res`) ed essere **allineato alla griglia**
> dello stock: l'evaluator calcola l'offset intero di voxel fra le due griglie e rifiuta con un
> errore se il residuo è > 0.25 voxel. Il caso più semplice e robusto è voxelizzare il target sullo
> **stesso volume/centro** dello stock (offset nullo). Un `.bin` prodotto carvando lo stesso stock
> (es. l'output di `simulate --out`) è già perfettamente allineato.

**Obiettivi e parametri** (in `fitness.conf`, formato `chiave = valore`, `#` per i commenti). Ogni
riga è un termine della fitness scalare; i pesi `w_*` scalano il relativo contributo.

| Parametro        | Default  | Obiettivo   | Significato                                                                      |
|------------------|----------|-------------|----------------------------------------------------------------------------------|
| `w_gouge`        | `1000.0` | Accuratezza | Penalità per **voxel gougiato** (target presente ma materiale asportato). È di fatto **irreversibile** → peso dominante (quasi un vincolo rigido). |
| `w_excess`       | `1.0`    | Accuratezza | Penalità per **voxel in eccesso** (materiale residuo dove il pezzo è vuoto). È **recuperabile** con altre passate → penalità lieve, da minimizzare. |
| `w_time`         | `1.0`    | Tempo       | Penalità per **secondo** di tempo-ciclo stimato. Il tempo **assorbe** lunghezza percorso, spostamenti in aria e (via decelerazione agli spigoli) i cambi di direzione bruschi. |
| `rapid_feed`     | `5000.0` | Tempo       | Velocità dei rapidi/spostamenti in aria, mm/min (modello di tempo).              |
| `default_feed`   | `400.0`  | Tempo       | Feed usato per una passata di taglio priva di `F`, mm/min.                       |
| `corner_decel_s` | `0.05`   | Tempo/finitura | Secondi aggiunti per un'inversione completa a 180° (scalati con l'angolo del cambio direzione). Modella la decelerazione dell'utensile a ogni spigolo. |
| `w_engage`       | `5.0`    | Sicurezza   | Penalità per unità di **engagement** oltre `e_break`, sommata sui segmenti.       |
| `e_break`        | `300.0`  | Sicurezza   | Soglia di engagement (voxel asportati per voxel di avanzamento ≈ sezione del truciolo): oltre, l'utensile **si romperebbe**. **Va calibrata** su utensile/unità. |
| `w_turn`         | `10.0`   | Finitura    | Penalità per **frequenza** di cambi di direzione bruschi (giri per mm di taglio). Essendo una *frequenza*, i percorsi lunghi e regolari (bustrofedo, spirale) **non** vengono penalizzati; solo i percorsi "sfarfallanti" con molti giri stretti sì. |
| `turn_angle_deg` | `60.0`   | Finitura    | Un cambio di direzione più netto di così conta come giro "brusco".               |
| `w_air`          | `0.0`    | Parsimonia  | Penalità extra per mm di percorso **in aria** (non tagliente); di norma 0 (già coperto dal tempo). |
| `w_moves`        | `0.0`    | Parsimonia  | Penalità per numero di movimenti (lunghezza del gene); 0 di default.             |

La fitness scalare (più bassa = migliore) è la somma pesata:

```
fitness = w_gouge·gouge + w_excess·excess          (accuratezza)
        + w_time·tempo_stimato                      (tempo)
        + w_engage·engagement_overload              (sicurezza)
        + w_turn·turn_freq                          (finitura)
        + w_air·air_len_mm + w_moves·n_moves        (parsimonia)
```

dove `engagement_overload = Σ_segmenti max(0, engagement − e_break)`. Oltre alla fitness, l'evaluator
stampa **tutte le metriche grezze** (una `chiave valore` per riga), così un GA può ricombinarle con
pesi propri o usare un fronte di Pareto:

`gouge_voxels`, `excess_voxels`, `gouge_mm3`, `excess_mm3`, `carved_voxels`, `target_voxels`,
`coverage` (frazione del target correttamente ottenuta = `(target−gouge)/target`), `path_len_mm`,
`cut_len_mm`, `air_len_mm` (segmenti che asportano 0 voxel), `est_time_s`, `move_time_s`,
`corner_time_s`, `engage_max`, `engage_overload`, `abrupt_turns`, `turn_freq_per_mm`, `n_moves`,
`voxel_mm`.

> **Come nasce l'engagement.** Durante il carving swept, il kernel GPU accumula (via `atomicAdd`, con
> costo nullo quando la valutazione non è attiva) i **voxel rimossi da ciascun segmento**; l'engagement
> del segmento è `voxel_rimossi / avanzamento_in_voxel`. Un segmento che asporta 0 voxel è "in aria"
> (rapido o taglio a vuoto). La somma dei rimossi coincide esattamente con `stock_solido − carved_solido`.

> **Calibrazione.** I pesi e le soglie di default sono un punto di partenza: `gouge` è reso dominante
> di proposito, ma `e_break` e `turn_angle_deg` dipendono da **utensile, stock e unità** e vanno tarati
> (es. lancia una passata "buona" nota e osserva `engage_max` per fissare `e_break`).

> **Set a risoluzione consistente + G-code in mm (consigliato).** Per una valutazione **fisicamente
> corretta** conviene voxelizzare **stock, utensile e target alla stessa `--res`** e scrivere il gene
> in **`mm`**. Così l'utensile taglia alla sua dimensione reale e non compare il warning
> «*tool voxel size differs from stock*» (che in modalità `voxel` segnala che l'utensile è timbrato
> 1 voxel-utensile = 1 voxel-stock, quindi con un raggio di taglio leggermente diverso da quello vero).
>
> Attenzione a una regola del voxelizer: ogni oggetto ha **almeno 32 voxel** sull'asse più corto
> (`MIN_RESOLUTION_XYZ`). Un utensile piccolo (es. la fresa da 3 mm) a `--res 0.1` darebbe 30 voxel
> < 32, quindi viene riscalato a 0.09375 mm/voxel — ecco perché `--res 0.1` **non** basta a farlo
> combaciare con lo stock. Serve una `--res` abbastanza fine da dare all'utensile ≥ 32 voxel sull'asse
> minore (per la fresa da 3 mm: `--res ≤ ~0.09`, es. **0.05**). A quel punto stock, utensile e target
> condividono la stessa `voxelSizeMm` e il confronto è esatto.
>
> Il generatore `tools/gen_gcode.py --units mm` produce il gene direttamente in millimetri (le stesse
> geometrie in unità-voxel moltiplicate ×0.1); l'output resta indipendente dalla risoluzione, quindi
> lo stesso file carva su uno stock a qualsiasi `--res` purché l'utensile ne condivida la voxelSize.

Esempi:
```
# valuta un gene contro il pezzo finito (target voxelizzato sulla griglia dello stock)
autocam fitness --gcode gene.gcode --workpiece test/workpiece_100_100_50.bin \
                --tool test/hemispheric_mill_3.bin --target part.bin

# un target "di prova" perfettamente allineato = l'output di un carving noto
autocam simulate --no-view --gcode ref.gcode --workpiece stock.bin --tool tool.bin --out part.bin

# --- set a risoluzione consistente (0.05) + G-code in mm: niente warning, taglio a misura reale ---
autocam voxelize models/workpiece_100_100_50.stl        --out test/workpiece_100_100_50_r05.bin       --res 0.05
autocam voxelize models/hemispheric_mill_3.stl          --out test/hemispheric_mill_3_r05.bin          --res 0.05
autocam voxelize models/workpiece_100_100_50_target.stl --out test/workpiece_100_100_50_target_r05.bin --res 0.05
python3 tools/gen_gcode.py --out gcode/complex_demo_mm.gcode --units mm
autocam fitness --gcode gcode/complex_demo_mm.gcode --gcode-units mm \
                --workpiece test/workpiece_100_100_50_r05.bin \
                --tool test/hemispheric_mill_3_r05.bin \
                --target test/workpiece_100_100_50_target_r05.bin
```
> Su griglie grandi (a `--res 0.05` lo stock è 2000×2000×1000) la voxelizzazione può richiedere più
> memoria GPU; se il renderer va in out-of-memory, abbassa il budget di slicing con `--mem-mb` (es.
> `--mem-mb 64`): l'output è identico, cambia solo il numero di blocchi Z elaborati.

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
