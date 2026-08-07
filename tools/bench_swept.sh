#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# bench_swept.sh — misura l'ottimizzazione "tube pruning" del carving swept.
#
# Confronta, con lo STESSO binario, il carving:
#   - baseline   : AUTOCAM_SWEPT_SKIP=0  (riscrive ogni colonna dell'AABB)
#   - tube-prune : AUTOCAM_SWEPT_SKIP=1  (salta le colonne che l'utensile non tocca)
# (Step 2, quando presente: AUTOCAM_SWEPT_TUBE=1 dispaccia solo il tubo.)
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
N=6                                    # run per config (la prima si scarta, warm-up)

# benchmark:  "nome | gcode | tool"
BENCHES=(
  "square_600  (assi-allineato, controllo) | gcode/square_600.gcode   | test/hemispheric_mill_10.bin"
  "bench_complex (raster 45°, diag.+aria)   | gcode/bench_complex.gcode | test/hemispheric_mill_3.bin"
)

if [ ! -x "$BIN" ]; then echo "Manca $BIN (compila prima)."; exit 1; fi

# --- GPU realmente usata da autocam (NON glxinfo) --------------------------
echo "=== GPU realmente usata da autocam ==="
DRV=$(EGL_LOG_LEVEL=debug "$BIN" simulate --gcode gcode/pocket_small.gcode --no-view 2>&1 \
      | sed -n 's/.*pci id for fd [0-9]*: \([0-9a-fx:]*\), driver \([a-z_]*\).*/\2 (\1)/p' | head -1)
echo "driver: ${DRV:-non rilevato}"
case "$DRV" in
  ""|*swrast*|*llvmpipe*|*softpipe*)
    echo "!!! ATTENZIONE: driver software o non rilevato — i tempi NON valgono. !!!" ;;
  *) echo "OK: GPU hardware." ;;
esac
echo

# --- min 'carving netto' baseline vs skip, INTERLEAVATO per annullare il ---
# --- drift di clock/thermal (una run per modo a ogni giro; primo giro = warm-up).
one_net () {  # $1 gcode  $2 tool  $3 skip -> "carving netto" di UNA run
  AUTOCAM_SWEPT_SKIP="$3" "$BIN" simulate --gcode "$1" --tool "$2" --no-view 2>/dev/null \
    | sed -n 's/.*carving netto \([0-9.]*\) ms.*/\1/p'
}
bench_one () {  # $1 gcode  $2 tool -> "minBaseline minSkip"
  local bB="" bA="" t0 t1
  for i in $(seq 0 "$N"); do                       # giro 0 = warm-up (scartato)
    t0=$(one_net "$1" "$2" 0)
    t1=$(one_net "$1" "$2" 1)
    [ "$i" -eq 0 ] && continue
    [ -n "$t0" ] && { [ -z "$bB" ] || awk "BEGIN{exit !($t0<$bB)}"; } && bB=$t0
    [ -n "$t1" ] && { [ -z "$bA" ] || awk "BEGIN{exit !($t1<$bA)}"; } && bA=$t1
  done
  echo "${bB:-NA} ${bA:-NA}"
}

printf "%-40s %11s %11s %9s\n" "benchmark" "baseline" "tube-prune" "speedup"
printf "%-40s %11s %11s %9s\n" "" "(SKIP=0)" "(SKIP=1)" ""
printf -- "%.0s-" {1..74}; echo
for e in "${BENCHES[@]}"; do
  IFS='|' read -r name gc tool <<<"$e"
  name=$(echo "$name" | sed 's/[[:space:]]*$//'); gc=$(echo "$gc" | xargs); tool=$(echo "$tool" | xargs)
  read -r b a < <(bench_one "$gc" "$tool")
  if [ "$b" = NA ] || [ "$a" = NA ]; then sp="?"; else sp=$(awk "BEGIN{printf \"%.2fx\", $b/$a}"); fi
  printf "%-40s %8s ms %8s ms %9s\n" "$name" "$b" "$a" "$sp"
done
echo
echo "square_600 = controllo assi-allineato (atteso ~1x: AABB già = tubo)."
echo "bench_complex = diagonali lunghe + ripos. in aria (atteso >1x)."
