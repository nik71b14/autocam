# autocam — benchmark campaign

*2026-09-04 08:39 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.97.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 521.83 ± 9.50 | 547.97 ± 10.75 |
| S1 | 27.61 ± 1.56 | 46.61 ± 2.63 |
| S2 | 17.90 ± 1.94 | 35.05 ± 2.89 |
| S3 | 15.71 ± 2.00 | 33.51 ± 3.09 |

S2→S3 netto: **1.139×** · overall S0→S3 netto 33.2× · totale 16.4×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 31.68 ± 3.45 | 49.72 ± 4.51 |
| S3 | 25.86 ± 2.38 | 44.53 ± 3.99 |

S2→S3 netto (N=30): **1.225×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 31.86 ± 3.10 | 10.05 ± 0.59 | 3.169× | 52.92 ± 3.20 | 28.06 ± 1.49 |
| air_moves | 36.58 ± 2.25 | 4.83 ± 0.60 | 7.575× | 54.42 ± 2.98 | 22.30 ± 1.26 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 50.83 ± 4.04 | 10.02 ± 1.04 | 4.04 ± 0.62 | 4.58 ± 0.78 | 2.48× | 0.88× | 11.1× |
| pocket_axis | 145.50 ± 11.53 | 17.75 ± 0.52 | 8.67 ± 0.68 | 9.30 ± 0.79 | 2.05× | 0.93× | 15.7× |
| raster45 | 270.56 ± 14.26 | 44.35 ± 0.87 | 35.49 ± 0.80 | 14.87 ± 1.45 | 1.25× | 2.39× | 18.2× |
| rapids | 80.62 ± 8.62 | 17.89 ± 1.08 | 13.13 ± 1.15 | 6.77 ± 1.29 | 1.36× | 1.94× | 11.9× |
| localized | 39.30 ± 4.50 | 13.06 ± 1.71 | 5.40 ± 0.76 | 4.69 ± 0.63 | 2.42× | 1.15× | 8.4× |
| finishing | 108.54 ± 7.24 | 15.50 ± 0.24 | 7.83 ± 0.33 | 8.46 ± 0.67 | 1.98× | 0.93× | 12.8× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.54 ± 0.50 | 7.06 ± 1.04 | -6.4% | 6.67 ± 0.59 | 6.04 ± 0.80 | -9.4% |
| pocket_axis | 18.00 ± 1.77 | 15.85 ± 1.15 | -11.9% | 18.17 ± 0.85 | 14.77 ± 1.73 | -18.7% |
| raster45 | 43.46 ± 2.49 | 33.67 ± 3.09 | -22.5% | 18.49 ± 2.58 | 19.59 ± 2.82 | +6.0% |
| rapids | 27.82 ± 1.89 | 24.54 ± 2.28 | -11.8% | 17.45 ± 1.87 | 17.66 ± 1.58 | +1.2% |
| localized | 7.01 ± 0.81 | 6.39 ± 0.67 | -8.8% | 5.43 ± 0.34 | 5.55 ± 0.65 | +2.2% |
| finishing | 15.21 ± 1.45 | 13.13 ± 0.90 | -13.7% | 15.19 ± 1.20 | 12.44 ± 1.07 | -18.1% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 2.96. Wall time: 186s.*
