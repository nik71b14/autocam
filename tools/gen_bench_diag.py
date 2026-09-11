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
#   air_moves : an AIR-HEAVY program -- N short diagonal cuts at alternating far ends,
#               each reached by a LONG diagonal G0 reposition across the stock. The long
#               in-air rapids dominate the baseline (their big diagonal AABB is rewritten
#               for zero removal) and are whole-dispatch-skipped by pruning, while the
#               short cuts keep the pruned time finite -> a realistic, bounded speedup.
#
# Voxel units (stock centre = origin; the 1000x1000x500 reference stock). Z500 = cut
# depth; the tool is out of the stock for Z>~517, so Z600 clears it with a short plunge.
# The exact regeneration command is written into each file's header. Committed instances:
#   python3 tools/gen_bench_diag.py --mode diag_cut  --out gcode/bench/diag_cut.gcode
#   python3 tools/gen_bench_diag.py --mode air_moves --safe-z 600 --hops 2 \
#       --out gcode/bench/air_moves.gcode
#   ./release/autocam simulate --gcode gcode/bench/diag_cut.gcode \
#       --tool test/hemispheric_mill_3.bin --no-view            # (see tools/bench_swept.sh)
# ---------------------------------------------------------------------------
import argparse, math

SQRT2 = math.sqrt(2.0)

def uv_to_xy(u, v):
    # u_hat = (1,1)/sqrt2, v_hat = (1,-1)/sqrt2  ->  (x,y) = u*u_hat + v*v_hat
    return ((u + v) / SQRT2, (u - v) / SQRT2)

def build(mode, n, U, vmax, cut_z, safe_z, feed, cut_half, hops):
    cmd = (f"--mode {mode} --n {n} --U {U:g} --vmax {vmax:g} --cut-z {cut_z:g} --safe-z {safe_z:g}"
           + (f" --cut-half {cut_half:g} --hops {hops}" if mode == "air_moves" else ""))
    out = ["%",
           f"(diagonal stress benchmark: {mode}; rotated basis u=(1,1)/2^.5 v=(1,-1)/2^.5)",
           f"(regenerate: python3 tools/gen_bench_diag.py {cmd} --out <this file>)",
           f"(voxel units; Z{int(cut_z)}=cut Z{int(safe_z)}=safe/air)",
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
    else:  # air_moves: short cuts + long diagonal in-air rapids (the air dominates the baseline)
        for k, v in enumerate(vs):
            uc = -U if k % 2 == 0 else U                 # far end alternates -> long air hop
            xa, ya = uv_to_xy(uc - cut_half, v); xb, yb = uv_to_xy(uc + cut_half, v)
            out.append(f"G0 Z{safe_z:.3f}")              # retract to clearance (tool out of stock)
            for h in range(hops):                        # extra full-diagonal in-air rapids
                xh, yh = uv_to_xy(U if h % 2 == 0 else -U, v)
                out.append(f"G0 X{xh:.3f} Y{yh:.3f}")
            out += [f"G0 X{xa:.3f} Y{ya:.3f}",           # final in-air reposition to the cut start
                    f"G0 Z{cut_z:.3f}", f"G1 Z{cut_z:.3f} F{feed:.3f}",
                    f"G1 X{xb:.3f} Y{yb:.3f} F{feed:.3f}"]  # short diagonal cut
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
    ap.add_argument("--cut-half", type=float, default=40.0, help="half length of the short cut (air_moves only)")
    ap.add_argument("--hops", type=int, default=2, help="extra full-diagonal in-air rapids per lane (air_moves only)")
    a = ap.parse_args()
    with open(a.out, "w") as f:
        f.write(build(a.mode, a.n, a.U, a.vmax, a.cut_z, a.safe_z, a.feed, a.cut_half, a.hops))
    print(f"wrote {a.out}")

if __name__ == "__main__":
    main()
