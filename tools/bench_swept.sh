#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# bench_swept.sh — Table 1: Phase-2 tube-pruning on adversarial stress benchmarks.
#
# Confronta, con lo STESSO binario, il carving:
#   - baseline (S2)   : AUTOCAM_SWEPT_SKIP=0  (riscrive ogni colonna dell'AABB)
#   - tube-prune (S3) : AUTOCAM_SWEPT_SKIP=1  (salta le colonne che l'utensile non tocca)
#
# Tre righe: un controllo assi-allineato (dove AABB == tubo, atteso ~1x) e due
# micro-benchmark diagonali generati da tools/gen_bench_diag.py (base ruotata):
# diag_cut (tagli diagonali lunghi) e air_moves (rapid G0 diagonali in aria).
#
#   cd ~/Documents/development/autocam
#   bash tools/bench_swept.sh
#
# NB: autocam usa EGL/Wayland -> GPU Intel reale (`iris`). `glxinfo` invece parla
# GLX/X11 e mostra llvmpipe: NON è quello che usa autocam. Lo script rileva il
# driver vero dal log EGL.
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/.." || exit 1     # repo root

BIN=./release/autocam
N=10                                   # run per config (la prima si scarta, warm-up); media delle restanti

# benchmark:  "nome | gcode | tool"
BENCHES=(
  "square_600 (axis-aligned control)         | gcode/square_600.gcode     | test/hemispheric_mill_10.bin"
  "diag_cut (repeated long diagonal cuts)    | gcode/bench/diag_cut.gcode  | test/hemispheric_mill_3.bin"
  "air_moves (repeated diagonal G0 rapids)   | gcode/bench/air_moves.gcode | test/hemispheric_mill_3.bin"
)

if [ ! -x "$BIN" ]; then echo "Manca $BIN (compila prima)."; exit 1; fi

# --- GPU realmente usata da autocam (NON glxinfo) --------------------------
echo "=== GPU realmente usata da autocam ==="
DRV=$(EGL_LOG_LEVEL=debug "$BIN" simulate --gcode gcode/square_600.gcode --no-view 2>&1 \
      | sed -n 's/.*pci id for fd [0-9]*: \([0-9a-fx:]*\), driver \([a-z_]*\).*/\2 (\1)/p' | head -1)
echo "driver: ${DRV:-non rilevato}"
case "$DRV" in
  ""|*swrast*|*llvmpipe*|*softpipe*)
    echo "!!! ATTENZIONE: driver software o non rilevato — i tempi NON valgono. !!!" ;;
  *) echo "OK: GPU hardware." ;;
esac
echo

# --- 'carving netto' baseline vs skip: MEDIA su N run, INTERLEAVATA per annullare il ---
# --- drift di clock/thermal (una run per modo a ogni giro; primo giro = warm-up scartato).
one_net () {  # $1 gcode  $2 tool  $3 skip -> "carving netto" di UNA run
  AUTOCAM_SWEPT_SKIP="$3" "$BIN" simulate --gcode "$1" --tool "$2" --no-view 2>/dev/null \
    | sed -n 's/.*carving netto \([0-9.]*\) ms.*/\1/p'
}
bench_one () {  # $1 gcode  $2 tool -> "meanBaseline meanSkip" (media aritmetica su N run)
  local sB=0 sA=0 nB=0 nA=0 t0 t1
  for i in $(seq 0 "$N"); do                       # giro 0 = warm-up (scartato)
    t0=$(one_net "$1" "$2" 0)
    t1=$(one_net "$1" "$2" 1)
    [ "$i" -eq 0 ] && continue
    [ -n "$t0" ] && { sB=$(awk "BEGIN{print $sB+$t0}"); nB=$((nB+1)); }
    [ -n "$t1" ] && { sA=$(awk "BEGIN{print $sA+$t1}"); nA=$((nA+1)); }
  done
  awk "BEGIN{printf \"%s %s\", ($nB?$sB/$nB:\"NA\"), ($nA?$sA/$nA:\"NA\")}"
}

printf "%-40s %11s %11s %9s\n" "benchmark" "baseline" "tube-prune" "speedup"
printf "%-40s %11s %11s %9s\n" "" "(SKIP=0)" "(SKIP=1)" ""
printf -- "%.0s-" {1..74}; echo
for e in "${BENCHES[@]}"; do
  IFS='|' read -r name gc tool <<<"$e"
  name=$(echo "$name" | sed 's/[[:space:]]*$//'); gc=$(echo "$gc" | xargs); tool=$(echo "$tool" | xargs)
  read -r b a < <(bench_one "$gc" "$tool")
  if [ "$b" = NA ] || [ "$a" = NA ]; then sp="?"; b2="$b"; a2="$a"; else
    sp=$(awk "BEGIN{printf \"%.2fx\", $b/$a}")
    b2=$(awk "BEGIN{printf \"%.2f\", $b}"); a2=$(awk "BEGIN{printf \"%.2f\", $a}"); fi
  printf "%-40s %8s ms %8s ms %9s\n" "$name" "$b2" "$a2" "$sp"
done
echo "media aritmetica di $N run (prima scartata, warm-up); netto = carving escl. read-back."
echo "square_600 = controllo assi-allineato (atteso ~1x: AABB già = tubo)."
echo "diag_cut   = tagli diagonali lunghi (AABB >> tubo, atteso >1x)."
echo "air_moves  = rapid G0 diagonali in aria (dispatch intero saltato, atteso >>1x)."
echo "raster45 (Table 3) e' il raster misto degli stessi diagonali: vedi bench_matrix.sh."
