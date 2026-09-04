# autocam — benchmark campaign

*2026-09-04 11:02 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.45.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 521.88 ± 6.95 | 549.69 ± 7.49 |
| S1 | 27.75 ± 1.44 | 46.77 ± 2.68 |
| S2 | 17.86 ± 1.54 | 35.25 ± 1.76 |
| S3 | 15.51 ± 1.85 | 33.59 ± 4.45 |

S2→S3 netto: **1.152×** · overall S0→S3 netto 33.6× · totale 16.4×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 32.67 ± 2.80 | 53.09 ± 4.80 |
| S3 | 26.24 ± 2.15 | 46.10 ± 4.22 |

S2→S3 netto (N=30): **1.245×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 34.25 ± 3.27 | 10.25 ± 0.75 | 3.342× | 55.63 ± 6.04 | 28.84 ± 1.79 |
| air_moves | 41.53 ± 3.59 | 5.60 ± 0.95 | 7.416× | 60.32 ± 4.53 | 23.36 ± 1.39 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 52.68 ± 5.00 | 9.71 ± 0.81 | 3.89 ± 0.36 | 4.01 ± 0.35 | 2.49× | 0.97× | 13.1× |
| pocket_axis | 150.32 ± 9.32 | 17.68 ± 0.39 | 8.76 ± 0.73 | 9.21 ± 0.47 | 2.02× | 0.95× | 16.3× |
| raster45 | 267.48 ± 30.47 | 43.74 ± 0.86 | 35.62 ± 0.93 | 14.98 ± 1.13 | 1.23× | 2.38× | 17.9× |
| rapids | 76.91 ± 5.10 | 17.32 ± 0.69 | 11.93 ± 0.58 | 5.96 ± 0.80 | 1.45× | 2.00× | 12.9× |
| localized | 37.60 ± 4.83 | 13.23 ± 1.12 | 6.05 ± 1.07 | 4.68 ± 0.57 | 2.19× | 1.29× | 8.0× |
| finishing | 113.13 ± 14.77 | 15.36 ± 0.61 | 7.76 ± 0.34 | 8.26 ± 0.64 | 1.98× | 0.94× | 13.7× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.36 ± 0.74 | 6.95 ± 0.67 | -5.6% | 6.85 ± 0.60 | 6.40 ± 0.81 | -6.7% |
| pocket_axis | 19.05 ± 0.97 | 15.87 ± 1.15 | -16.7% | 17.88 ± 1.12 | 15.12 ± 0.94 | -15.4% |
| raster45 | 42.91 ± 1.90 | 32.80 ± 1.59 | -23.6% | 17.82 ± 1.49 | 18.43 ± 1.87 | +3.4% |
| rapids | 26.06 ± 2.76 | 24.66 ± 2.61 | -5.4% | 15.44 ± 2.14 | 15.58 ± 1.62 | +0.9% |
| localized | 6.97 ± 0.49 | 6.48 ± 0.39 | -7.1% | 5.56 ± 0.47 | 5.34 ± 0.64 | -4.0% |
| finishing | 15.64 ± 1.00 | 13.10 ± 0.72 | -16.3% | 14.96 ± 1.44 | 12.77 ± 0.77 | -14.6% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 2.84. Wall time: 187s.*
