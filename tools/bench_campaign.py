#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# bench_campaign.py — full publication measurement campaign, one command.
#
# Produces DOCS/benchmark-campaign.md with, all interleaved under the paper
# protocol (first run of N discarded, mean +/- population sd of the rest):
#   Table A  square_600 ablation (mill_10): S0/S1/S2/S3, netto AND totale
#   Table B  square_600 S2/S3 high-N (mill_10): settles the pruning ratio
#   Table 1  stress diag_cut / air_moves (mill_3): S2/S3, netto AND totale
#   Table 3  cross-workload matrix (mill_3): S0/S1/S2/S3
#   Table 5  zero-fill lever (mill_3)
#
# RUN ON A GENUINELY IDLE MACHINE (load < 1). Close the Claude host app, the
# editor, the browser — everything but the terminal you launch this from. The
# GPU is the Intel iGPU via EGL/Wayland, so stay in your desktop session (do not
# drop to a bare TTY, or there is no Wayland display and no hardware GL).
#
#   cd ~/Documents/development/autocam
#   uptime                       # wait until load average is < 1
#   python3 tools/bench_campaign.py
# ---------------------------------------------------------------------------
import subprocess, re, statistics, os, time, datetime, sys

os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/..")   # repo root
BIN = "./release/autocam"; M10 = "test/hemispheric_mill_10.bin"; M3 = "test/hemispheric_mill_3.bin"
MD = "DOCS/benchmark-campaign.md"
N, NHI = 11, 31
if not os.access(BIN, os.X_OK):
    sys.exit(f"missing {BIN} (build it first, see .vscode/tasks.json)")
def load(): return open("/proc/loadavg").read().split()[0]
if float(load()) > 1.5:
    print(f"WARNING: load average is {load()} — results will be noisy. Idle the machine first.", file=sys.stderr)

RE = re.compile(r'carving netto ([0-9.]+) ms \| totale \(incl\. copyback\) ([0-9.]+) ms')
def run_once(gcode, tool, env, flags):
    e = dict(os.environ); e.update(env or {})
    out = subprocess.run([BIN, "simulate", "--gcode", gcode, "--tool", tool, "--no-view"] + (flags or []),
                         capture_output=True, text=True, env=e).stdout
    m = RE.search(out); return (float(m.group(1)), float(m.group(2))) if m else (None, None)
def measure(configs, gcode, tool, n):
    d = {lab: {'n': [], 't': []} for lab, _, _ in configs}
    for r in range(n):
        for lab, env, flags in configs:
            net, tot = run_once(gcode, tool, env, flags)
            if r > 0 and net is not None:
                d[lab]['n'].append(net); d[lab]['t'].append(tot)
    return d
def cell(xs):
    return f"{statistics.mean(xs):.2f} ± {statistics.pstdev(xs):.2f}" if xs else "—"
def mean(d, k): return statistics.mean(d[k]['n'])

open(MD, "w").close()
def w(s): open(MD, "a").write(s + "\n")
t0 = time.time()
w("# autocam — benchmark campaign\n")
w(f"*{datetime.datetime.now():%Y-%m-%d %H:%M} · Intel iris (driver xe) · interleaved, first of N discarded, "
  f"mean ± population sd. Stock 1000×1000×500. Load at start: {load()}.*\n")
SWEPT = [("S2", {"AUTOCAM_SWEPT_SKIP": "0"}, []), ("S3", {"AUTOCAM_SWEPT_SKIP": "1"}, [])]
FULL = [("S0", {}, ["--legacy"]), ("S1", {}, ["--legacy-external-buffer"]),
        ("S2", {"AUTOCAM_SWEPT_SKIP": "0"}, []), ("S3", {"AUTOCAM_SWEPT_SKIP": "1"}, [])]
WL = ["contour", "pocket_axis", "raster45", "rapids", "localized", "finishing"]

print("Table A: square_600 ablation ...", file=sys.stderr)
d = measure(FULL, "gcode/square_600.gcode", M10, N)
w(f"## Table A — square_600 ablation (256-voxel tool `mill_10`), N={N-1}")
w("| level | netto (ms) | totale incl. read-back (ms) |\n|---|--:|--:|")
for k in ("S0", "S1", "S2", "S3"): w(f"| {k} | {cell(d[k]['n'])} | {cell(d[k]['t'])} |")
w(f"\nS2→S3 netto: **{mean(d,'S2')/mean(d,'S3'):.3f}×** · overall S0→S3 netto {mean(d,'S0')/mean(d,'S3'):.1f}× "
  f"· totale {statistics.mean(d['S0']['t'])/statistics.mean(d['S3']['t']):.1f}×\n")

print("Table B: square_600 S2/S3 high-N ...", file=sys.stderr)
d = measure(SWEPT, "gcode/square_600.gcode", M10, NHI)
w(f"## Table B — square_600 S2/S3 high-N (`mill_10`), N={NHI-1}")
w("| level | netto (ms) | totale (ms) |\n|---|--:|--:|")
for k in ("S2", "S3"): w(f"| {k} | {cell(d[k]['n'])} | {cell(d[k]['t'])} |")
w(f"\nS2→S3 netto (N={NHI-1}): **{mean(d,'S2')/mean(d,'S3'):.3f}×**\n")

print("Table 1: stress diag_cut / air_moves ...", file=sys.stderr)
w(f"## Table 1 — stress benchmarks (32-voxel tool `mill_3`), N={N-1}")
w("| program | S2 netto | S3 netto | S2→S3 | S2 totale | S3 totale |\n|---|--:|--:|--:|--:|--:|")
for prog in ("diag_cut", "air_moves"):
    d = measure(SWEPT, f"gcode/bench/{prog}.gcode", M3, N)
    w(f"| {prog} | {cell(d['S2']['n'])} | {cell(d['S3']['n'])} | {mean(d,'S2')/mean(d,'S3'):.3f}× "
      f"| {cell(d['S2']['t'])} | {cell(d['S3']['t'])} |")
w("\n*(square_600 control is Table A.)*\n")

print("Table 3: cross-workload matrix ...", file=sys.stderr)
w(f"## Table 3 — cross-workload matrix (`mill_3`, netto ms), N={N-1}")
w("| workload | S0 | S1 | S2 | S3 | S1/S2 | S2/S3 | S0/S3 |\n|---|--:|--:|--:|--:|--:|--:|--:|")
for wl in WL:
    d = measure(FULL, f"gcode/bench/{wl}.gcode", M3, N)
    w(f"| {wl} | {cell(d['S0']['n'])} | {cell(d['S1']['n'])} | {cell(d['S2']['n'])} | {cell(d['S3']['n'])} "
      f"| {mean(d,'S1')/mean(d,'S2'):.2f}× | {mean(d,'S2')/mean(d,'S3'):.2f}× | {mean(d,'S0')/mean(d,'S3'):.1f}× |")

print("Table 5: zero-fill lever ...", file=sys.stderr)
ZF = [("S2f", {"AUTOCAM_SWEPT_SKIP": "0", "AUTOCAM_SWEPT_ZEROFILL": "1"}, []),
      ("S2n", {"AUTOCAM_SWEPT_SKIP": "0", "AUTOCAM_SWEPT_ZEROFILL": "0"}, []),
      ("S3f", {"AUTOCAM_SWEPT_SKIP": "1", "AUTOCAM_SWEPT_ZEROFILL": "1"}, []),
      ("S3n", {"AUTOCAM_SWEPT_SKIP": "1", "AUTOCAM_SWEPT_ZEROFILL": "0"}, [])]
w(f"## Table 5 — zero-fill lever (`mill_3`, netto ms), N={N-1}")
w("| workload | S2 fill | S2 no-fill | gain | S3 fill | S3 no-fill | gain |\n|---|--:|--:|--:|--:|--:|--:|")
for wl in WL:
    d = measure(ZF, f"gcode/bench/{wl}.gcode", M3, N)
    g2 = (mean(d, 'S2n')/mean(d, 'S2f')-1)*100; g3 = (mean(d, 'S3n')/mean(d, 'S3f')-1)*100
    w(f"| {wl} | {cell(d['S2f']['n'])} | {cell(d['S2n']['n'])} | {g2:+.1f}% "
      f"| {cell(d['S3f']['n'])} | {cell(d['S3n']['n'])} | {g3:+.1f}% |")
w(f"\n*gain = (no-fill/fill − 1), negative = faster. Load at end: {load()}. Wall time: {time.time()-t0:.0f}s.*")
print(f"done in {time.time()-t0:.0f}s -> {MD}", file=sys.stderr)
