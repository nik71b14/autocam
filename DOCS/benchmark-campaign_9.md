# autocam — benchmark campaign

*2026-09-04 10:56 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.13.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 517.66 ± 6.06 | 545.91 ± 6.58 |
| S1 | 26.76 ± 1.41 | 45.30 ± 2.14 |
| S2 | 17.67 ± 1.30 | 35.15 ± 2.92 |
| S3 | 15.18 ± 1.85 | 32.30 ± 2.39 |

S2→S3 netto: **1.164×** · overall S0→S3 netto 34.1× · totale 16.9×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 31.89 ± 2.44 | 51.04 ± 4.64 |
| S3 | 25.47 ± 1.66 | 44.68 ± 4.37 |

S2→S3 netto (N=30): **1.252×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 31.21 ± 3.90 | 9.71 ± 1.17 | 3.215× | 51.95 ± 7.46 | 27.75 ± 1.40 |
| air_moves | 38.31 ± 4.50 | 4.76 ± 0.72 | 8.055× | 56.93 ± 5.14 | 21.81 ± 1.25 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 51.34 ± 2.02 | 10.13 ± 1.12 | 3.93 ± 0.47 | 4.12 ± 0.53 | 2.58× | 0.95× | 12.5× |
| pocket_axis | 157.31 ± 15.53 | 17.95 ± 0.42 | 8.81 ± 0.53 | 9.38 ± 0.67 | 2.04× | 0.94× | 16.8× |
| raster45 | 267.11 ± 10.16 | 43.66 ± 0.93 | 35.96 ± 1.17 | 14.95 ± 1.61 | 1.21× | 2.41× | 17.9× |
| rapids | 80.19 ± 3.10 | 17.65 ± 0.61 | 13.07 ± 1.35 | 6.72 ± 0.77 | 1.35× | 1.94× | 11.9× |
| localized | 40.96 ± 3.10 | 14.15 ± 1.29 | 5.90 ± 0.69 | 5.46 ± 0.52 | 2.40× | 1.08× | 7.5× |
| finishing | 108.57 ± 13.14 | 15.20 ± 0.43 | 7.56 ± 0.52 | 8.17 ± 0.47 | 2.01× | 0.93× | 13.3× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.45 ± 0.43 | 6.95 ± 0.52 | -6.7% | 6.66 ± 0.71 | 6.16 ± 0.58 | -7.5% |
| pocket_axis | 18.51 ± 1.27 | 15.63 ± 1.22 | -15.5% | 17.13 ± 2.07 | 14.79 ± 1.53 | -13.7% |
| raster45 | 41.73 ± 1.71 | 33.34 ± 1.76 | -20.1% | 17.97 ± 1.66 | 18.52 ± 1.57 | +3.1% |
| rapids | 26.01 ± 2.82 | 22.92 ± 2.41 | -11.9% | 15.09 ± 2.30 | 16.18 ± 2.04 | +7.2% |
| localized | 7.20 ± 0.50 | 6.18 ± 0.44 | -14.1% | 5.47 ± 0.67 | 5.30 ± 0.37 | -3.2% |
| finishing | 15.32 ± 0.85 | 12.82 ± 1.25 | -16.3% | 14.61 ± 1.12 | 12.74 ± 0.93 | -12.8% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 3.15. Wall time: 186s.*
