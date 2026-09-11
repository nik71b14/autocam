# Development plan — Sparse tiled carving backend (dual-backend, A/B-switchable)

## Context

Every optimization experiment on the carver (RMQ, oriented tiling, tube pruning, in-air skip) has
pointed at the same wall: **memory-write bandwidth on the 128 MB unpacked flat buffer**
(`obj1_flat`, `W·H·MAX_TRANSITIONS·4 B`, stride 32). That buffer is ~16× padding — real data is only
~2·10⁶ transitions (~8 MB). Optimizations that reduced *compute* or *launched threads* hit diminishing
or negative returns; the one that reduced *bytes written* (tube pruning) won. The next conceptual step
is therefore to **shrink the working set itself**: carve on a *sparse tiled* representation that keeps
only the transitions in the tiles a segment actually touches (idea (c) in the optimization survey).

**Hard requirement from the user:** the current flat-buffer algorithm must stay fully working **in
parallel**, so the two can be compared **at runtime without git**.

## Goal

- A second carving backend, `sparse`, selectable at runtime via **`AUTOCAM_CARVE_BACKEND=flat|sparse`**
  (default `flat`), both compiled in — same pattern as `AUTOCAM_SWEPT_SKIP`.
- **Byte-identical output** to the flat backend (bit-exact gate at every stage).
- Reduced per-segment working set → target several × on carving-bound programs (unlike the
  compute/launch levers, this has a high ceiling because it attacks the proven bottleneck).

## Is the "parallel" requirement possible? Yes.

Both backends implement one interface and are chosen by an env var; nothing about the flat path
changes, so the committed optimizations (swept subtraction, tube pruning, air-skip) remain the
baseline. A/B is `AUTOCAM_CARVE_BACKEND=flat` vs `=sparse` on the same binary.

## Dual-backend architecture

Introduce `include/carveBackend.hpp` with an abstract interface holding exactly what the driver needs:

```
struct ICarveBackend {
  virtual void  initStock(const VoxelObject& stock) = 0;   // make the stock resident in this layout
  virtual void  uploadTool(const VoxelObject& tool) = 0;    // shared tool (compressed)
  virtual bool  carveSwept(ivec3 startOffset, ivec3 displacement, int segmentIndex) = 0;
  virtual void  finish() = 0;                               // glFinish/barrier
  virtual VoxelObject readResult() = 0;                     // assemble compact (compressed,prefix)
  // optional removed-tracking hooks for the fitness evaluator
};
```

- `FlatBackend` — a thin wrapper over the **current** `BoolOps` flat path
  (`subtractGPU_init` / `subtractSwept` / `subtractGPU_copyback`). No behaviour change.
- `SparseTileBackend` — the new implementation below.
- `GcodeViewer` picks the backend at construction from `getenv("AUTOCAM_CARVE_BACKEND")` (default
  flat) and forwards `carveSwept`/`finish`/`readResult`. (Pragmatic alternative if the refactor is
  too invasive: a `mode` switch inside `BoolOps`; the interface is cleaner and preferred.)

## Sparse tiled representation

Partition the `W×H` column grid into square **tiles** of `TS×TS` columns (start with `TS = 32`). A
**tile directory** (`⌈W/TS⌉×⌈H/TS⌉` entries) maps each tile to a state:

- **UNIFORM_SOLID** — every column is `[0, z1]` (full solid to top). O(1). *This is the initial state
  of the entire stock*; the untouched bulk stays here forever.
- **DETAILED** — per-column transition lists stored in a small **per-tile block** taken from a pool.
- (later) **UNIFORM_EMPTY** — fully carved through; O(1), block freed.

**DETAILED tile block:** a small fixed-stride sub-buffer `TS·TS·K` uints (`K` small, e.g. 8) + per-column
counts, with an overflow path for the rare column exceeding `K`. Within a materialized tile we keep
the GPU-simple fixed-stride merge, but `K ≪ 32` **and** only materialized tiles cost memory.

**Memory model:** total ≈ (#DETAILED tiles) × `TS·TS·K·4 B`. A typical toolpath materializes only the
tiles near the cut → a small fraction of 128 MB; the untouched bulk is O(directory).

**Allocation:** a pool of fixed-size tile-blocks + a free list; a tile is materialized on first touch
by grabbing a block (atomic bump / free-list). `K` chosen so almost all columns fit; overflow → re-stride
the tile to a larger-`K` block (rare) or a per-column spill list.

## Carve flow (sparse backend, per segment)

1. Host computes the swept bbox (reuse existing math) → the small rectangle of **tiles** it overlaps.
2. **Materialize** any UNIFORM tile in that set: grab a block, fill its columns with `[0, z1]`
   (a cheap fill kernel).
3. Dispatch the swept-subtract kernel **only over the overlapped tiles' columns**, reading/writing
   their small blocks (not a global 128 MB buffer). The per-column envelope+merge is the **same** as
   today; only the indexing changes (tile directory → block offset → column slot, `K`-stride).
4. (later) Tiles that become fully empty → freed back to the pool.

Working set per segment = the overlapped tiles' blocks (small, real), not 128 MB.

## Final assembly (`readResult`)

Walk the directory in row-major column order: UNIFORM tiles expand trivially (`[0,z1]` per column),
DETAILED tiles emit their compressed columns; prefix-sum the per-column counts to build the global
`(compressedData, prefixSumData)` — exactly the current copyBack output → **cmp bit-exact vs flat**.

## Shader work

- `shaders/subtract_swept_tiled.comp` — the tiled variant: takes a tile-directory SSBO + a tile-pool
  SSBO, maps `(gx,gy) → tile → block offset → column slot`, same envelope/merge/writeback with `K`-stride
  (and the same tube-pruning early-out gated by `enableSkip`).
- `shaders/materialize_tile.comp` — fill a freshly-allocated tile block with `[0, z1]`.
- Host owns directory/pool allocation; kernels just read offsets.

## Staging (each stage gated bit-exact)

- **Stage 0 — scaffolding + toggle.** `ICarveBackend`; `FlatBackend` wraps the current path; wire
  `AUTOCAM_CARVE_BACKEND`; extend `bench_swept.sh` with a backend axis. *Gate: flat output unchanged;
  toggle works.*
- **Stage 1 — sparse, correctness-first (no perf).** Tiled structure with **all tiles DETAILED**
  (materialize everything), per-tile merge, `readResult`. Ignore UNIFORM. *Gate: sparse output ==
  flat output byte-identical on every benchmark — proves representation + kernel.*
- **Stage 2 — UNIFORM fast path.** Untouched tiles stay UNIFORM (O(1)); materialize on touch.
  *Gate: still bit-exact; measure working-set + timing vs flat.*
- **Stage 3 — allocator + overflow + tuning.** Free-list pool, overflow handling, tune `TS`/`K`.
  *Gate: bit-exact; A/B flat vs sparse.*
- **Stage 4 — polish.** Free UNIFORM_EMPTY tiles, removed-tracking hooks (fitness), big-grid scaling.

## Verification

- **Bit-exact:** `cmp` the output `.bin` flat vs sparse on `square_600`, `bench_complex`,
  `star_pocket`, `pocket`; residual-volume invariant matches refs (`square_600 = 281 165 072`,
  `star_pocket = 462 722 118`). This is the non-negotiable gate at every stage.
- **Performance:** extend `tools/bench_swept.sh` to a 2-D sweep
  (`AUTOCAM_CARVE_BACKEND ∈ {flat,sparse}` × `AUTOCAM_SWEPT_SKIP ∈ {0,1}`), reporting net carving and a
  peak-working-set debug print (# allocated tile blocks × block bytes). Real iris GPU (EGL/Wayland).

## Risks / open questions

- **GPU dynamic allocation is the crux** (parallel materialization of tile blocks → atomics on a
  free-list; fragmentation). Mitigate with fixed-size blocks + a simple atomic bump/free-list.
- **Overflow columns** (> `K` transitions): rare for milling but must be handled (re-stride tile or
  spill). Pick `K` with margin and guard.
- **Indirection cost:** the per-thread directory lookup adds a small cost; the win must come from the
  reduced working set exceeding it — Stage 2 measures exactly this. If it does **not** beat flat,
  that is itself a valuable result (and the toggle keeps flat as the safe default).
- **Tuning knobs:** tile size `TS` and stride `K` (working set vs overhead vs overflow).
- Keep `flat` the **default** until `sparse` proves a win; the toggle makes this safe and reversible.

## Anticipated files

- `include/carveBackend.hpp` (new) — the interface.
- `src/flatBackend.*` (wrap current BoolOps) + `src/sparseTileBackend.*` (new).
- `shaders/subtract_swept_tiled.comp`, `shaders/materialize_tile.comp` (new).
- `src/gcodeViewer.cpp` — backend selection from env; forward carve/finish/readResult.
- `tools/bench_swept.sh` — add the backend axis.
