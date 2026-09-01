#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# gen_bench_diag.py -- parametric generator for the two diagonal stress benchmarks
# of the paper's Table 1 (swept tube-pruning lever). Walks lanes in a 45deg-rotated
# basis: cutting direction u_hat = (1,1)/sqrt2, step direction v_hat = (1,-1)/sqrt2;
# each (u,v) maps to (x,y) = u*u_hat + v*v_hat. Emits one of:
#
#   diag_cut  : N long diagonal G1 CUTS at depth (boustrophedon, minimal air) -- the
#               case where the endpoint AABB (~L^2/2 columns) dwarfs the real swept
#               band (~L*F), so pruning has the most to skip on *cutting* segments.
#   air_moves : N long diagonal G0 RAPIDS with the tool above the stock (no removal)
#               -- the whole-dispatch air-skip case (the removed interval is out of
#               [0,z1), so the entire dispatch is skipped, not merely pruned per column).
#
# Voxel units (stock centre = origin; the 1000x1000x500 reference stock). Z500 = cut
# depth, Z1200 = safe/clear (tool fully out of the stock). Run with the 32-wide tool:
#   python3 tools/gen_bench_diag.py --mode diag_cut  --out gcode/bench/diag_cut.gcode
#   python3 tools/gen_bench_diag.py --mode air_moves --out gcode/bench/air_moves.gcode
#   ./release/autocam simulate --gcode gcode/bench/diag_cut.gcode \
#       --tool test/hemispheric_mill_3.bin --no-view            # (see tools/bench_swept.sh)
# ---------------------------------------------------------------------------
import argparse, math

SQRT2 = math.sqrt(2.0)

def uv_to_xy(u, v):
    # u_hat = (1,1)/sqrt2, v_hat = (1,-1)/sqrt2  ->  (x,y) = u*u_hat + v*v_hat
    return ((u + v) / SQRT2, (u - v) / SQRT2)

def build(mode, n, U, vmax, cut_z, safe_z, feed):
    out = ["%",
           f"(diagonal stress benchmark: {mode}; rotated basis u=(1,1)/2^.5 v=(1,-1)/2^.5)",
           f"(gen_bench_diag.py --mode {mode} --n {n} --U {U:g} --vmax {vmax:g}; "
           f"voxel units, Z{int(cut_z)}=cut Z{int(safe_z)}=safe)",
           f"G0 Z{safe_z:.3f}"]
    vs = [(-vmax + 2 * vmax * k / (n - 1)) if n > 1 else 0.0 for k in range(n)]
    if mode == "diag_cut":
        x0, y0 = uv_to_xy(-U, vs[0])
        out += [f"G0 X{x0:.3f} Y{y0:.3f}", f"G0 Z{cut_z:.3f}", f"G1 Z{cut_z:.3f} F{feed:.3f}"]
        for k, v in enumerate(vs):                       # boustrophedon: long diagonals at depth
            ua, ub = (-U, U) if k % 2 == 0 else (U, -U)
            xa, ya = uv_to_xy(ua, v); xb, yb = uv_to_xy(ub, v)
            out += [f"G1 X{xa:.3f} Y{ya:.3f} F{feed:.3f}",   # step to lane start (short, at depth)
                    f"G1 X{xb:.3f} Y{yb:.3f} F{feed:.3f}"]   # the long diagonal cut
        out.append(f"G0 Z{safe_z:.3f}")
    else:  # air_moves: long diagonal G0 rapids, tool above the stock (no removal)
        for k, v in enumerate(vs):
            ua, ub = (-U, U) if k % 2 == 0 else (U, -U)
            xa, ya = uv_to_xy(ua, v); xb, yb = uv_to_xy(ub, v)
            out += [f"G0 Z{safe_z:.3f}", f"G0 X{xa:.3f} Y{ya:.3f}", f"G0 X{xb:.3f} Y{yb:.3f}"]
    out += [f"G0 Z{safe_z:.3f}", "G0 X0 Y0", "%"]
    return "\n".join(out) + "\n"

def main():
    ap = argparse.ArgumentParser(description="Generate the diagonal stress benchmarks (paper Table 1).")
    ap.add_argument("--mode", required=True, choices=["diag_cut", "air_moves"])
    ap.add_argument("--out", required=True, help="output .gcode path")
    ap.add_argument("--n", type=int, default=13, help="number of long diagonal passes")
    ap.add_argument("--U", type=float, default=420.0, help="half cut length along u_hat (voxels)")
    ap.add_argument("--vmax", type=float, default=200.0, help="max lane offset along v_hat (voxels)")
    ap.add_argument("--cut-z", type=float, default=500.0, help="cut depth (voxel units)")
    ap.add_argument("--safe-z", type=float, default=1200.0, help="safe/clear Z (tool out of stock)")
    ap.add_argument("--feed", type=float, default=500.0, help="feed rate F")
    a = ap.parse_args()
    with open(a.out, "w") as f:
        f.write(build(a.mode, a.n, a.U, a.vmax, a.cut_z, a.safe_z, a.feed))
    print(f"wrote {a.out}")

if __name__ == "__main__":
    main()
