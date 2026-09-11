# autocam — benchmark campaign

*2026-09-04 10:43 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.55.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 524.53 ± 3.66 | 552.23 ± 4.89 |
| S1 | 26.00 ± 1.35 | 45.30 ± 2.97 |
| S2 | 16.72 ± 1.53 | 33.25 ± 2.45 |
| S3 | 14.86 ± 2.03 | 31.36 ± 2.57 |

S2→S3 netto: **1.125×** · overall S0→S3 netto 35.3× · totale 17.6×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 32.82 ± 3.92 | 53.02 ± 5.96 |
| S3 | 25.98 ± 2.34 | 46.13 ± 4.22 |

S2→S3 netto (N=30): **1.263×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 31.17 ± 2.11 | 9.72 ± 0.78 | 3.207× | 50.70 ± 2.62 | 27.60 ± 1.67 |
| air_moves | 35.55 ± 1.13 | 4.62 ± 0.41 | 7.686× | 55.78 ± 3.38 | 21.85 ± 0.45 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 51.40 ± 4.44 | 9.77 ± 0.73 | 3.81 ± 0.31 | 4.15 ± 0.55 | 2.57× | 0.92× | 12.4× |
| pocket_axis | 153.15 ± 20.16 | 17.65 ± 0.54 | 8.60 ± 0.45 | 8.89 ± 0.48 | 2.05× | 0.97× | 17.2× |
| raster45 | 263.39 ± 16.02 | 43.88 ± 1.19 | 35.70 ± 0.90 | 14.95 ± 1.17 | 1.23× | 2.39× | 17.6× |
| rapids | 88.56 ± 16.62 | 17.12 ± 0.68 | 12.48 ± 0.78 | 6.53 ± 1.24 | 1.37× | 1.91× | 13.6× |
| localized | 38.63 ± 2.82 | 13.34 ± 1.25 | 5.69 ± 0.62 | 4.76 ± 0.73 | 2.34× | 1.20× | 8.1× |
| finishing | 115.81 ± 9.78 | 15.29 ± 0.36 | 7.50 ± 0.51 | 8.13 ± 0.79 | 2.04× | 0.92× | 14.2× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.45 ± 0.52 | 6.38 ± 0.78 | -14.3% | 6.19 ± 0.74 | 5.68 ± 0.64 | -8.3% |
| pocket_axis | 18.98 ± 1.07 | 16.90 ± 1.10 | -11.0% | 18.08 ± 1.49 | 15.18 ± 0.98 | -16.0% |
| raster45 | 44.11 ± 2.30 | 34.07 ± 1.99 | -22.8% | 18.96 ± 1.78 | 19.55 ± 1.75 | +3.1% |
| rapids | 27.69 ± 2.45 | 24.68 ± 2.46 | -10.9% | 16.54 ± 2.48 | 16.94 ± 1.80 | +2.4% |
| localized | 6.78 ± 0.51 | 6.69 ± 0.50 | -1.4% | 5.50 ± 0.47 | 5.40 ± 0.62 | -1.9% |
| finishing | 15.65 ± 1.07 | 13.29 ± 0.69 | -15.1% | 15.64 ± 0.82 | 13.25 ± 1.43 | -15.3% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 4.38. Wall time: 186s.*
