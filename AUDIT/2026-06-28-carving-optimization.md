# AUDIT — Ottimizzazione del carving (sessione 2026-06-28)

Snapshot operativo per riprendere senza rifare l'analisi. Branch: `feature/faster-carving`.
HEAD a fine sessione: **`e274efd`**.

## Contesto (1 riga)
`autocam` simula l'asportazione di materiale (fresatura 3 assi) sottraendo un utensile voxelizzato da
un workpiece voxelizzato lungo un toolpath G-code; serve veloce perché girerà nel loop di un
algoritmo genetico, e l'output (gcode → geometria) è il dato di training per una LLM.
Vedi anche i memory `carving-perf`, `autocam-project-goal`.

## Risultato della sessione
`square_600`: **~1320 ms → ~26 ms di carving netto** (≈33–50×), geometria **bit-identica**.
Il carving è ora GPU-bound. Tutto committato.

| commit | contenuto |
|---|---|
| `c251ed1` | Fase 1: niente copia per-valore del workpiece per passo (→ riferimento), rimosso `glFinish` per-passo + barriera SSBO corretta, dispatch sul bounding-box del tool. Fix crash di teardown (modi facevano `return`, distruttori GL dopo `destroyGLContext` → SIGSEGV; ora scope prima del teardown). |
| `0985eab` | Fase 2: **sottrazione del tool spazzato** (1 dispatch per segmento lineare invece di ~L/step). |
| `bfbc734` | `copyBack`: **compattazione GPU** prima del readback (`compress_transitions.comp` + prefix-sum CPU) → readback 72→8 ms. |
| `883084e` | **bounding** del range di substep nello swept. |
| `e274efd` | timer `carving netto` (glFinish dopo il loop) vs `totale` (incl. copyBack). |

## Modello di costo finale (square_600, steady-state)
- enqueue CPU: ~0.4 ms (NON cresce coi segmenti: 9→202 segmenti resta ~0.4 ms → il "pompaggio"
  per-segmento NON è un collo di bottiglia; tool+workpiece sono precaricati una volta sola).
- carving netto (GPU): ~26 ms.
- copyBack: ~14 ms (readback 8 + compress 3 + counts/prefix 3).
- **Collo di bottiglia attuale: la merge per colonna sul buffer `obj1_flat` da ~128 MB
  (W×32 uint, stride 32) → memory-bound.**

## Esperimento RMQ — PROVATO e REVERTATO (non rifarlo)
Inviluppi direzionali del tool (sparse table RMQ) per rendere O(1) l'envelope per colonna nei tratti
assi-allineati a Z costante. Risultato: **solo ~25–30%**, geometria bit-identica. Motivo: il collo di
bottiglia non era l'envelope ma la **merge memory-bound** sul buffer 128 MB. Revertato pulito
(`git checkout HEAD -- include/boolOps.hpp shaders/subtract_swept.comp src/boolOps.cpp`).
**Conclusione: la micro-ottimizzazione del calcolo dà rendimenti decrescenti; il prossimo guadagno è
architetturale.**

## Leve future (nessuna fatta — pausa qui)
1. **Fitness su GPU (loop GA)** — valutare carved-vs-target sulla GPU (→ scalare) per saltare il
   `copyBack` (~14 ms) a ogni iterazione del GA. *Il vero acceleratore del GA.*
2. **Unità reali (mm) + export dataset** (coppie gcode→geometria) per la LLM. Oggi manca; è il ponte
   verso lo scopo. (Nota: la semantica Z/unità è ancora ambigua — vedi sotto.)
3. **Layout dati del workpiece** (il buffer 128 MB) — il vero limite memory-bound, ma restructure
   grosso e rischioso. Ultima risorsa.

## Nodi aperti / trabocchetti noti
- **Unità/Z ambigue**: si lavora in voxel, non mm. Mapping offset→translate: `translate =
  (w1/2+off.x, h1/2+off.y, z1/2−off.z)` (Z invertito). I G0 in aria (es. Z1200) mappano spesso fuori
  griglia e vengono comunque processati (no-op ma costano). Skippare l'aria richiede prima il fix unità.
- `MAX_TRANSITIONS = 32` (stride del buffer flat): cap non gestito in overflow.
- Envelope swept usa min(transizione più bassa)/max(più alta) per colonna → esatto per utensili
  convessi in Z; per non-convessi (es. step_mill) sovra-rimuove (come già il loop substep).
- `~VoxelViewer` chiama `glfwTerminate()` globale: attenzione all'ordine di teardown (gestito).

## File chiave
- `shaders/subtract_swept.comp` — shader fuso sweep+subtract (envelope per colonna + merge in-place).
- `shaders/subtract_flat.comp` — merge per-passo (modalità `--legacy`).
- `shaders/compress_transitions.comp` — compattazione per il copyBack.
- `src/boolOps.cpp` — `subtractGPU`, `subtractSwept`, `subtractGPU_copyback`, `subtractGPU_init`.
- `src/modes/simulate_mode.cpp` — loop a segmenti vs `--legacy`, timer netto/totale.
- `src/gcodeViewer.cpp` — `carveSwept`, `carve`, `copyBack`, `finishGPU`.

## Come build / run / misura
```
# build (g++ via .vscode/tasks.json):
#   release/autocam, debug/autocam   (vedi DOCS/COMMANDS.md)
./release/autocam simulate --gcode gcode/square_600.gcode --no-view            # swept (default)
./release/autocam simulate --gcode gcode/square_600.gcode --no-view --legacy   # stamping per-passo (A/B)
./release/autocam simulate ... --out test/out.bin                              # salva il risultato
./release/autocam view test/out.bin                                            # visualizza
```
La prima run di ogni processo è warm-up (compile shader): misurare le run successive.

## Validazione (invariante: volume solido residuo)
Confronto legacy vs swept e bit-identità dopo ogni ottimizzazione. Numeri di riferimento:
`square_600` → 281.165.072 voxel solidi; `star_pocket` → 462.722.118; swept/legacy ratio 0.9993.

Script (Python puro, niente numpy; header .bin = 72 byte params + 2×size_t + compressed + prefix):
```python
import struct, array
def solid(path):
    data=open(path,'rb').read(); off=72
    dataSize=struct.unpack_from('<q',data,off)[0]; off+=8
    prefixSize=struct.unpack_from('<q',data,off)[0]; off+=8
    comp=array.array('I'); comp.frombytes(data[off:off+dataSize]); off+=dataSize
    pref=array.array('I'); pref.frombytes(data[off:off+prefixSize])
    L=len(comp); n=len(pref); t=0
    for i in range(n):
        s=pref[i]; e=pref[i+1] if i+1<n else L; sg=-1; k=s
        while k<e: t+= -comp[k] if sg<0 else comp[k]; sg=-sg; k+=1
    return t   # somma alternata = volume solido (per colonna: sum(top_i - bottom_i))
```

## Documento tecnico
Descrizione dettagliata della strategia e delle ottimizzazioni: `DOCS/carving-simulation.md` (EN,
base per articolo).
