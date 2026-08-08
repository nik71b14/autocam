# autocam — Algorithm and Performance Summary

**Purpose of this document.** A self-contained technical summary of the `autocam` system —
its data structures, algorithms and measured performance — written as a starting point for a
scientific article. It covers the full pipeline (voxelization → material-removal simulation →
surface reconstruction → fitness scoring); for the carving stage there is a deeper companion
note, `DOCS/carving-simulation.md`, with the full correctness argument and a documented negative
result. Numbers below were measured on an Intel Lunar Lake integrated GPU (OpenGL 4.6 compute),
first (warm-up, shader-compile) run discarded.

---

## 1. Motivation

CNC toolpath / G-code data is scarce in public datasets because it is proprietary. The long-term
goal of the project is to **synthesize training data** for a learned toolpath generator: given a
stock block and a cutting tool, produce the G-code that machines a given target part. Learning
this map requires many `(G-code → resulting geometry)` pairs, which in turn requires a **fast,
accurate forward model**: a simulator that, given a toolpath, a tool and a stock, computes the
machined geometry.

That forward model sits inside an optimization loop (a genetic algorithm searches toolpaths and
scores each candidate against a target part), so it is evaluated potentially **millions of times**.
Consequently, **raw simulation speed at fixed accuracy is the central engineering requirement**,
and it drives every design decision below. The scope is deliberately restricted to **3-axis
milling** (tool translation only; no tool-axis tilt or rotation).

---

## 2. System overview

`autocam` is a single zero-dependency C++17 command-line tool built around GPU compute shaders. It
exposes four sub-commands that compose into the data-generation pipeline:

```
   STL mesh ──voxelize──►  .bin voxel object ──┐
                                               │
   G-code toolpath ─────────────────────────► simulate ──► carved .bin ──► mesh (STL) / view
        + tool .bin + stock .bin               │
                                               └────────► fitness  ──► scalar score + metrics
                                                            (vs target .bin, for the GA loop)
```

| Command    | Role in the pipeline                                                              |
|------------|-----------------------------------------------------------------------------------|
| `voxelize` | Convert an STL surface mesh into the sparse voxel representation (§4).             |
| `simulate` | Carve a stock with a tool along a G-code toolpath; the forward model (§5).         |
| `view`     | Raymarch or marching-cubes render a voxel object (§6) — inspection / STL export.   |
| `fitness`  | Headless: carve a candidate gene and score it against a target part (§7) — the GA. |

The two ingredients shared by every stage are the **compressed Z-transition column** representation
(§3) and a GPU-resident working buffer, which together make Boolean operations, meshing and scoring
all reduce to *embarrassingly parallel per-column work*.

---

## 3. Core data structure: compressed Z-transition columns

Both stock and tool are stored as a **sparse, column-wise Z-transition** representation rather than
a dense 3D occupancy grid.

For a grid `W × H × D`, consider each `(x, y)` **column** of `D` voxels along Z. A solid column is
encoded as the **sorted list of Z coordinates at which occupancy flips** (empty→solid or
solid→empty). Because machining parts are mostly solid with a few bounding surfaces, a column
typically has only **2–4 transitions** (a plain stock column is just `[bottom, top]`).

The whole object (`struct VoxelObject`) is two flat arrays:

- `compressedData` — all columns' transition lists, sorted ascending, concatenated.
- `prefixSumData` — for each column `i = x + y·W`, the start offset of its list in `compressedData`
  (an exclusive prefix sum of per-column transition counts). There is no trailing sentinel: the
  last column's end is the total transition count.

**Occupancy** at `(x, y, z)` is recovered by a **parity walk**: voxel `z` is solid iff an *odd*
number of transitions are `≤ z`. An unpaired final transition means the column is solid to the top.

Three properties are exploited throughout:

1. **Compactness** — storage is proportional to *surface complexity*, not volume. A `1000×1000×500`
   stock is ≈2·10⁶ transitions (a few MB), versus 500 M dense voxels.
2. **Boolean = per-column merge** — a set operation between two objects is a merge of two sorted
   transition lists per column, independent across the `W·H` columns → massively parallel on the GPU.
3. **Bounding interval is free** — the lowest/highest transition of a column are exactly its solid
   extent `[bottom, top]`, a single interval for tools that are convex in Z (ball, flat, conical,
   bull-nose). This is what makes the swept-tool envelope (§5.2) cheap.

The on-disk `.bin` format is self-describing (`voxelfile`): a magic/version/params header followed
by the two arrays. The persisted `VoxelizationParams` record carries the canonical scale fields
(`resolution` = mm/voxel, `resolutionXYZ`, `center` in mm) — see §8.

---

## 4. Voxelization (STL → voxel object)

`voxelize` converts a triangle mesh into the transition representation by **layered rasterization**:

1. **Resolution.** The grid is sized from the mesh bounding box and the requested voxel size
   (`--res`, mm/voxel): `resXYZ = ceil(size / res)`. A floor `MIN_RESOLUTION_XYZ = 32` guarantees at
   least 32 voxels on the smallest axis for tiny objects; when it triggers, the effective voxel size
   is reduced accordingly (a source of surprise for small tools — see §8).
2. **Slicing.** The mesh is rasterized into a stack of binary Z-slices held in a `GL_TEXTURE_2D_ARRAY`
   (`resX × resY × (slicesPerBlock+1)`) with a depth renderbuffer. To bound GPU memory, slices are
   produced **in Z-blocks** (`slicesPerBlock`, controlled by `--mem-mb`): fewer slices per block ⇒
   smaller texture, so arbitrarily tall grids fit a fixed budget. This same block-streaming idea
   reappears in the mesher (§6).
3. **Transition extraction.** A compute shader (`transitions_xyz2.comp`) compares each slice with the
   next and writes a transition wherever occupancy flips, building each column's sorted list, plus a
   per-column count and an overflow flag (`maxTransitionsPerZColumn = 32`).
4. **Compaction.** The per-column counts are prefix-summed and the transition lists compacted into
   the dense `compressedData` / `prefixSumData` pair, then saved.

The output is unit-agnostic voxels anchored in world millimetres via `CoordinateSystem` (§8).

---

## 5. Material-removal simulation (carving) — the core contribution

Machining removes the tool from the stock along the toolpath: the set difference `stock \ tool`,
accumulated over the whole path. This is the hot loop of the whole project. The full treatment
(correctness proof, cost model, negative result) is in `DOCS/carving-simulation.md`; this is the
summary.

### 5.1 The subtraction operator

`stock \ tool` is a compute shader that, **per stock column**, merges the stock's transition list
with the tool's (translated into the stock frame) using a two-pointer sweep with difference
semantics (emit a transition when `stockInside AND NOT toolInside` changes). Two design points make
it fast and are reused everywhere:

- **Unpacked GPU-resident working buffer.** During a carving session the stock lives in a
  fixed-stride *flat* buffer `obj1_flat` of `W·H·MAX_TRANSITIONS` uints (`MAX_TRANSITIONS = 32`) plus
  a per-column count buffer. Subtractions write **in place** into the same column slots — no
  per-operation reallocation or compaction.
- **The tool is uploaded once** and positioned by an integer offset; only the stock buffer mutates.

### 5.2 Swept-volume subtraction

The naive simulator advances the tool in small jog steps and **stamps the full tool** (one dispatch)
at each step — `≈ L/s` subtractions for a path of length `L` and step `s`. `autocam` instead does
**one subtraction per linear toolpath segment**, of the tool's **swept volume** (the Minkowski sum
`tool ⊕ p₀p₁`).

- **Correctness.** Set difference is order-independent and distributes over unions, so subtracting
  the swept volume once equals subtracting the tool at every sampled position along the segment. It
  is in fact *more* faithful than discrete stamping, which leaves a spurious scallop between samples.
- **On-the-fly, no intermediate buffer.** A single fused shader (`subtract_swept.comp`), dispatched
  over the swept bounding box, computes for each stock column the **Z-envelope of the tool over the
  segment** (min/max of the column's bounding interval across sub-positions sampled at ≤1-voxel
  spacing) and subtracts that single interval immediately. Sampling at Chebyshev spacing
  `K = max(|Δx|,|Δy|,|Δz|)` hits every integer position the stamper would, so the result is
  **bit-identical** to per-step stamping (modulo the intended scallop removal).
- **Generality.** Handles axis-aligned, diagonal and Z-ramp moves; a pure plunge degenerates to the
  tool extruded in Z; segment junctions overlap idempotently. Exact for Z-convex tools; non-convex
  tools slightly over-remove (same as the stamper).

A `--legacy` flag keeps the per-step stamper for A/B validation.

### 5.3 Sub-step range bounding

Within the swept shader, a given stock column is covered only for a contiguous sub-range of
sub-steps. Solving the linear centre motion for that `[k0,k1]` interval (with ±1 safety margins) lets
each column iterate only the sub-steps that can contribute, dropping the per-column work from *segment
length* to ≈*tool extent* for axis-aligned moves.

### 5.4 GPU-side compaction for read-back

Returning the result in the compact format (for meshing, export or the next GA step) must not read
back the 128 MB unpacked buffer. Instead the stock is compacted **on the GPU** first
(`compress_transitions.comp` + a cheap CPU prefix sum of the per-column counts), so only ~8 MB of
dense transitions cross the bus. Read-back drops from ~72 ms to ~8 ms.

### 5.5 Per-segment removal counter (for the fitness stage)

The swept shader optionally accumulates the voxels it removes into a per-segment SSBO via a guarded
`atomicAdd(removedCount[segmentIndex], oldSolid − newSolid)`, gated by a uniform. When off it is
**zero-cost and byte-identical** to normal carving (verified); when on it feeds the engagement /
safety metric of §7 without a second pass. By construction `Σ removed == stockSolid − carvedSolid`.

---

## 6. Surface reconstruction (GPU marching cubes)

For visualization and STL export the carved voxel object is meshed with an **edge-indexed marching
cubes entirely on the GPU** (`GpuMarchingCubes`): a 4-pass pipeline (classify edges → scan → emit
shared vertices; classify cells → scan → emit triangles) that reads occupancy directly from the
transition SSBOs and produces an interleaved position+normal VBO and index EBO. Normals are
gradient-based (smooth shading); when the mesh stays GPU-resident it is drawn with **no CPU
round-trip**. `readbackTo()` copies it out for STL export or CPU Taubin smoothing.

**Streaming for large grids (asymmetry with carving).** Marching-cubes working memory is proportional
to **volume**, so medium grids overflow (the stock at `--mesh-step 2`, grid `502×502×252`, would ask
≈2.2 GB). The mesher therefore **streams in Z-slabs**: the virtual-cell Z range is split into slabs
sized to fit the budget, each slab runs the unchanged 4-pass pipeline with a `zBase` uniform, and the
per-slab meshes are concatenated on the CPU. Occupancy reads are global, so boundary normals are
correct; boundary vertices are duplicated but with identical position and normal (deterministic,
local) ⇒ watertight, seamless, and the triangle count matches the single-shot mesh. The single-shot
path (`zBase = -1`) is bit-identical to before, so small objects are unaffected.

Note the contrast with carving, whose memory is proportional to **area × 32**, *independent of Z*
(the transitions are sparse per column). Taller/finer-in-Z parts are therefore already free to carve;
only enormous XY would require tiling the carve kernels — out of scope.

---

## 7. Fitness evaluation (the GA objective)

`fitness` is the headless scorer the genetic algorithm calls per candidate. It loads the voxelized
**target part**, carves the stock with the candidate gene (§5, with the removal counter on), then
computes a single scalar (**lower is better**) plus ~20 raw metrics. Four objectives:

- **Accuracy** — a per-column set-diff of carved vs target, aligned in world-mm by an integer voxel
  offset (requires equal voxel size and voxel-aligned grids; residual ≤ 0.25 voxel or it errors out):
  - **GOUGE** = target present ∧ material removed → **near-irreversible**, so it carries the dominant
    weight (`w_gouge = 1000`).
  - **EXCESS** = leftover material where the part is empty → recoverable, low weight (`w_excess = 1`).
- **Time** — estimated cycle time: feed moves + rapid air moves + a corner-deceleration term per
  vertex (angle-weighted). Subsumes path length and air travel.
- **Safety** — peak **engagement** = removed voxels per voxel of advance (≈ chip cross-section);
  engagement above a breakage threshold `e_break` is penalized (tool-breakage / wear proxy), using
  the per-segment counter of §5.5.
- **Smoothness** — the **frequency** (per mm of cutting) of abrupt direction changes while cutting;
  a boustrophedon/spiral scores low, a jittery path high. Frequency, not presence, so long clean
  passes are not punished.

Weights and thresholds live in a zero-dependency `key = value` file (`fitness.conf`). The evaluator
prints machine-parseable `key value` lines (`fitness`, `gouge_voxels`, `excess_voxels`, `coverage`,
`est_time_s`, `engage_max`, `turn_freq_per_mm`, …) for direct consumption by the GA driver.

---

## 8. Coordinate system and units

`CoordinateSystem` is the single source of truth for scale. **World space is millimetres**; a voxel
object is an integer grid `[0, resolutionXYZ)` with `voxelSizeMm = resolution` and origin
`centerMm − ½·extent`. It centralizes every conversion (normalized ↔ voxel-index ↔ mm) so the
formulas live in one place. Two objects may be carved against each other only if a voxel means the
**same physical size** (`sameVoxelSizeAs`, relative ε = 1e-3), because the GPU kernels add tool
transitions directly into stock indices.

Practical consequences the article should note:

- G-code may be interpreted as **voxel units or millimetres** (`--gcode-units`); the accompanying
  `tools/gen_gcode.py` can emit either.
- A **consistent-resolution set** (stock, tool and target voxelized at the same `--res`, G-code in
  mm) is required for a warning-free, correctly-aligned score. Because of the `MIN_RESOLUTION_XYZ = 32`
  floor, a 3 mm tool at `--res 0.1` is silently bumped to `0.09375` mm/voxel; using `--res ≤ ~0.09`
  (e.g. 0.05) makes the tool match the stock exactly.

---

## 9. Consolidated performance

Carving benchmark `square_600` (a 600-unit square perimeter, 9 linear segments), stock
`1000×1000×500`, steady state:

| Stage                         | Dispatches | Carving loop (wall)               | Note                                   |
|-------------------------------|-----------:|-----------------------------------|----------------------------------------|
| Baseline (per-step stamp)     |     ~2327  | ~1232 ms                          | per-step deep copy + `glFinish`, whole-grid dispatch |
| §5.1 overhead removal         |     ~2327  | enqueue ~2.5 ms / total ~700 ms   | GPU-bound; bottleneck = #dispatches    |
| §5.2 swept subtraction        |         9  | total ~114 ms                     | one subtraction per segment            |
| §5.4 GPU compaction           |         9  | total ~67 ms                      | read-back 72 → 8 ms                    |
| §5.3 sub-step bounding        |         9  | **net carving ~26 ms, total ~40 ms** | geometry bit-identical              |

**Overall: ~1320 ms → ~40 ms total (~33×)**, net GPU carving ~26 ms, geometry bit-identical.

**Beyond the swept baseline.** On top of that swept pipeline, two traffic-pruning optimizations — tube
pruning of the off-band columns a diagonal move over-dispatches, and a host-side skip of whole in-air
segments — speed the carve up where the bounding box over-covers the swept band. Measured across a
machining-workload suite (`tools/bench_matrix.sh`): ~**2.8×** on a 45° diagonal raster, ~**2.1×** on
air-heavy programs, ~1× on axis-aligned work (the AABB already equals the tube), with rapids becoming
essentially free. A sparse *tiled* backend was also built as a working-set study
(`AUTOCAM_CARVE_BACKEND=sparse`): it cuts the memory footprint ~**15×** for *localized* machining but
leaves the per-carve bandwidth (bounded by the swept bounding box, not the buffer size) essentially
unchanged (~1.1×, a locality effect). The full cross-workload matrix — separating the established
per-move swept gain (~12×) from this work's pruning — the negative results (RMQ, tiled dispatch) and
the bottleneck map are in `DOCS/carving-simulation.md`.

**Post-optimization cost model.** Dispatch enqueue ~0.4 ms and, crucially, **flat in segment count**
(9→202 segments stays ~0.4 ms — the per-segment push to the GPU is *not* a bottleneck; tool and stock
upload once); GPU carving ~26 ms; read-back/compaction ~14 ms. The carving cost is dominated by the
**per-column merge over the 128 MB unpacked stock buffer** (strided access) — i.e. the simulator is
now **memory-bandwidth-bound**, not compute-bound.

**A documented negative result.** Replacing the per-column sub-step loop with precomputed directional
Z-envelopes (range-min/max sparse tables, `O(1)` per axis-aligned column) was implemented exactly
(bit-identical residual volume) but gave only **~25–30 %**, because it optimized compute that was not
the bottleneck — the memory-bound merge is. It was reverted. Lesson for the paper: once per-step
overhead and algorithmic redundancy are gone, the remaining levers are **architectural** (working-set
data layout) or move the bottleneck out of the inner loop (GPU-side fitness).

**Fitness / consistent-set run.** On a self-consistent 0.05 mm set with mm G-code the scorer runs
warning-free and its metrics scale as expected (~8× the voxel counts of the 0.1 mm reference for the
×2 finer grid), with gouge slightly lower because the tool is a true 3.0 mm ball rather than the
0.09375-rounded 3.2 mm effective tool.

---

## 10. Correctness and validation

Every carving optimization is validated against a strong, rendering-independent invariant: the
**residual solid voxel volume** of the carved stock, computed offline from the saved `.bin` as the
per-column alternating sum of transitions `Σ_columns Σ_intervals (topᵢ − bottomᵢ)`.

- **Swept vs. legacy stamping**: on `square_600` the removed-volume ratio is **0.9993** (0.07 %, the
  expected scallop-removal residue) — confirming geometric equivalence.
- **Bit-exactness across §5.3–5.4**: residual solid identical to the byte —
  `square_600 = 281 165 072` voxels; `star_pocket = 462 722 118` voxels (diagonal path, exercises the
  sub-step fallback).
- **Counter self-consistency**: `Σ removed + carvedSolid == stockSolid` exactly.
- **Fitness symmetry checks**: a no-op gene → gouge 0, excess 0, coverage 1; holes-only vs full target
  and over-cut cases give equal-magnitude excess/gouge as expected.

A pure-Python (no numpy) reference script for the volume invariant, and the exact reference numbers,
are in `AUDIT/2026-06-28-carving-optimization.md`.

---

## 11. Limitations and future work

- **GPU-side fitness (the biggest expected win).** In the GA loop the per-iteration cost is dominated
  by the ~14 ms read-back/compaction. Evaluating carved-vs-target on the GPU and returning only a
  scalar would remove the read-back from the inner loop — plus a persistent "load once, evaluate many
  genes" server. This, not more carving micro-optimization, is the throughput lever.
- **Working-set data layout.** The 128 MB unpacked stock buffer is the memory ceiling. A more compact
  GPU-resident form (smaller stride, or operating directly on the compressed data) could raise the
  bandwidth bound — a large, risky restructure.
- **Non-convex tools / transition cap.** The swept envelope uses each column's bounding interval,
  exact only for Z-convex tools; the flat stride `MAX_TRANSITIONS = 32` is not yet overflow-guarded.
- **Tool orientation.** 3-axis only; tilt/rotation (4–5 axis) is out of scope.
- **Dataset export.** The `(G-code → geometry)` pair exporter that feeds the learned generator is the
  next integration step now that units (§8) are consistent.

---

## 12. Implementation map (for reproducibility)

| Concern                                | Location                                                             |
|----------------------------------------|---------------------------------------------------------------------|
| Voxel format (in-memory / on-disk)     | `include/boolOps.hpp` (`VoxelObject`), `include/voxelFile.hpp`       |
| Scale / units single source of truth   | `include/coordinateSystem.hpp`                                      |
| Voxelization (layered rasterization)   | `src/voxelizer.cpp`, `shaders/transitions_xyz2.comp`               |
| Subtraction operator (per-step)        | `shaders/subtract_flat.comp`                                        |
| Swept-volume subtraction (fused)       | `shaders/subtract_swept.comp`                                       |
| GPU compaction for read-back           | `shaders/compress_transitions.comp`                                |
| Carving host (init/subtract/swept/copy)| `src/boolOps.cpp`, `src/gcodeViewer.cpp`                            |
| Carving driver, `--legacy`, timing     | `src/modes/simulate_mode.cpp`                                       |
| GPU marching cubes + Z-slab streaming  | `src/gpuMarchingCubes.cpp`, `shaders/mc_*.comp`                     |
| Fitness evaluator                      | `src/modes/fitness_mode.cpp`, `include/fitnessConfig.hpp`, `fitness.conf` |
| CLI dispatch                           | `src/main.cpp`, `include/modes.hpp`                                 |

Reproduce the headline carving measurement (discard the first, warm-up, run):

```
./release/autocam simulate --gcode gcode/square_600.gcode --no-view            # swept (default)
./release/autocam simulate --gcode gcode/square_600.gcode --no-view --legacy   # per-step baseline
```

Companion documents: `DOCS/carving-simulation.md` (carving deep-dive, the article's core section),
`AUDIT/2026-06-28-carving-optimization.md` (session snapshot, validation script, reference numbers),
`DOCS/MANUAL.md` (full command reference, in Italian).
</content>
</invoke>
