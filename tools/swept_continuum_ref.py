#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# swept_continuum_ref.py -- exact continuum reference for the fused swept-tool
# subtraction (shaders/subtract_swept.comp), for ONE linear toolpath segment.
#
# WHAT IT COMPUTES
#   The removed volume of the true SWEPT solid, analytically in t: for every stock
#   column (gx,gy) the lateral distance to the moving tool axis is a quadratic in the
#   interpolation parameter t in [0,1]; we minimise it in closed form (clamped to
#   [0,1]) to get the closest approach d_min, and remove the cap depth D(d_min) there.
#   No sampling in t. This is the continuum the K-sampled kernel approximates, and it
#   lives on the SAME stock grid as the kernel -- unlike the --legacy stamper, which
#   sits on the Euclidean+truncation lattice and conflates lattice mismatch with the
#   sampling gap. Pure stdlib, no GPU dependency.
#
# THREE CONVENTIONS MATCHED FROM THE KERNEL (so the residual is the sampling gap,
# not a modelling mismatch):
#   (a) tool datum via zShift. The kernel puts the tool BBOX centre at
#       tr = (w1/2+off.x, h1/2+off.y, z1/2-off.z) and shifts the tool in Z by
#       zShift = tr.z - z2/2 (src/boolOps.cpp subtractSwept; shaders/subtract_swept.comp).
#       The removed depth of a covered column is the tool column's highest transition
#       plus zShift; we read that datum, we do not assume a tip at z=0.
#   (b) the depth profile is taken from the TOOL .bin, not an ideal sphere. The GPU
#       voxeliser is a slice rasteriser (no center-in-solid test), and its cap profile
#       differs from round(sqrt(R^2-r^2)) by ~1-2%. We build a continuous radial depth
#       D(r) by averaging the tool's own per-column cap depth in radial rings, so the
#       continuum and the kernel share one profile and the tool-shape term cancels in
#       the gap.
#   (c) glm::round endpoints. VOXEL-mode carveSwept rounds p0,p1 to integer offsets
#       (gcodeViewer.cpp) before off->tr; we take integer tStart/tEnd the same way.
#
# HOW TO RUN (from the repo root; needs test/workpiece_100_100_50.bin and the tool .bin):
#   python3 tools/swept_continuum_ref.py
#   # optional: validate the kernel emulation byte-for-byte against saved GPU outputs
#   python3 tools/swept_continuum_ref.py --gpu-dir <dir with {axis,diag}_swept.bin>
# ---------------------------------------------------------------------------
import struct, array, math, collections, os, sys

# ---------- .bin I/O (AVOX: 12B header + params + 2xu64 sizes + comp + prefix) ----------
def load_bin(path):
    d = open(path, 'rb').read(); assert d[:4] == b'AVOX', f"{path}: not an AVOX file"
    pb = struct.unpack_from('<I', d, 8)[0]
    rx, ry, rz = struct.unpack_from('<iii', d, 16)
    off = 12 + pb
    db = struct.unpack_from('<Q', d, off)[0]; off += 8
    pfb = struct.unpack_from('<Q', d, off)[0]; off += 8
    comp = array.array('I'); comp.frombytes(d[off:off+db]); off += db
    pref = array.array('I'); pref.frombytes(d[off:off+pfb])
    return (rx, ry, rz), comp, pref

def solid_volume(path):
    _, comp, pref = load_bin(path)
    L = len(comp); n = len(pref); t = 0
    for i in range(n):
        s = pref[i]; e = pref[i+1] if i+1 < n else L; sg = -1; k = s
        while k < e:
            t += -comp[k] if sg < 0 else comp[k]; sg = -sg; k += 1
    return t

# ---------- tool: cap-depth profile straight from the .bin (conventions a,b) ----------
def tool_profile(tool_path, z1, offz):
    (w2, h2, z2), comp, pref = load_bin(tool_path)
    L = len(comp); n = len(pref); cx, cy = w2//2, h2//2
    zshift = (z1//2 - offz) - z2//2                 # (a) tr.z - z2/2
    depth = {}                                       # (dx,dy) -> removed cap depth (voxels)
    rings = collections.defaultdict(list)
    for y in range(h2):
        for x in range(w2):
            i = x + y*w2; s = pref[i]; e = pref[i+1] if i+1 < n else L
            if e <= s: continue
            d = min(comp[e-1] + zshift, z1)          # ztMax in stock coords (zbMin<=0 clamped)
            if d > 0:
                depth[(x-cx, y-cy)] = d
                rings[round(math.hypot(x-cx, y-cy))].append(d)
    prof = sorted((r, sum(v)/len(v)) for r, v in rings.items())   # (b) radial profile
    rmax = max(math.hypot(dx, dy) for (dx, dy) in depth)
    def Dr(r):                                        # continuous, piecewise-linear D(r)
        if r > prof[-1][0]: return 0.0
        for i in range(len(prof)-1):
            r0, d0 = prof[i]; r1, d1 = prof[i+1]
            if r <= r1: return d0 + (d1-d0)*(r-r0)/(r1-r0) if r1 > r0 else d0
        return prof[-1][1]
    return depth, Dr, rmax, zshift, z2

# ---------- geometry ----------
def dmin2(gx, gy, sx, sy, dx, dy):
    """min over t in [0,1] of |(gx,gy)-((sx,sy)+t(dx,dy))|^2 (quadratic, clamped)."""
    a = dx*dx + dy*dy
    t = 0.0 if a == 0 else ((gx-sx)*dx + (gy-sy)*dy) / a
    t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
    ex = gx - (sx + t*dx); ey = gy - (sy + t*dy)
    return ex*ex + ey*ey

def _bbox(sx, sy, ex, ey, R):
    m = int(math.ceil(R)) + 2
    return (int(min(sx, ex))-m, int(max(sx, ex))+m, int(min(sy, ey))-m, int(max(sy, ey))+m)

def continuum(sx, sy, ex, ey, Dr, R):
    """Exact continuum removal: D(d_min) over covered columns, continuous t."""
    dx, dy = ex-sx, ey-sy; x0, x1, y0, y1 = _bbox(sx, sy, ex, ey, R); R2 = (R+0.5)**2
    tot = 0.0
    for gy in range(y0, y1+1):
        for gx in range(x0, x1+1):
            d2 = dmin2(gx, gy, sx, sy, dx, dy)
            if d2 <= R2: tot += Dr(math.sqrt(d2))
    return tot

def kernel_emulate(sx, sy, ex, ey, R, depth_fn, K):
    """subtract_swept.comp emulation: max over K-sampled t = round(k/K*delta). O(K*R^2)."""
    dx, dy = ex-sx, ey-sy; x0, x1, y0, y1 = _bbox(sx, sy, ex, ey, R)
    W = x1-x0+1; H = y1-y0+1
    grid = array.array('d', bytes(8*W*H))
    m = int(math.ceil(R))
    disk = [(ddx, ddy, depth_fn(ddx, ddy))
            for ddy in range(-m, m+1) for ddx in range(-m, m+1) if depth_fn(ddx, ddy) > 0]
    for k in range(K+1):
        t = k/K if K else 0.0
        base = (round(sy + t*dy)-y0)*W + (round(sx + t*dx)-x0)
        for (ddx, ddy, dep) in disk:
            idx = base + ddy*W + ddx
            if grid[idx] < dep: grid[idx] = dep
    return math.fsum(grid)

# ==================== main ====================
if __name__ == "__main__":
    gpu_dir = None
    if "--gpu-dir" in sys.argv:
        gpu_dir = sys.argv[sys.argv.index("--gpu-dir")+1]
    STOCK = "test/workpiece_100_100_50.bin"; TOOL = "test/hemispheric_mill_3.bin"
    z1, offz = 500, 500
    depth, Dr, R, zshift, z2 = tool_profile(TOOL, z1, offz)
    # segments: integer tStart/tEnd via off->tr (conv. c): axis and 45deg, both L=600 vox
    SEGS = {"axis": (200, 500, 800, 500), "diag": (200, 200, 800, 800)}
    K = 600

    print(f"tool={TOOL}  z2={z2}  zShift={zshift}  R={R:.2f}  (conventions a,b,c matched)\n")
    depf = lambda dx, dy: Dr(math.hypot(dx, dy))

    print(f"{'seg':<6}{'continuum':>13}{'kernel':>13}{'abs diff':>11}{'gap %':>9}  sign")
    for name, (sx, sy, ex, ey) in SEGS.items():
        cont = continuum(sx, sy, ex, ey, Dr, R)
        kern = kernel_emulate(sx, sy, ex, ey, R, depf, K)
        diff = cont - kern; pct = diff/cont*100.0
        sign = "kernel under-removes" if diff > 1e-6 else ("kernel over-removes" if diff < -1e-6 else "exact")
        print(f"{name:<6}{cont:>13.1f}{kern:>13.1f}{diff:>11.1f}{pct:>8.3f}%  {sign}")

    if gpu_dir:  # optional: certify the emulation is byte-exact vs the real GPU kernel
        print("\nemulation vs GPU (tool 2D profile, K=600):")
        base = solid_volume(STOCK); depf2 = lambda dx, dy: depth.get((dx, dy), 0)
        for name, (sx, sy, ex, ey) in SEGS.items():
            p = os.path.join(gpu_dir, f"{name}_swept.bin")
            if not os.path.exists(p): p = os.path.join(gpu_dir, f"f{name}_swept.bin")
            if os.path.exists(p):
                emu = kernel_emulate(sx, sy, ex, ey, R, depf2, K); gpu = base - solid_volume(p)
                print(f"  {name}: emulated={emu:.0f}  GPU={gpu}  ({(emu/gpu-1)*100:+.3f}%)")

    # resolution sweep: finer voxels = larger R and L together (voxel size ~ 1/R). The
    # gap is profile-robust (ratio cancels scale), so an analytic hemisphere is used here.
    print("\nresolution sweep (gap = continuum vs K-sampled kernel; analytic hemisphere):")
    print(f"  {'voxel':>7}{'R':>4}{'L':>6}{'axis gap%':>12}{'45deg gap%':>13}")
    for vox, R2, L in [("2.0x", 8, 300), ("1.0x", 16, 600), ("0.5x", 32, 1200), ("0.33x", 48, 1800)]:
        Da = lambda r, RR=R2: math.sqrt(max(0.0, RR*RR - r*r))
        df = lambda dx, dy, RR=R2: (math.sqrt(RR*RR-(dx*dx+dy*dy)) if dx*dx+dy*dy <= RR*RR else 0.0)
        row = []
        for mode in ("axis", "diag"):
            sx, sy = 500-L//2, 500-(L//2 if mode == "diag" else 0)
            ex, ey = 500+L//2, 500+(L//2 if mode == "diag" else 0)
            c = continuum(sx, sy, ex, ey, Da, float(R2)); k = kernel_emulate(sx, sy, ex, ey, float(R2), df, L)
            row.append((c-k)/c*100.0)
        print(f"  {vox:>7}{R2:>4}{L:>6}{row[0]:>11.3f}%{row[1]:>12.3f}%")
