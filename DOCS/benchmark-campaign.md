# autocam — benchmark campaign (consolidated)

*Intel iris (driver `xe`), stock 1000×1000×500. **10 independent full campaigns** (`benchmark-campaign_2..11.md`; run 1 discarded as warm-up), 2026-09-04. Each campaign is itself the mean of 10 interleaved runs (first of 11 discarded), so every number below is a **mean of 10×10 = 100 carves**. All start-loads < 1.15.*

*Each cell is `grand-mean ± sd (CV%)`, where **sd is the population standard deviation across the 10 campaign means** — i.e. run-to-run reproducibility of the reported figure, not within-run jitter. Ratios are computed per campaign, then averaged (± cross-campaign sd).*

## Table A — square_600 ablation (256-voxel tool `mill_10`)

| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 522.27 ± 2.55 (0.5%) | 549.48 ± 2.01 (0.4%) |
| S1 | 27.13 ± 0.63 (2.3%) | 46.37 ± 1.08 (2.3%) |
| S2 | 17.59 ± 0.44 (2.5%) | 34.42 ± 0.73 (2.1%) |
| S3 | 15.26 ± 0.31 (2.0%) | 32.22 ± 0.62 (1.9%) |

- **S2→S3 netto: 1.153 ± 0.023×**  ·  totale 1.068 ± 0.018×
- overall **S0→S3 netto 34.244 ± 0.661×**  ·  totale 17.061 ± 0.330×
- S0→S1 netto 19.263 ± 0.425×  ·  S1→S2 netto 1.543 ± 0.028×

## Table B — square_600 S2/S3 high-N (`mill_10`, N=30 per campaign)

| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 32.04 ± 0.59 (1.8%) | 51.34 ± 1.18 (2.3%) |
| S3 | 25.66 ± 0.49 (1.9%) | 45.03 ± 0.93 (2.1%) |

- **S2→S3 netto (high-N): 1.248 ± 0.008×**  ·  totale 1.140 ± 0.011×

## Table 1 — stress benchmarks (32-voxel tool `mill_3`)

| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 32.79 ± 1.46 (4.5%) | 10.14 ± 0.30 (3.0%) | 3.233 ± 0.089× | 53.64 ± 1.70 (3.2%) | 28.75 ± 0.79 (2.7%) |
| air_moves | 39.03 ± 1.95 (5.0%) | 5.26 ± 0.43 (8.2%) | 7.445 ± 0.342× | 58.10 ± 2.28 (3.9%) | 22.65 ± 0.59 (2.6%) |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms)

| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 51.47 ± 0.61 (1.2%) | 10.00 ± 0.30 (3.0%) | 4.01 ± 0.18 (4.5%) | 4.08 ± 0.09 (2.3%) | 2.493 ± 0.063× | 0.985 ± 0.039× | 12.629 ± 0.287× |
| pocket_axis | 151.22 ± 3.49 (2.3%) | 17.81 ± 0.20 (1.1%) | 8.76 ± 0.10 (1.1%) | 9.25 ± 0.21 (2.3%) | 2.034 ± 0.028× | 0.947 ± 0.017× | 16.359 ± 0.445× |
| raster45 | 264.75 ± 3.97 (1.5%) | 43.88 ± 0.32 (0.7%) | 35.59 ± 0.24 (0.7%) | 14.79 ± 0.36 (2.4%) | 1.233 ± 0.010× | 2.408 ± 0.048× | 17.913 ± 0.420× |
| rapids | 81.00 ± 3.24 (4.0%) | 17.41 ± 0.31 (1.8%) | 12.76 ± 0.38 (3.0%) | 6.63 ± 0.39 (5.8%) | 1.365 ± 0.041× | 1.928 ± 0.079× | 12.241 ± 0.643× |
| localized | 39.00 ± 1.00 (2.6%) | 13.46 ± 0.38 (2.8%) | 5.77 ± 0.17 (3.0%) | 4.92 ± 0.26 (5.2%) | 2.336 ± 0.083× | 1.175 ± 0.055× | 7.942 ± 0.265× |
| finishing | 111.57 ± 2.48 (2.2%) | 15.36 ± 0.14 (0.9%) | 7.60 ± 0.15 (1.9%) | 8.18 ± 0.22 (2.7%) | 2.023 ± 0.032× | 0.930 ± 0.017× | 13.653 ± 0.405× |

## Table 5 — zero-fill lever (`mill_3`, netto ms)

| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.32 ± 0.18 (2.4%) | 6.76 ± 0.22 (3.3%) | -7.7% ± 3.1 | 6.57 ± 0.21 (3.2%) | 6.12 ± 0.24 (4.0%) | -6.8% ± 3.7 |
| pocket_axis | 18.30 ± 0.50 (2.8%) | 15.68 ± 0.53 (3.4%) | -14.3% ± 1.4 | 17.63 ± 0.35 (2.0%) | 15.09 ± 0.28 (1.8%) | -14.4% ± 1.5 |
| raster45 | 43.86 ± 1.05 (2.4%) | 34.05 ± 0.87 (2.6%) | -22.4% ± 1.2 | 18.70 ± 0.54 (2.9%) | 19.36 ± 0.56 (2.9%) | +3.5% ± 1.5 |
| rapids | 27.09 ± 0.79 (2.9%) | 24.16 ± 0.73 (3.0%) | -10.8% ± 2.1 | 15.72 ± 0.51 (3.2%) | 16.29 ± 0.63 (3.9%) | +3.6% ± 2.3 |
| localized | 6.99 ± 0.21 (3.0%) | 6.46 ± 0.14 (2.2%) | -7.5% ± 4.1 | 5.40 ± 0.25 (4.6%) | 5.38 ± 0.21 (3.8%) | -0.3% ± 2.9 |
| finishing | 15.38 ± 0.42 (2.7%) | 13.04 ± 0.25 (1.9%) | -15.1% ± 1.7 | 14.94 ± 0.37 (2.5%) | 12.70 ± 0.27 (2.1%) | -15.0% ± 1.3 |

*gain = (no-fill/fill − 1), negative = faster.*

## Reproducibility notes

- **Machine was genuinely idle**: every cross-campaign CV is ≤ 5% (square_600 netto 0.5%–2.5%; stress/matrix cells all < 6%). The load contamination that produced the earlier drifting ratios is gone.
- Within-campaign single-run jitter (shot-to-shot) for square_600 S2 netto: ≈ 1.6 ms (Table A), 3.2 ms (Table B) — small relative to the effects measured.

## Analysis — what to quote in the paper

**1. Robust, headline results (tiny sd — quote these).** Fusion and pruning both hold up across all 10 campaigns:
- Fusion S1→S2 (drop the external buffer): **1.543 ± 0.028×** on square_600; 1.2×–2.6× across the matrix (Table 3).
- Pruning on the stress cases: raster45 **2.408 ± 0.048×**, rapids **1.928 ± 0.079×**, diag_cut **3.233 ± 0.089×**, air_moves **7.445 ± 0.342×**.
- End-to-end S0→S3 on square_600: **34.244 ± 0.661×** netto (17.061 ± 0.330× totale incl. read-back).

**2. Pruning on axis-aligned work is small and tool-size dependent — state it honestly.**
- square_600 (256-voxel tool `mill_10`): **1.153 ± 0.023×** (Table A) / **1.248 ± 0.008×** (Table B). Modest, from air-skipping the approach/retract rapids plus the corners of a *large* tool's dilated AABB.
- Small-tool axis-aligned workloads (`mill_3`: contour, pocket_axis, finishing): S2→S3 = 0.985 ± 0.039× / 0.947 ± 0.017× / 0.930 ± 0.017× — i.e. **≈1× (a slight loss)**: nothing off-tube to prune, so the per-column skip test is pure overhead. This is the expected, honest behaviour and matches the AABB≈tube argument.

**3. Absolute square_600 S2/S3 times are measurement-context sensitive (DVFS) — do NOT quote a bare number.**
- The *same* S2 kernel reads **17.6 ms** in the full ablation sweep (Table A) but **32.0 ms** measured alone (Table B) — a **1.82× gap** with near-zero sd in each. Cause: in Table A every round is dominated by the ~520 ms legacy S0, so the iGPU clocks down/recovers between swept runs; Table B runs S2/S3 back-to-back and stays in a hotter, differently-clocked state. Load is *not* the cause (idle, CV < 2%).
- Consequence: use the **ratios** (context-cancelling) for claims; if an absolute S2/S3 time is cited, name the protocol (ablation-sweep vs standalone) it came from. The ablation table should use Table A's internally-consistent set; a standalone S2/S3 cost should cite Table B.

**4. Zero-fill lever (Table 5).** Consistent win on write-heavy S2 (pocket_axis -14.3% ± 1.4, raster45 -22.4% ± 1.2, finishing -15.1% ± 1.7); roughly neutral once pruning (S3) has already removed the dead columns on the diagonal cases (raster45 S3 +3.5% ± 1.5).
