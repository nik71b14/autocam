# Fast Voxel-Based Material-Removal Simulation for 3-Axis CNC Milling

**Status:** working technical note (basis for a scientific article).
**Scope:** the GPU material-removal ("carving") pipeline of `autocam`: representation, baseline
algorithm, the optimizations applied, their correctness argument, and measured results — including a
documented negative result.

---

## 1. Motivation

Toolpath/G-code data for CNC machining is scarce in public datasets because it is proprietary.
The long-term goal of this project is to **synthesize training data** for a learned toolpath
generator: given a workpiece and a tool, produce the G-code that yields a target finished part. A
prerequisite is a **forward model** — a fast, accurate simulator that, given a G-code toolpath, a
tool, and a stock, computes the resulting geometry. This forward model is the fitness signal inside
an optimization loop (e.g. a genetic algorithm), so it may be evaluated **millions of times**.
Therefore raw simulation speed, at fixed accuracy, is the central engineering requirement.

We restrict the present work to **3-axis milling** (tool translation only; no tool-axis rotation).

---

## 2. Volume representation: compressed Z-transition columns

Both the stock (workpiece) and the tool are voxelized into a **sparse, column-wise
Z-transition** representation rather than a dense 3D grid.

For a grid of resolution `W × H × D`, consider each `(x, y)` **column** of `D` voxels along Z.
A solid column is encoded as the **sorted list of Z coordinates at which occupancy flips**
(empty→solid or solid→empty). Because milling parts are largely solid blocks with a few surfaces,
each column typically has only **2–4 transitions** (e.g. a stock block column is just
`[bottom, top]`).

Two arrays encode the whole object (`struct VoxelObject`):

- `compressedData`: all transitions of all columns, concatenated, each column's list sorted ascending.
- `prefixSumData`: for each column `i = x + y·W`, the start offset of its transition list in
  `compressedData` (an exclusive prefix sum of the per-column transition counts).

Occupancy at `(x, y, z)` is recovered by counting transitions `< z`: **odd ⇒ solid**. Equivalently,
walking the sorted list toggles an `inside` flag at each transition.

**Properties exploited later.** (i) The format is compact (proportional to surface complexity, not
volume). (ii) A Boolean operation between two objects is a **per-column merge of two sorted
transition lists** — embarrassingly parallel across columns. (iii) The *lowest* and *highest*
transitions of a column are exactly its solid extent `[bottom, top]` (a single interval for tools
that are convex in Z).

Representative sizes used in the experiments: stock `1000×1000×500` (≈2·10⁶ transitions); tool
(`hemispheric_mill_10`) `256×256×1280`.

---

## 3. The subtraction operator (Boolean difference on the GPU)

Material removal of the tool from the stock is the set difference `stock \ tool`. It is realized as
a compute shader (`subtract_flat.comp`) that, **for each stock column**, merges the stock's
transition list with the tool's (translated to the stock frame) and emits the resulting transitions.

Two design points make the operator efficient and are reused throughout:

1. **Unpacked working buffer.** During a carving session the stock is held GPU-resident in a
   fixed-stride **flat** buffer `obj1_flat` of `W·H·MAX_TRANSITIONS` uints (`MAX_TRANSITIONS = 32`),
   plus a per-column count buffer `obj1_dataNum`. The subtraction writes results **in place** into
   the same column slots. This avoids any per-operation reallocation/compaction.

2. **In-place two-pointer merge with difference semantics.** Walking both sorted lists, the shader
   tracks `obj1On`/`obj2On` occupancy and emits a transition when the *result* occupancy
   (`obj1On AND NOT obj2On`) changes. This is the standard sweep for `A \ B` on 1D interval sets.

The tool is positioned by an integer offset; the stock-frame placement uses
`translate = (W/2 + off.x, H/2 + off.y, D/2 − off.z)` (note the inverted Z axis), and the tool's Z
transitions are shifted by `translate.z − D_tool/2`.

**Coordinate note.** The current pipeline operates in **voxel units**, not millimetres; mapping to
physical units is future work (Section 8).

---

## 4. Baseline and its bottlenecks

The naive simulator advances the tool along the toolpath in small fixed **jog steps** (e.g. 2 voxel
units) and **stamps the full tool** (one subtraction dispatch) at every step. For a toolpath of total
length `L` and step `s`, this is `≈ L/s` subtractions; a `600×600` square at `s = 2` produced ~2327
subtractions.

Profiling this baseline (programme `square_600`) revealed that the cost was dominated by **per-step
overheads**, not by the geometric work:

- **A per-step deep copy** of the entire stock `VoxelObject` (`VoxelObject obj1 = objects[0];`, ~12 MB)
  just to read three grid dimensions.
- **A per-step `glFinish()`** (full CPU↔GPU sync) after each dispatch, serializing the pipeline. The
  accompanying barrier was also the wrong type for a shader→shader RAW dependency
  (`GL_BUFFER_UPDATE_BARRIER_BIT` instead of `GL_SHADER_STORAGE_BARRIER_BIT`); the `glFinish` was
  masking it.
- **A whole-grid dispatch** (`⌈W/8⌉·⌈H/8⌉` workgroups ≈ 10⁶ threads) even though only the tool's
  footprint (≈256² columns) performs work; the rest early-return.
- **Lateral redundancy:** consecutive stamps re-process columns already carved by the previous step.

Carving-loop wall time for the baseline `square_600`: **~1232 ms**.

---

## 5. Optimizations

We apply four techniques. After each, geometry is validated bit-exactly (Section 6). Steady-state
timings (first run per process discarded as shader-compile warm-up) are summarized in Section 7.

### 5.1 Removing per-step CPU and synchronization overhead

- Replace the per-step stock copy with a **reference** (12 MB/step eliminated).
- Replace the per-step `glFinish()` with a single `GL_SHADER_STORAGE_BARRIER_BIT` between dispatches,
  allowing dispatches to **pipeline**; perform one `glFinish` only at the final read-back. (The
  RAW hazard on `obj1_flat` between consecutive subtractions is satisfied by the SSBO barrier.)
- **Restrict the dispatch to the tool bounding box** by mapping the local invocation through a base
  offset (`baseX/baseY` uniforms), launching `⌈footprint/8⌉` workgroups instead of `⌈grid/8⌉`.

Effect: the carving loop's CPU cost collapses from ~1232 ms to ~2.5 ms of dispatch enqueue; the work
becomes GPU-bound. (Removing `glFinish` does not by itself reduce wall time — the GPU work is the
same — but it exposes the true GPU cost and is a prerequisite for pipelining.)

### 5.2 Swept-volume subtraction (core contribution)

The key algorithmic change replaces **`L/s` stamps per linear move** with **one subtraction per
linear toolpath segment** of the **swept tool volume** (the Minkowski sum of the tool with the
segment, `tool ⊕ p₀p₁`).

**Correctness (order-independence).** Set difference distributes over unions and is
order-independent: `A \ (B₁ ∪ … ∪ Bₙ) = (((A \ B₁) \ B₂) … \ Bₙ)`. Hence subtracting the swept volume
once is **equivalent** to subtracting the tool at every sampled position along the segment. It is in
fact *more* faithful than discrete stamping, which leaves a spurious **scallop** between samples in
the feed direction; the swept formulation removes that artefact.

**On-the-fly swept column (no intermediate buffer).** We never materialize the swept tool. A single
fused compute shader (`subtract_swept.comp`), dispatched over the swept bounding box, computes for
each stock column `(gx, gy)` the **Z-envelope** of the tool over the segment and immediately
subtracts it:

```
for k = k0 .. k1:                         # sub-positions sampled at ~1 voxel spacing
    tr   = round( translateStart + (k/K)·translateDelta )   # tool centre at sub-step k
    (xl,yl) = tool-local column of (gx,gy) at tr
    if (xl,yl) inside tool and tool column non-empty:
        zb = lowestTransition(xl,yl)  + (tr.z − D_tool/2)
        zt = highestTransition(xl,yl) + (tr.z − D_tool/2)
        zbMin = min(zbMin, zb);  ztMax = max(ztMax, zt)
# swept column = single interval [zbMin, ztMax]; subtract from stock column (same merge as §3)
```

`K = max(|Δx|, |Δy|, |Δz|)` (Chebyshev length) guarantees ≤1 voxel motion per sub-step, so the
sampled centres hit **every integer position** the discrete stamper would, making the swept result
**bit-identical** to per-step stamping (modulo the intended scallop removal). The envelope uses the
column's lowest/highest transition, i.e. its bounding solid interval; this is exact for tools that
are **convex in Z** (ball, flat, conical, bull-nose), and degrades gracefully (slight over-removal)
for non-convex tools — identical behaviour to the discrete stamper.

**Generality.** The formulation handles all linear moves: axis-aligned, **diagonal**, and
**Z-ramps** (`Δz ≠ 0` shifts the per-sub-step envelope). A pure plunge (`Δx = Δy = 0`) degenerates to
the tool extruded in Z. Junctions between consecutive segments overlap and are idempotent.

The host iterates over consecutive toolpath points (`getToolpath()`); a `--legacy` flag retains the
per-step stamper for A/B comparison and validation.

Effect: `square_600` goes from **2327 dispatches to 9**; total simulate time ~700 ms → ~114 ms; the
geometric work per move drops by roughly two orders of magnitude.

### 5.3 Sub-step range bounding

In the swept shader, a column is covered by the tool only for a contiguous sub-range of sub-steps.
From the linear centre motion `tr ≈ start + k·Δ/K`, we solve for the `k`-interval `[k0, k1]` during
which the tool can cover `(gx, gy)` (with ±1 safety margins), and iterate only that range. The exact
in-bounds test inside the loop preserves correctness; the bound merely skips sub-steps that cannot
contribute. For an axis-aligned move the per-column iteration count drops from `K` (segment length)
to ≈ tool extent, with the saving growing for longer segments.

Effect: `square_600` net carving ~67 ms → ~40 ms; geometry bit-identical.

### 5.4 GPU-side compaction for result read-back

After carving, the result must be returned in the compact column format (for visualization, on-disk
export, or marching-cubes meshing). Reading back the **unpacked** flat buffer is expensive: for a
`1000×1000` grid it is `W·H·32·4 B ≈ 128 MB`, ~72 ms via `glGetBufferSubData` on the test iGPU, plus
a CPU re-compaction pass.

Instead we compact **on the GPU** before read-back:

1. Read back only the per-column counts (`obj1_dataNum`, ~4 MB).
2. Compute the exclusive prefix sum on the CPU (a few ms) — this is exactly the output
   `prefixSumData` and gives the total transition count.
3. Run `compress_transitions.comp`: each column copies its `count` transitions from the strided flat
   slot to the dense output at offset `prefixSum[column]`.
4. Read back only the dense `compressedData` (~8 MB).

Effect: read-back 72 ms → ~8 ms; the CPU re-compaction is eliminated. This reuses the existing
voxelizer compaction shader. (Note: a multi-level GPU prefix sum is unnecessary here — the CPU scan
of `W·H` counts is cheap; only the *compaction* needs the GPU to avoid the 128 MB transfer.)

### 5.5 Measurement methodology

Because per-step `glFinish` was removed, the carving loop only *enqueues* asynchronous dispatches; the
GPU work surfaces at the next sync. We therefore report two numbers from `simulate`: **`carving
netto`** (a `glFinish` placed immediately after the loop — the true GPU carving time; the sync is
free because read-back syncs anyway) and **`totale`** (including read-back/compaction). The first run
of each process is discarded as shader-compilation warm-up.

---

## 6. Correctness and validation

Every optimization is validated against a strong invariant: the **residual solid voxel volume** of
the carved stock, computed offline from the saved `.bin` as the per-column alternating sum of
transitions, `Σ_columns Σ_intervals (topᵢ − bottomᵢ)`.

- **Swept vs. legacy stamping.** On `square_600` the removed-volume ratio swept/legacy is **0.9993**
  (0.07 % difference) — the expected scallop-removal/discretization residue, confirming geometric
  equivalence.
- **Bit-exactness across §5.3–5.4.** After sub-step bounding and after GPU compaction, the residual
  solid is **identical to the byte**: `square_600` = 281 165 072 voxels; `star_pocket` (diagonal,
  exercises the substep fallback) = 462 722 118 voxels.

This volume invariant is cheap, deterministic, and independent of the rendering path, making it a
reliable regression check.

---

## 7. Results

`square_600` (a 600-unit square perimeter; 9 linear segments), Intel Lunar Lake integrated GPU,
steady state:

| Stage | Dispatches | Carving loop (wall) | Notes |
|---|---|---|---|
| Baseline (per-step stamp) | ~2327 | ~1232 ms | per-step copy + `glFinish`, whole-grid dispatch |
| §5.1 overhead removal | ~2327 | enqueue ~2.5 ms / total ~700 ms | GPU-bound; bottleneck = #dispatches |
| §5.2 swept | 9 | total ~114 ms | one subtraction per segment |
| §5.4 GPU compaction | 9 | total ~67 ms | read-back 72→8 ms |
| §5.3 substep bounding | 9 | net carving ~26 ms, total ~40 ms | |

Overall: **~1320 ms → ~40 ms total (~33×)**, net GPU carving ~26 ms, geometry bit-identical.

**Where the time now goes** (post-optimization): dispatch enqueue ~0.4 ms (flat in segment count:
9→202 segments stays ~0.4 ms — confirming the per-segment "push" to the GPU is *not* a bottleneck;
tool and stock are uploaded once); GPU carving ~26 ms; read-back/compaction ~14 ms. The carving cost
is dominated by the **per-column merge over the 128 MB unpacked stock buffer** (strided access),
i.e. it is **memory-bound**.

---

## 8. A negative result: directional-envelope acceleration (RMQ)

We attempted to remove the per-column sub-step loop of §5.2 by precomputing, per tool, **directional
Z-envelopes** as range-minimum/maximum sparse tables (RMQ): for an axis-aligned constant-Z move, the
swept columns of a given stock column form a contiguous span of one tool row/column, so the envelope
is a single `O(1)` range query instead of `O(sub-steps)`. Non-axis-aligned moves fell back to the
sub-step loop. The implementation was exact (bit-identical residual volume on `square_600` and
`star_pocket`).

**Outcome:** only **~25–30 %** improvement on axis-aligned-heavy programmes (`square_600`
~22→18 ms; `pocket` ~60→40 ms), at the cost of ~8 MB of per-tool tables, a build pass, and shader
branching. The reason is informative: eliminating the envelope computation **exposed** that the
dominant cost is not the envelope but the **memory-bound per-column merge over the 128 MB stock
buffer** (Section 7), plus the diagonal/in-air rapid moves that use the slower fallback. The change
was **reverted**.

The lesson for the article: once per-step overheads and algorithmic redundancy are removed,
the simulator is **bandwidth-bound on the working-set buffer**; further *compute* micro-optimization
yields diminishing returns. The remaining levers are architectural (data layout / working-set size)
or shift the bottleneck out of the inner simulation entirely (Section 9).

---

## 9. Limitations and future work

- **Physical units.** The pipeline runs in voxel units; a calibrated mapping to millimetres
  (resolution, feed, tool geometry) is required for physically meaningful data and for skipping
  rapid moves that travel entirely above the stock (currently processed as no-ops).
- **GPU-side fitness for the optimization loop.** In a genetic-algorithm loop the per-iteration cost
  is dominated by the read-back/compaction (~14 ms). Evaluating the objective (carved-vs-target) on
  the GPU and returning a scalar would remove the read-back from the inner loop entirely — the single
  largest expected speedup for the intended use case.
- **Working-set data layout.** The 128 MB unpacked stock buffer is the memory bottleneck. A more
  compact GPU-resident representation (smaller fixed stride, or operating directly on the compressed
  form) could lift the bandwidth ceiling, at the cost of a substantial and risky restructure.
- **Non-convex tools / transition cap.** The swept envelope uses each column's bounding interval
  (lowest/highest transition), exact for Z-convex tools; non-convex tools over-remove. The flat
  buffer stride `MAX_TRANSITIONS = 32` is not yet overflow-guarded.
- **Tool orientation.** 3-axis only; the tool is axis-fixed. Tilt/rotation (4–5 axis) is out of scope.

---

## 10. Implementation map

| Concern | Location |
|---|---|
| Subtraction operator (per-step) | `shaders/subtract_flat.comp` |
| Swept-volume subtraction (fused) | `shaders/subtract_swept.comp` |
| GPU compaction for read-back | `shaders/compress_transitions.comp` |
| Host: init / subtract / swept / copy-back | `src/boolOps.cpp` (`subtractGPU_init`, `subtractGPU`, `subtractSwept`, `subtractGPU_copyback`) |
| Carving driver, segment loop, `--legacy`, timing | `src/modes/simulate_mode.cpp` |
| Viewer glue, `carve`/`carveSwept`/`copyBack`/`finishGPU` | `src/gcodeViewer.cpp` |
| Voxel format | `include/boolOps.hpp` (`VoxelObject`), `include/voxelizer.hpp` (`VoxelizationParams`) |

Reproduce the headline measurement (discard the first, warm-up, run):

```
./release/autocam simulate --gcode gcode/square_600.gcode --no-view             # swept (default)
./release/autocam simulate --gcode gcode/square_600.gcode --no-view --legacy    # per-step baseline
```

Validate equivalence by adding `--out r.bin` and comparing residual solid volume (see
`AUDIT/2026-06-28-carving-optimization.md` for the script and reference numbers).
