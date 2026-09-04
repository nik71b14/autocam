# autocam — benchmark campaign

*2026-09-04 10:28 · Intel iris (driver xe) · interleaved, first of N discarded, mean ± population sd. Stock 1000×1000×500. Load at start: 0.43.*

## Table A — square_600 ablation (256-voxel tool `mill_10`), N=10
| level | netto (ms) | totale incl. read-back (ms) |
|---|--:|--:|
| S0 | 523.67 ± 6.07 | 550.53 ± 7.27 |
| S1 | 26.91 ± 1.30 | 46.25 ± 2.75 |
| S2 | 17.54 ± 1.26 | 34.41 ± 2.25 |
| S3 | 15.87 ± 2.39 | 32.85 ± 3.16 |

S2→S3 netto: **1.105×** · overall S0→S3 netto 33.0× · totale 16.8×

## Table B — square_600 S2/S3 high-N (`mill_10`), N=30
| level | netto (ms) | totale (ms) |
|---|--:|--:|
| S2 | 32.69 ± 4.11 | 52.02 ± 5.49 |
| S3 | 26.31 ± 3.71 | 46.25 ± 6.09 |

S2→S3 netto (N=30): **1.242×**

## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N=10
| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |
|---|--:|--:|--:|--:|--:|
| diag_cut | 32.85 ± 2.53 | 10.46 ± 0.79 | 3.141× | 54.40 ± 3.70 | 29.76 ± 3.16 |
| air_moves | 38.19 ± 2.41 | 5.43 ± 0.70 | 7.031× | 56.63 ± 3.31 | 22.69 ± 1.22 |

*(square_600 control is Table A.)*

## Table 3 — cross-workload matrix (`mill_3`, netto ms), N=10
| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |
|---|--:|--:|--:|--:|--:|--:|--:|
| contour | 51.19 ± 3.85 | 9.80 ± 0.90 | 4.04 ± 0.52 | 4.19 ± 0.61 | 2.42× | 0.97× | 12.2× |
| pocket_axis | 146.59 ± 12.02 | 17.90 ± 0.62 | 8.67 ± 0.33 | 9.22 ± 0.59 | 2.06× | 0.94× | 15.9× |
| raster45 | 260.54 ± 25.77 | 43.94 ± 1.10 | 35.34 ± 1.21 | 14.24 ± 1.17 | 1.24× | 2.48× | 18.3× |
| rapids | 76.77 ± 7.42 | 17.11 ± 0.63 | 12.45 ± 1.04 | 6.04 ± 0.79 | 1.37× | 2.06× | 12.7× |
| localized | 39.15 ± 3.35 | 13.79 ± 1.08 | 5.59 ± 0.42 | 4.88 ± 0.41 | 2.46× | 1.15× | 8.0× |
| finishing | 110.27 ± 13.43 | 15.52 ± 0.26 | 7.69 ± 0.58 | 8.18 ± 0.63 | 2.02× | 0.94× | 13.5× |
## Table 5 — zero-fill lever (`mill_3`, netto ms), N=10
| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |
|---|--:|--:|--:|--:|--:|--:|
| contour | 7.30 ± 0.86 | 6.67 ± 0.51 | -8.6% | 6.20 ± 0.43 | 5.99 ± 0.69 | -3.4% |
| pocket_axis | 18.22 ± 1.08 | 15.68 ± 1.28 | -13.9% | 18.01 ± 0.97 | 15.48 ± 0.90 | -14.0% |
| raster45 | 43.58 ± 3.43 | 33.95 ± 3.20 | -22.1% | 18.86 ± 3.20 | 19.47 ± 2.94 | +3.2% |
| rapids | 27.69 ± 3.14 | 25.01 ± 2.71 | -9.7% | 15.88 ± 2.45 | 16.92 ± 2.18 | +6.6% |
| localized | 7.38 ± 0.60 | 6.29 ± 0.82 | -14.7% | 5.21 ± 0.75 | 5.47 ± 0.37 | +4.9% |
| finishing | 15.33 ± 1.33 | 13.05 ± 0.99 | -14.9% | 14.60 ± 1.71 | 12.72 ± 0.64 | -12.9% |

*gain = (no-fill/fill − 1), negative = faster. Load at end: 2.62. Wall time: 186s.*
