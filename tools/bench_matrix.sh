#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# bench_matrix.sh — reproduces the paper's carving-speed matrix.
#
# Measures `carving netto` (the carve algorithm, excluding read-back) for each
# refinement LEVEL over a suite of machining WORKLOADS, on the SAME binary /
# representation / hardware (apples-to-apples). Levels:
#   S0  per-step stamping            (--legacy)                  [van Hook / z-map class]
#   S1  swept, EXTERNAL buffer       (--legacy-external-buffer)  [straightforward per-move
#                                                                 swept: materialize the swept
#                                                                 volume, then subtract it]
#   S2  swept, FUSED in-place        (AUTOCAM_SWEPT_SKIP=0)      [this work, Phase 1: envelope
#                                                                 on the fly, subtracted in
#                                                                 place, no intermediate buffer]
#   S3  + tube-pruning + air-skip    (AUTOCAM_SWEPT_SKIP=1)      [this work, Phase 2]
# S1->S2 isolates the FUSION / no-external-buffer gain (and halves resident memory: S1
# needs a full-size swept twin, S2 does not). S2->S3 is the pruning gain. (S4 sparse tiled
# layout is a working-set STUDY, reported separately — see the paper.)
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

net() {  # $1 gcode  $2 level(S0|S1|S2|S3) -> one carving-netto reading (ms)
  local g="gcode/bench/$1.gcode"
  case "$2" in
    S0) "$BIN" simulate --gcode "$g" --tool "$TOOL" --no-view --legacy 2>/dev/null ;;
    S1) "$BIN" simulate --gcode "$g" --tool "$TOOL" --no-view --legacy-external-buffer 2>/dev/null ;;
    S2) AUTOCAM_SWEPT_SKIP=0 "$BIN" simulate --gcode "$g" --tool "$TOOL" --no-view 2>/dev/null ;;
    S3) AUTOCAM_SWEPT_SKIP=1 "$BIN" simulate --gcode "$g" --tool "$TOOL" --no-view 2>/dev/null ;;
  esac | sed -n 's/.*carving netto \([0-9.]*\) ms.*/\1/p'
}

printf "%-13s %9s %9s %9s %9s   %s\n" "workload" "S0 stamp" "S1 extbuf" "S2 fused" "S3 +prune" "S1/S2  S2/S3  S0/S3"
printf -- '%.0s-' {1..80}; echo
for w in $WL; do
  declare -A m; m[S0]=""; m[S1]=""; m[S2]=""; m[S3]=""
  for i in $(seq 0 "$N"); do
    for L in S0 S1 S2 S3; do
      t=$(net "$w" "$L"); [ "$i" -eq 0 ] && continue; [ -z "$t" ] && continue
      [ -z "${m[$L]}" ] || awk "BEGIN{exit !($t<${m[$L]})}" && m[$L]=$t
    done
  done
  fuse=$(awk "BEGIN{printf \"%.2f\", ${m[S1]}/${m[S2]}}")   # fusion (external buffer -> fused in-place)
  prune=$(awk "BEGIN{printf \"%.2f\", ${m[S2]}/${m[S3]}}")  # tube-pruning + air-skip
  ovr=$(awk "BEGIN{printf \"%.1f\", ${m[S0]}/${m[S3]}}")    # overall
  printf "%-13s %7s ms %7s ms %7s ms %7s ms   %5sx %5sx %5sx\n" \
    "$w" "${m[S0]}" "${m[S1]}" "${m[S2]}" "${m[S3]}" "$fuse" "$prune" "$ovr"
done
echo
echo "S0=stamping[van Hook], S1=swept external-buffer, S2=swept fused in-place[this work],"
echo "S3=+tube-pruning+air-skip[this work]. S1->S2 = fusion/no-external-buffer gain (also halves"
echo "resident memory). carving netto (ms), min of $N interleaved runs, tool=$TOOL, Intel iris."
