# autocam — benchmark campaign

*2026-09-04 10:48 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.78.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 523.42 ± 5.29 | 549.32 ± 9.41 |
| S1 | 27.33 ± 1.46 | 47.14 ± 2.97 |
| S2 | 17.06 ± 2.18 | 33.61 ± 2.81 |
| S3 | 15.05 ± 2.05 | 31.88 ± 2.85 |

S2→S3 netto: **1.134×** · overall S0→S3 netto 34.8× · totale 17.2×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 31.46 ± 2.94 | 50.12 ± 4.38 |
| S3 | 25.22 ± 1.79 | 44.87 ± 3.82 |

S2→S3 netto (N=30): **1.247×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 31.61 ± 1.27 | 9.71 ± 0.64 | 3.256× | 53.14 ± 3.43 | 28.28 ± 2.04 |
| air_moves | 40.71 ± 4.90 | 5.57 ± 0.96 | 7.308× | 60.12 ± 6.14 | 23.10 ± 1.37 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 51.29 ± 4.57 | 10.06 ± 1.11 | 4.19 ± 0.45 | 4.00 ± 0.48 | 2.40× | 1.05× | 12.8× |
| pocket_axis | 152.30 ± 11.65 | 17.97 ± 0.53 | 8.91 ± 0.60 | 9.61 ± 0.69 | 2.02× | 0.93× | 15.8× |
| raster45 | 258.14 ± 31.66 | 43.91 ± 0.92 | 35.50 ± 0.86 | 14.28 ± 1.37 | 1.24× | 2.49× | 18.1× |
| rapids | 78.64 ± 8.76 | 17.28 ± 0.71 | 12.53 ± 0.93 | 6.58 ± 1.17 | 1.38× | 1.91× | 12.0× |
| localized | 39.44 ± 3.66 | 13.66 ± 1.37 | 5.71 ± 0.90 | 4.93 ± 0.54 | 2.39× | 1.16× | 8.0× |
| finishing | 107.44 ± 12.04 | 15.45 ± 0.64 | 7.66 ± 0.49 | 7.91 ± 0.46 | 2.02× | 0.97× | 13.6× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.38 ± 0.45 | 6.70 ± 0.79 | -9.2% | 6.60 ± 0.57 | 6.25 ± 0.66 | -5.3% |
| pocket_axis | 18.63 ± 1.57 | 16.15 ± 1.18 | -13.3% | 17.89 ± 1.26 | 15.57 ± 0.65 | -13.0% |
| raster45 | 43.94 ± 3.87 | 33.97 ± 2.13 | -22.7% | 18.76 ± 1.78 | 19.91 ± 1.92 | +6.1% |
| rapids | 25.82 ± 2.92 | 23.01 ± 2.17 | -10.9% | 14.75 ± 2.08 | 15.10 ± 1.67 | +2.4% |
| localized | 6.84 ± 0.47 | 6.51 ± 0.46 | -4.9% | 5.74 ± 0.50 | 5.61 ± 0.76 | -2.3% |
| finishing | 14.81 ± 1.26 | 12.95 ± 0.68 | -12.6% | 14.44 ± 1.12 | 12.22 ± 0.98 | -15.4% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 2.31. Wall time: 185s.*
