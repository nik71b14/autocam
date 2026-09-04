# autocam — benchmark campaign

*2026-09-04 08:49 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.67.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 521.79 ± 7.72 | 550.11 ± 7.73 |
| S1 | 26.60 ± 1.27 | 45.03 ± 2.21 |
| S2 | 17.71 ± 1.44 | 34.56 ± 1.71 |
| S3 | 15.26 ± 1.79 | 32.61 ± 3.68 |

S2→S3 netto: **1.161×** · overall S0→S3 netto 34.2× · totale 16.9×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 30.95 ± 3.27 | 49.79 ± 3.76 |
| S3 | 24.73 ± 2.12 | 43.46 ± 3.99 |

S2→S3 netto (N=30): **1.252×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 34.03 ± 2.24 | 10.27 ± 0.63 | 3.313× | 55.68 ± 2.52 | 29.11 ± 1.74 |
| air_moves | 39.29 ± 4.45 | 5.44 ± 0.85 | 7.217× | 58.51 ± 5.63 | 23.46 ± 1.83 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 52.44 ± 5.71 | 10.23 ± 1.11 | 4.17 ± 0.31 | 4.18 ± 0.49 | 2.45× | 1.00× | 12.6× |
| pocket_axis | 152.65 ± 8.79 | 18.16 ± 0.44 | 8.71 ± 0.30 | 9.53 ± 0.91 | 2.08× | 0.91× | 16.0× |
| raster45 | 263.51 ± 16.50 | 43.92 ± 1.13 | 35.17 ± 0.68 | 14.75 ± 1.30 | 1.25× | 2.38× | 17.9× |
| rapids | 82.07 ± 9.26 | 17.45 ± 0.83 | 13.12 ± 0.94 | 6.80 ± 0.63 | 1.33× | 1.93× | 12.1× |
| localized | 39.89 ± 3.41 | 13.80 ± 1.03 | 5.81 ± 0.64 | 5.17 ± 0.78 | 2.37× | 1.12× | 7.7× |
| finishing | 113.07 ± 11.44 | 15.48 ± 0.45 | 7.84 ± 0.43 | 8.62 ± 0.49 | 1.98× | 0.91× | 13.1× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.08 ± 0.83 | 6.98 ± 0.44 | -1.3% | 6.72 ± 0.65 | 6.00 ± 0.73 | -10.8% |
| pocket_axis | 18.38 ± 1.30 | 15.63 ± 1.02 | -14.9% | 17.85 ± 1.25 | 15.21 ± 0.83 | -14.8% |
| raster45 | 43.90 ± 2.47 | 33.99 ± 2.37 | -22.6% | 18.71 ± 1.73 | 19.23 ± 1.69 | +2.8% |
| rapids | 27.49 ± 3.20 | 24.45 ± 2.89 | -11.0% | 15.71 ± 2.22 | 16.34 ± 2.18 | +4.0% |
| localized | 7.12 ± 0.49 | 6.64 ± 0.52 | -6.7% | 5.73 ± 0.53 | 5.66 ± 0.46 | -1.2% |
| finishing | 15.84 ± 1.31 | 13.13 ± 0.96 | -17.1% | 14.78 ± 1.57 | 12.64 ± 0.93 | -14.4% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 3.48. Wall time: 187s.*
