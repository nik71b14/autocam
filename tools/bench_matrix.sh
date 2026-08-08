#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# bench_matrix.sh — reproduces the paper's carving-speed matrix.
#
# Measures `carving netto` (the carve algorithm, excluding read-back) for each
# refinement LEVEL over a suite of machining WORKLOADS, on the SAME binary /
# representation / hardware (apples-to-apples). Levels:
#   S0  per-step stamping        (--legacy)                 [van Hook / z-map class]
#   S1  swept-segment subtract   (AUTOCAM_SWEPT_SKIP=0)     [multi-dexel per-move class]
#   S2  + tube-pruning+air-skip  (AUTOCAM_SWEPT_SKIP=1)     [this work]
# (S3 sparse tiled layout is a working-set STUDY, reported separately: it trades
#  host-side materialization for a footprint win on localized work — see the paper.)
#
# Rounds are interleaved across levels to cancel GPU clock/thermal drift; the first
# round is discarded (warm-up); the minimum is reported. Real GPU only.
#
#   cd ~/Documents/development/autocam && bash tools/bench_matrix.sh
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/.." || exit 1
BIN=./release/autocam
TOOL=test/hemispheric_mill_3.bin      # small tool (32 vox): exercises the L>>D regime
N=5
WL="contour pocket_axis raster45 rapids localized finishing"

net() {  # $1 gcode  $2 level(S0|S1|S2) -> one carving-netto reading (ms)
  local g="gcode/bench/$1.gcode"
  case "$2" in
    S0) "$BIN" simulate --gcode "$g" --tool "$TOOL" --no-view --legacy 2>/dev/null ;;
    S1) AUTOCAM_SWEPT_SKIP=0 "$BIN" simulate --gcode "$g" --tool "$TOOL" --no-view 2>/dev/null ;;
    S2) AUTOCAM_SWEPT_SKIP=1 "$BIN" simulate --gcode "$g" --tool "$TOOL" --no-view 2>/dev/null ;;
  esac | sed -n 's/.*carving netto \([0-9.]*\) ms.*/\1/p'
}

printf "%-14s %10s %10s %10s   %s\n" "workload" "S0 stamp" "S1 swept" "S2 +prune" "S0/S2  S1/S2"
printf -- '%.0s-' {1..70}; echo
for w in $WL; do
  declare -A m; m[S0]=""; m[S1]=""; m[S2]=""
  for i in $(seq 0 "$N"); do
    for L in S0 S1 S2; do
      t=$(net "$w" "$L"); [ "$i" -eq 0 ] && continue; [ -z "$t" ] && continue
      [ -z "${m[$L]}" ] || awk "BEGIN{exit !($t<${m[$L]})}" && m[$L]=$t
    done
  done
  sp02=$(awk "BEGIN{printf \"%.1f\", ${m[S0]}/${m[S2]}}")
  sp12=$(awk "BEGIN{printf \"%.2f\", ${m[S1]}/${m[S2]}}")
  printf "%-14s %8s ms %8s ms %8s ms   %5sx %5sx\n" "$w" "${m[S0]}" "${m[S1]}" "${m[S2]}" "$sp02" "$sp12"
done
echo
echo "S0=stamping[van Hook], S1=swept[Mueller&Surmann/tri-dexel], S2=+tube-pruning+air-skip[this work]."
echo "carving netto (ms), min of $N interleaved runs, tool=$TOOL, Intel iris."
