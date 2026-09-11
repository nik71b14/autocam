# autocam — benchmark campaign

*2026-09-04 11:06 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 1.12.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 519.35 ± 8.60 | 546.22 ± 8.76 |
| S1 | 26.91 ± 1.79 | 45.38 ± 3.33 |
| S2 | 17.23 ± 1.44 | 33.46 ± 1.45 |
| S3 | 14.77 ± 1.90 | 31.59 ± 2.66 |

S2→S3 netto: **1.166×** · overall S0→S3 netto 35.2× · totale 17.3×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 31.64 ± 3.63 | 50.64 ± 5.72 |
| S3 | 25.40 ± 2.70 | 44.52 ± 4.24 |

S2→S3 netto (N=30): **1.246×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 30.97 ± 2.13 | 10.12 ± 0.84 | 3.059× | 51.91 ± 3.77 | 28.33 ± 1.86 |
| air_moves | 36.37 ± 2.89 | 4.59 ± 0.86 | 7.920× | 53.80 ± 3.06 | 21.89 ± 1.14 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 50.49 ± 3.90 | 9.63 ± 0.67 | 3.89 ± 0.43 | 3.92 ± 0.34 | 2.48× | 0.99× | 12.9× |
| pocket_axis | 148.36 ± 10.36 | 17.61 ± 0.43 | 8.76 ± 0.58 | 9.10 ± 0.67 | 2.01× | 0.96× | 16.3× |
| raster45 | 262.21 ± 14.97 | 43.86 ± 0.93 | 35.70 ± 1.59 | 14.93 ± 1.75 | 1.23× | 2.39× | 17.6× |
| rapids | 80.05 ± 4.03 | 17.28 ± 0.43 | 13.13 ± 1.27 | 6.70 ± 1.08 | 1.32× | 1.96× | 11.9× |
| localized | 37.52 ± 3.93 | 13.10 ± 1.14 | 5.66 ± 0.55 | 4.64 ± 0.40 | 2.31× | 1.22× | 8.1× |
| finishing | 111.23 ± 6.48 | 15.62 ± 0.62 | 7.56 ± 0.43 | 8.27 ± 0.63 | 2.07× | 0.91× | 13.4× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.23 ± 0.79 | 6.82 ± 0.50 | -5.7% | 6.62 ± 0.48 | 6.01 ± 0.42 | -9.2% |
| pocket_axis | 17.63 ± 1.74 | 15.06 ± 1.37 | -14.6% | 17.70 ± 1.35 | 14.67 ± 1.32 | -17.1% |
| raster45 | 45.44 ± 4.16 | 36.28 ± 3.97 | -20.2% | 19.83 ± 2.79 | 20.23 ± 2.80 | +2.0% |
| rapids | 28.27 ± 2.66 | 25.01 ± 2.26 | -11.5% | 16.08 ± 2.68 | 16.34 ± 2.24 | +1.6% |
| localized | 6.88 ± 0.60 | 6.41 ± 0.40 | -6.8% | 5.32 ± 0.62 | 5.43 ± 0.64 | +2.0% |
| finishing | 14.83 ± 1.28 | 13.13 ± 0.67 | -11.5% | 15.29 ± 0.84 | 12.93 ± 0.83 | -15.4% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 3.11. Wall time: 186s.*
