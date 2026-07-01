#!/usr/bin/env python3
"""
Generate G-code carving programs for the autocam voxel simulator.

Coordinates are in STOCK VOXEL UNITS (like gcode/square_600.gcode): the default
`autocam simulate` runs in `--gcode-units voxel`, where 1 unit == 1 stock voxel and
the stock centre is the origin. The 100x100x50 mm test stock is 1000x1000x500
voxels, so the usable XY envelope is about +-460 (leaving room for the tool radius).

Z convention (calibrated empirically for test/hemispheric_mill_3.bin):
  LOWER Z = DEEPER.  ~Z550 grazes the surface, Z0 is the deepest, Z700 clears the
  stock (safe). The simulator carves rapids too, so every reposition first retracts
  to the safe Z. For a different tool, re-probe (plunge at a few Z and measure).

Examples:
  # the committed demo (4 features, one per quadrant)
  python3 tools/gen_gcode.py --out gcode/complex_demo.gcode

  # only some features
  python3 tools/gen_gcode.py --out p.gcode --features pocket,spiral

  # a reproducible random variant (for building a dataset of carvings)
  python3 tools/gen_gcode.py --out v07.gcode --seed 7 --randomize

  # a batch of random variants
  for s in $(seq 1 20); do
    python3 tools/gen_gcode.py --out "ds/run_$s.gcode" --seed "$s" --randomize
  done

  # physical millimetres (for a resolution-consistent fitness set — no tool/stock warning)
  python3 tools/gen_gcode.py --out gcode/complex_demo_mm.gcode --units mm

Run a generated file with:
  # voxel units (legacy sample stock/tool):
  autocam simulate --gcode <file>.gcode --tool test/hemispheric_mill_3.bin --mesh --mesh-step 2

  # mm units against a resolution-consistent set (stock+tool+target all at the same --res):
  autocam fitness --gcode gcode/complex_demo_mm.gcode --gcode-units mm \
      --workpiece test/workpiece_100_100_50_r05.bin \
      --tool test/hemispheric_mill_3_r05.bin \
      --target test/workpiece_100_100_50_target_r05.bin
"""

import argparse
import math
import random

ALL_FEATURES = ["holes", "pocket", "spiral", "star"]


class GcodeBuilder:
    """Accumulates G-code lines with the safe-retract carving convention baked in.

    All feature geometry is authored in the historical VOXEL-unit frame (1 unit == 1
    stock voxel of the 0.1 mm test stock). `uscale` multiplies every emitted X/Y/Z:
      - voxel units:  uscale = 1.0  (unchanged, run with `--gcode-units voxel`)
      - millimetres:  uscale = 0.1  (physical mm, run with `--gcode-units mm`) — this
        works because the reference stock is 0.1 mm/voxel, so voxel_index * 0.1 == mm.
    The mm output is resolution-independent: it carves the same physical shape on a
    stock voxelized at any --res, provided the tool shares that voxel size."""

    def __init__(self, safe_z, feed, uscale=1.0):
        self.safe = safe_z
        self.feed = feed
        self.us = uscale
        # 1 decimal is enough for voxel indices; mm (uscale<1) needs 3 to keep the
        # sub-voxel spiral/star geometry (and so voxel output stays byte-identical).
        self.dec = 1 if uscale == 1.0 else 3
        self.lines = []

    def comment(self, s):
        self.lines.append(f"({s})")

    def retract(self):
        self.lines.append(f"G0 Z{self.safe * self.us:.{self.dec}f}")

    def rapid(self, x, y):
        self.lines.append(f"G0 X{x * self.us:.{self.dec}f} Y{y * self.us:.{self.dec}f}")

    def plunge(self, z):
        self.lines.append(f"G1 Z{z * self.us:.{self.dec}f} F{self.feed}")

    def cut(self, x, y, z):
        self.lines.append(f"G1 X{x * self.us:.{self.dec}f} Y{y * self.us:.{self.dec}f} Z{z * self.us:.{self.dec}f} F{self.feed}")

    def text(self):
        return "\n".join(self.lines) + "\n"


# --- Feature generators (all start and end at the safe height) ------------------

def feature_holes(g, region, rows, cols, depths):
    """A rows x cols grid of plunged holes; depth varies per row (cycles `depths`)."""
    x0, x1, y0, y1 = region
    g.comment(f"holes {rows}x{cols} region={_r(region)} depths={depths}")
    ys = _linspace(y0, y1, rows)
    xs = _linspace(x0, x1, cols)
    for i, Y in enumerate(ys):
        z = depths[i % len(depths)]
        for X in xs:
            g.retract()
            g.rapid(X, Y)
            g.plunge(z)
    g.retract()


def feature_pocket(g, region, depth, step):
    """A rectangular pocket cleared with a boustrophedon (zig-zag) raster."""
    x0, x1, y0, y1 = region
    g.comment(f"raster pocket region={_r(region)} Z{depth} step{step}")
    g.retract()
    g.rapid(x0, y0)
    g.plunge(depth)
    y = float(y0)
    to_right = True
    while y <= y1 + 1e-6:
        g.cut(x1 if to_right else x0, y, depth)  # cut across this row
        y_next = y + step
        if y_next <= y1 + 1e-6:
            g.cut(x1 if to_right else x0, y_next, depth)  # step over in Y at the same end
        y = y_next
        to_right = not to_right
    g.retract()


def feature_spiral(g, center, turns, rmax, depth, per_turn=48):
    """An Archimedean spiral groove from the centre outward."""
    cx, cy = center
    g.comment(f"spiral center={_r(center)} turns={turns} rmax={rmax} Z{depth}")
    g.retract()
    g.rapid(cx, cy)
    g.plunge(depth)
    n = max(1, int(turns * per_turn))
    for i in range(1, n + 1):
        t = i / n
        ang = turns * 2 * math.pi * t
        r = rmax * t
        g.cut(cx + r * math.cos(ang), cy + r * math.sin(ang), depth)
    g.retract()


def feature_star(g, center, points, r_out, r_in, depth):
    """A `points`-pointed star outline (alternating outer/inner radius)."""
    cx, cy = center
    g.comment(f"star center={_r(center)} points={points} R={r_out}/{r_in} Z{depth}")
    pts = []
    for k in range(2 * points + 1):  # +1 to close the outline
        ang = math.pi / 2 + k * math.pi / points
        r = r_out if k % 2 == 0 else r_in
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    g.retract()
    g.rapid(pts[0][0], pts[0][1])
    g.plunge(depth)
    for x, y in pts[1:]:
        g.cut(x, y, depth)
    g.retract()


# --- Helpers --------------------------------------------------------------------

def _linspace(a, b, n):
    if n <= 1:
        return [0.5 * (a + b)]
    return [a + (b - a) * i / (n - 1) for i in range(n)]


def _r(t):
    return "(" + ",".join(f"{v:.0f}" for v in t) + ")"


# --- Default ("demo") parameters: reproduce gcode/complex_demo.gcode ------------

def demo_params(env):
    """Fixed parameters for the 4-quadrant demo (env = half XY envelope)."""
    return {
        "holes": dict(region=(-400, -150, 150, 400), rows=3, cols=3, depths=[250, 150, 50]),
        "pocket": dict(region=(100, 420, -420, -120), depth=300, step=25),
        "spiral": dict(center=(300, 300), turns=4, rmax=150, depth=350),
        "star": dict(center=(-280, -280), points=5, r_out=150, r_in=60, depth=250),
    }


def random_params(rng, env):
    """Randomised parameters within the XY envelope (one feature per quadrant)."""
    # quadrant sign per feature: holes TL, spiral TR, pocket BR, star BL
    def quad_center(sx, sy, margin):
        # a centre comfortably inside the (sx,sy) quadrant
        lo, hi = margin, env - margin
        return (sx * rng.uniform(lo, hi), sy * rng.uniform(lo, hi))

    # holes (top-left)
    hx, hy = quad_center(-1, +1, 120)
    rows = rng.randint(2, 4)
    cols = rng.randint(2, 4)
    span = rng.uniform(120, 240)
    holes = dict(region=(hx - span / 2, hx + span / 2, hy - span / 2, hy + span / 2),
                 rows=rows, cols=cols,
                 depths=[rng.choice([50, 120, 200, 300, 400]) for _ in range(rows)])

    # pocket (bottom-right)
    px, py = quad_center(+1, -1, 160)
    pw = rng.uniform(160, 300)
    ph = rng.uniform(160, 300)
    pocket = dict(region=(px - pw / 2, px + pw / 2, py - ph / 2, py + ph / 2),
                  depth=rng.choice([200, 250, 300, 350]), step=rng.choice([20, 25, 30]))

    # spiral (top-right)
    sx, sy = quad_center(+1, +1, 180)
    spiral = dict(center=(sx, sy), turns=rng.randint(2, 6),
                  rmax=rng.uniform(90, 170), depth=rng.choice([200, 300, 350, 400]))

    # star (bottom-left)
    tx, ty = quad_center(-1, -1, 180)
    ro = rng.uniform(100, 170)
    star = dict(center=(tx, ty), points=rng.randint(5, 8),
                r_out=ro, r_in=ro * rng.uniform(0.35, 0.55), depth=rng.choice([150, 250, 300]))

    return {"holes": holes, "pocket": pocket, "spiral": spiral, "star": star}


def clamp_params(p, env):
    """Clamp feature coordinates so the tool stays inside the stock envelope."""
    def cl(v):
        return max(-env, min(env, v))

    for f in ("holes", "pocket"):
        if f in p:
            x0, x1, y0, y1 = p[f]["region"]
            p[f]["region"] = (cl(x0), cl(x1), cl(y0), cl(y1))
    for f in ("spiral", "star"):
        if f in p:
            cx, cy = p[f]["center"]
            p[f]["center"] = (cl(cx), cl(cy))
    return p


def build(features, params, safe_z, feed, seed=None, uscale=1.0, units="voxel"):
    g = GcodeBuilder(safe_z, feed, uscale)
    g.lines.append("%")
    g.comment("autocam carving program - tool: hemispheric_mill_3, stock 100x100x50mm")
    if units == "mm":
        g.comment("mm coords; higher Z = higher above stock; Z0 = deepest; Z%.1f = safe"
                  % (safe_z * uscale))
    else:
        g.comment("voxel-unit coords (like square_600); lower Z = deeper; Z%d = safe" % int(safe_z))
    if seed is not None:
        g.comment(f"generated by tools/gen_gcode.py --seed {seed} --randomize")
    g.retract()
    g.rapid(0, 0)

    if "holes" in features:
        feature_holes(g, **params["holes"])
    if "pocket" in features:
        feature_pocket(g, **params["pocket"])
    if "spiral" in features:
        feature_spiral(g, **params["spiral"])
    if "star" in features:
        feature_star(g, **params["star"])

    g.rapid(0, 0)
    g.lines.append("%")
    return g.text()


def main():
    ap = argparse.ArgumentParser(description="Generate autocam carving G-code.")
    ap.add_argument("--out", required=True, help="output .gcode path")
    ap.add_argument("--features", default=",".join(ALL_FEATURES),
                    help="comma list of: " + ",".join(ALL_FEATURES))
    ap.add_argument("--randomize", action="store_true", help="randomise feature parameters")
    ap.add_argument("--seed", type=int, default=None, help="RNG seed (with --randomize)")
    ap.add_argument("--safe-z", type=float, default=700.0, help="safe/clearance Z (no cut), in voxel units")
    ap.add_argument("--feed", type=int, default=400, help="feed rate F")
    ap.add_argument("--env", type=float, default=460.0, help="half XY envelope in voxels")
    ap.add_argument("--units", choices=["voxel", "mm"], default="voxel",
                    help="output units: 'voxel' (legacy, 1 unit=1 stock voxel) or 'mm' (physical; "
                         "coords x0.1). Run the file with autocam's matching --gcode-units.")
    args = ap.parse_args()

    # In mm mode every coordinate is multiplied by the reference voxel size (0.1 mm),
    # turning the voxel-authored geometry into physical millimetres.
    uscale = 0.1 if args.units == "mm" else 1.0

    features = [f.strip() for f in args.features.split(",") if f.strip()]
    bad = [f for f in features if f not in ALL_FEATURES]
    if bad:
        ap.error(f"unknown feature(s): {bad}; choose from {ALL_FEATURES}")

    if args.randomize:
        rng = random.Random(args.seed)
        params = random_params(rng, args.env)
    else:
        params = demo_params(args.env)
    params = clamp_params(params, args.env)

    text = build(features, params, args.safe_z, args.feed,
                 seed=args.seed if args.randomize else None, uscale=uscale, units=args.units)
    with open(args.out, "w") as fh:
        fh.write(text)
    print(f"wrote {args.out}  ({text.count(chr(10))} lines, features={features}, units={args.units}"
          + (f", seed={args.seed}" if args.randomize else "") + ")")


if __name__ == "__main__":
    main()
