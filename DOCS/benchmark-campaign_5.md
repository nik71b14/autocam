# autocam — benchmark campaign

*2026-09-04 10:33 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.78.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 519.86 ± 6.18 | 548.29 ± 5.73 |
| S1 | 27.23 ± 0.96 | 48.17 ± 3.30 |
| S2 | 17.84 ± 1.74 | 35.14 ± 2.00 |
| S3 | 15.29 ± 1.83 | 32.09 ± 2.31 |

S2→S3 netto: **1.167×** · overall S0→S3 netto 34.0× · totale 17.1×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 32.31 ± 2.60 | 52.52 ± 4.18 |
| S3 | 25.67 ± 1.77 | 45.67 ± 3.44 |

S2→S3 netto (N=30): **1.259×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 33.45 ± 4.83 | 10.32 ± 1.21 | 3.240× | 53.28 ± 7.23 | 28.76 ± 2.06 |
| air_moves | 41.92 ± 4.58 | 5.79 ± 1.19 | 7.241× | 61.68 ± 5.03 | 23.05 ± 1.54 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 50.95 ± 3.57 | 10.08 ± 0.84 | 3.97 ± 0.46 | 4.03 ± 0.64 | 2.54× | 0.99× | 12.7× |
| pocket_axis | 145.28 ± 8.94 | 18.01 ± 0.55 | 8.81 ± 0.47 | 9.06 ± 0.67 | 2.04× | 0.97× | 16.0× |
| raster45 | 271.76 ± 19.95 | 43.60 ± 1.14 | 35.52 ± 0.85 | 14.98 ± 1.31 | 1.23× | 2.37× | 18.1× |
| rapids | 82.60 ± 7.96 | 17.37 ± 0.69 | 13.07 ± 1.66 | 6.73 ± 1.08 | 1.33× | 1.94× | 12.3× |
| localized | 38.27 ± 5.69 | 13.13 ± 1.70 | 5.98 ± 0.96 | 5.01 ± 1.02 | 2.19× | 1.19× | 7.6× |
| finishing | 114.60 ± 15.50 | 15.19 ± 0.46 | 7.51 ± 0.48 | 8.01 ± 0.73 | 2.02× | 0.94× | 14.3× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.42 ± 0.67 | 6.79 ± 0.81 | -8.4% | 6.78 ± 0.81 | 5.85 ± 0.46 | -13.7% |
| pocket_axis | 18.12 ± 1.42 | 15.58 ± 1.18 | -14.0% | 17.46 ± 1.27 | 14.82 ± 1.49 | -15.1% |
| raster45 | 45.41 ± 3.74 | 34.40 ± 2.64 | -24.2% | 18.72 ± 1.88 | 19.90 ± 2.82 | +6.3% |
| rapids | 27.33 ± 2.77 | 23.85 ± 2.41 | -12.7% | 16.02 ± 2.36 | 16.84 ± 1.74 | +5.2% |
| localized | 7.15 ± 0.32 | 6.47 ± 0.41 | -9.5% | 5.35 ± 0.37 | 5.26 ± 0.55 | -1.6% |
| finishing | 14.74 ± 1.66 | 12.44 ± 1.13 | -15.6% | 14.66 ± 0.92 | 12.41 ± 0.76 | -15.4% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 2.85. Wall time: 186s.*
