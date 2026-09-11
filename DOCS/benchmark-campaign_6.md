# autocam — benchmark campaign

*2026-09-04 10:38 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.55.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 526.41 ± 8.73 | 551.22 ± 10.69 |
| S1 | 28.42 ± 1.64 | 48.01 ± 2.60 |
| S2 | 18.23 ± 1.89 | 35.18 ± 2.72 |
| S3 | 15.53 ± 2.01 | 31.96 ± 2.25 |

S2→S3 netto: **1.173×** · overall S0→S3 netto 33.9× · totale 17.2×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 32.28 ± 3.81 | 51.13 ± 5.29 |
| S3 | 26.19 ± 2.51 | 44.77 ± 4.17 |

S2→S3 netto (N=30): **1.233×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 35.53 ± 2.54 | 10.55 ± 0.29 | 3.369× | 55.96 ± 3.43 | 30.27 ± 2.40 |
| air_moves | 39.51 ± 3.21 | 5.66 ± 0.68 | 6.976× | 59.66 ± 5.62 | 22.80 ± 0.88 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 51.47 ± 3.57 | 10.72 ± 1.18 | 4.42 ± 0.65 | 4.19 ± 0.33 | 2.42× | 1.05× | 12.3× |
| pocket_axis | 154.69 ± 19.95 | 17.53 ± 0.50 | 8.65 ± 0.28 | 9.13 ± 1.02 | 2.03× | 0.95× | 16.9× |
| raster45 | 263.81 ± 12.59 | 44.74 ± 1.47 | 35.96 ± 0.91 | 15.42 ± 1.41 | 1.24× | 2.33× | 17.1× |
| rapids | 81.83 ± 3.62 | 18.21 ± 0.75 | 12.87 ± 0.71 | 7.39 ± 1.01 | 1.42× | 1.74× | 11.1× |
| localized | 39.60 ± 2.94 | 13.57 ± 0.79 | 5.82 ± 0.43 | 5.06 ± 0.57 | 2.33× | 1.15× | 7.8× |
| finishing | 110.33 ± 15.26 | 15.20 ± 0.35 | 7.29 ± 0.47 | 7.82 ± 0.67 | 2.09× | 0.93× | 14.1× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.60 ± 0.52 | 6.98 ± 0.69 | -8.2% | 6.52 ± 0.43 | 6.51 ± 0.82 | -0.2% |
| pocket_axis | 18.03 ± 1.36 | 15.35 ± 1.26 | -14.9% | 17.16 ± 1.94 | 14.92 ± 0.86 | -13.1% |
| raster45 | 43.30 ± 2.19 | 33.46 ± 1.57 | -22.7% | 18.37 ± 1.42 | 18.99 ± 1.57 | +3.4% |
| rapids | 27.18 ± 3.19 | 24.36 ± 2.01 | -10.4% | 16.18 ± 1.77 | 17.03 ± 2.27 | +5.3% |
| localized | 6.95 ± 0.74 | 6.46 ± 0.81 | -7.0% | 4.87 ± 0.40 | 4.87 ± 0.60 | +0.0% |
| finishing | 15.86 ± 1.17 | 13.26 ± 0.95 | -16.4% | 15.32 ± 1.41 | 12.75 ± 0.87 | -16.8% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 2.62. Wall time: 186s.*
