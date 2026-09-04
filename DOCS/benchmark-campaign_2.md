# autocam — benchmark campaign

*2026-09-04 08:44 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.76.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 524.11 ± 7.56 | 551.29 ± 7.63 |
| S1 | 27.35 ± 1.82 | 46.38 ± 3.06 |
| S2 | 18.03 ± 1.66 | 34.20 ± 2.25 |
| S3 | 15.25 ± 1.80 | 31.96 ± 2.73 |

S2→S3 netto: **1.182×** · overall S0→S3 netto 34.4× · totale 17.3×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 31.64 ± 2.76 | 50.06 ± 3.44 |
| S3 | 25.44 ± 1.95 | 43.83 ± 2.88 |

S2→S3 netto (N=30): **1.244×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 32.80 ± 3.26 | 10.30 ± 0.91 | 3.186× | 53.72 ± 4.26 | 28.82 ± 2.78 |
| air_moves | 38.90 ± 2.56 | 5.13 ± 0.58 | 7.587× | 57.55 ± 4.54 | 22.52 ± 0.87 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 51.41 ± 2.74 | 9.88 ± 0.90 | 3.84 ± 0.43 | 3.98 ± 0.57 | 2.57× | 0.96× | 12.9× |
| pocket_axis | 151.50 ± 10.46 | 17.65 ± 0.59 | 8.90 ± 0.44 | 9.34 ± 0.81 | 1.98× | 0.95× | 16.2× |
| raster45 | 269.59 ± 18.51 | 43.56 ± 0.77 | 35.39 ± 0.83 | 14.39 ± 1.27 | 1.23× | 2.46× | 18.7× |
| rapids | 82.33 ± 10.25 | 17.29 ± 0.87 | 12.97 ± 1.01 | 6.87 ± 1.56 | 1.33× | 1.89× | 12.0× |
| localized | 38.90 ± 3.64 | 12.82 ± 1.39 | 5.45 ± 0.92 | 4.59 ± 0.68 | 2.35× | 1.19× | 8.5× |
| finishing | 111.22 ± 12.53 | 15.30 ± 0.60 | 7.60 ± 0.62 | 8.39 ± 0.68 | 2.01× | 0.91× | 13.3× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 6.97 ± 0.87 | 6.35 ± 0.71 | -8.9% | 6.54 ± 0.59 | 6.31 ± 0.52 | -3.6% |
| pocket_axis | 17.41 ± 1.78 | 14.91 ± 1.26 | -14.4% | 17.16 ± 1.48 | 15.14 ± 1.51 | -11.8% |
| raster45 | 44.24 ± 2.30 | 34.20 ± 2.33 | -22.7% | 19.04 ± 2.63 | 19.36 ± 2.18 | +1.7% |
| rapids | 27.41 ± 3.70 | 23.61 ± 2.60 | -13.9% | 15.56 ± 2.29 | 15.64 ± 2.50 | +0.5% |
| localized | 6.65 ± 0.81 | 6.44 ± 0.61 | -3.1% | 5.23 ± 0.41 | 5.44 ± 0.45 | +4.1% |
| finishing | 15.75 ± 1.05 | 13.28 ± 0.93 | -15.7% | 15.13 ± 1.37 | 12.55 ± 0.98 | -17.0% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 2.58. Wall time: 186s.*
