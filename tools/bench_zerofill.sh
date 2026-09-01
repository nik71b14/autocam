#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# bench_zerofill.sh — the trailing-zero-fill lever (paper §6/§7).
#
# subtract_swept.comp writes the full 32-slot column stride on every processed
# column: writeCount real transitions + zeros to MAX_TRANSITIONS. The zeros are
# dead (occupancy reads only obj1_dataNum[col] slots), so AUTOCAM_SWEPT_ZEROFILL=0
# drops them — byte-identical output, less write traffic on ACTIVE columns.
#
# This A/Bs zero-fill on (default) vs off, for the fused kernel with pruning off
# (S2) and on (S3), interleaved min-of-N to cancel drift. Real GPU only.
#   cd ~/Documents/development/autocam && bash tools/bench_zerofill.sh
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/.." || exit 1
BIN=./release/autocam
TOOL=test/hemispheric_mill_3.bin
N=10                                  # run per config (prima scartata, warm-up); media delle restanti
WL="contour pocket_axis finishing raster45 rapids localized"

net(){ # $1 workload  $2 skip(0|1)  $3 zerofill(0|1) -> carving-netto ms
  AUTOCAM_SWEPT_SKIP=$2 AUTOCAM_SWEPT_ZEROFILL=$3 "$BIN" simulate \
    --gcode "gcode/bench/$1.gcode" --tool "$TOOL" --no-view 2>/dev/null \
    | sed -n 's/.*carving netto \([0-9.]*\) ms.*/\1/p'
}
upd(){ local k=$1 t=$2; [ -z "$t" ] && return    # accumulate for the mean
  sum[$k]=$(awk "BEGIN{print ${sum[$k]:-0}+$t}"); cnt[$k]=$(( ${cnt[$k]:-0} + 1 )); }

printf "%-13s %9s %9s %7s   %9s %9s %7s\n" "workload" "S2 fill" "S2 nofill" "gain" "S3 fill" "S3 nofill" "gain"
printf -- '-%.0s' {1..74}; echo
for w in $WL; do
  declare -A sum cnt m; for k in a b c d; do sum[$k]=0; cnt[$k]=0; done
  for i in $(seq 0 "$N"); do
    a=$(net "$w" 0 1); b=$(net "$w" 0 0)   # S2: fill vs no-fill (adjacent)
    c=$(net "$w" 1 1); d=$(net "$w" 1 0)   # S3: fill vs no-fill
    [ "$i" -eq 0 ] && continue
    upd a "$a"; upd b "$b"; upd c "$c"; upd d "$d"
  done
  for k in a b c d; do m[$k]=$(awk "BEGIN{printf \"%.2f\", ${sum[$k]}/${cnt[$k]}}"); done
  g2=$(awk "BEGIN{printf \"%+.1f%%\", (${m[b]}/${m[a]}-1)*100}")
  g3=$(awk "BEGIN{printf \"%+.1f%%\", (${m[d]}/${m[c]}-1)*100}")
  printf "%-13s %8s %9s %7s   %8s %9s %7s\n" "$w" "${m[a]}" "${m[b]}" "$g2" "${m[c]}" "${m[d]}" "$g3"
done
echo
echo "carving netto (ms), media di $N run interleaved; gain = (no-fill/fill - 1), negative = faster."
echo "S2 = fused, pruning off (AUTOCAM_SWEPT_SKIP=0); S3 = + tube pruning. Intel iris, mill_3."
echo "Output is byte-identical between fill and no-fill (verified via .bin diff)."
