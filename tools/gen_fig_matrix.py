#!/usr/bin/env python3
# Generate DOCS/figures/fig9_matrix.svg — the cross-workload carving-speed matrix,
# now FOUR refinement levels (log scale): S0 stamping, S1 swept/external-buffer,
# S2 swept/fused-in-place (this work's fusion), S3 +tube-pruning+air-skip (this work).
# Data = tools/bench_matrix.sh, carving netto (ms), mean of ten interleaved campaigns
# (§5.4; 10×10 carves, cross-campaign CV <= 6%), Intel iris.
# Zero-dependency (stdlib only). Re-run after re-benchmarking, then convert to EMF:
#   inkscape <abs>/fig9_matrix.svg --export-type=emf --export-filename=<abs>/fig9_matrix.emf
import math, os

# (name, sublabel, S0, S1_extbuf, S2_fused, S3_prune)
DATA = [
    ("contour",     "axis perimeter",   51.47, 10.00, 4.01,  4.08),
    ("pocket_axis", "axis raster",      151.22, 17.81, 8.76,  9.25),
    ("raster45",    "45° diagonal",     264.75, 43.88, 35.59, 14.79),
    ("rapids",      "air-heavy",         81.00, 17.41, 12.76, 6.63),
    ("localized",   "corner feature",    39.00, 13.46, 5.77,  4.92),
    ("finishing",   "fine raster",      111.57, 15.36, 7.60,  8.18),
]

# Log-scale x mapping: px(v) = X0 + log10(v)*DEC  (1 ms -> X0, one decade -> DEC px)
X0, DEC = 140.0, 300.0
def px(v): return X0 + math.log10(v) * DEC

COL = {"S0": "#a0aec0", "S1": "#ed8936", "S2": "#3182ce", "S3": "#38a169"}
TXT = {"S0": "#4a5568", "S1": "#9c4221", "S2": "#2c5282", "S3": "#276749"}
BH, GAP = 10, 2                      # bar height, gap between bars in a group
GH = 4 * (BH + GAP) + 16             # group height (4 bars + label)
TOP = 96
W, H = 900, int(TOP + len(DATA) * GH + 40)

out = []
out.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="Arial, Helvetica, sans-serif">')
out.append(f'<rect x="0" y="0" width="{W}" height="{H}" fill="#ffffff"/>')
out.append('<text x="30" y="28" font-size="18" font-weight="bold" fill="#1a202c">Carving time per workload &#215; refinement level (net carving, log scale)</text>')
out.append('<text x="30" y="47" font-size="12" fill="#4a5568">S0&#8594;S1 established per-move swept; <tspan font-weight="bold">S1&#8594;S2 = fusion / no external buffer (this work, halves memory)</tspan>; S2&#8594;S3 = pruning (this work).</text>')
# legend
leg = [("S0", "stamping [van Hook]", 30), ("S1", "swept, external buffer", 205),
       ("S2", "swept, fused in-place [ours]", 400), ("S3", "+pruning [ours]", 650)]
for key, lab, x in leg:
    out.append(f'<rect x="{x}" y="60" width="13" height="13" fill="{COL[key]}"/>'
               f'<text x="{x+18}" y="71" font-size="12" fill="#1a202c">{key} {lab}</text>')

# log gridlines
ticks = [1, 3, 10, 30, 100, 200]
for t in ticks:
    x = px(t)
    out.append(f'<line x1="{x:.1f}" y1="{TOP-6}" x2="{x:.1f}" y2="{H-34}" stroke="#e2e8f0" stroke-width="1"/>')
    out.append(f'<text x="{x:.1f}" y="{H-20}" font-size="10.5" fill="#718096" text-anchor="middle">{t}</text>')
out.append(f'<text x="{px(31):.1f}" y="{H-6}" font-size="11" fill="#718096" text-anchor="middle">carving netto (ms, log scale)</text>')

for gi, (name, sub, s0, s1, s2, s3) in enumerate(DATA):
    gy = TOP + gi * GH
    out.append(f'<text x="134" y="{gy+13}" font-size="11.5" font-weight="bold" fill="#1a202c" text-anchor="end">{name}</text>')
    out.append(f'<text x="134" y="{gy+25}" font-size="9" fill="#718096" text-anchor="end">{sub}</text>')
    rows = [("S0", s0, ""), ("S1", s1, ""),
            ("S2", s2, f"· {s1/s2:.2f}× fuse"),
            ("S3", s3, f"· {s2/s3:.2f}× prune")]
    for ri, (key, val, note) in enumerate(rows):
        y = gy + ri * (BH + GAP)
        x2 = px(val)
        out.append(f'<rect x="{X0:.1f}" y="{y}" width="{max(x2-X0,1):.1f}" height="{BH}" fill="{COL[key]}"/>')
        bold = ' font-weight="bold"' if (key in ("S2", "S3") and note and "fuse" in note and s1/s2 >= 2.0) else ''
        label = f"{val:.1f} {note}".rstrip()
        out.append(f'<text x="{x2+5:.1f}" y="{y+BH-1.5:.1f}" font-size="9.5" fill="{TXT[key]}"{bold}>{label}</text>')

out.append('</svg>')
svg = "\n".join(out) + "\n"

here = os.path.dirname(os.path.abspath(__file__))
dst = os.path.join(here, "..", "DOCS", "FIGURES", "fig9_matrix.svg")
with open(dst, "w") as f:
    f.write(svg)
print("wrote", os.path.normpath(dst))
