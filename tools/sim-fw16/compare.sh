#!/bin/sh
# Runs the search of FW 1.6 (tune.c, swr.c here) and the search of NG in the
# NG simulator on the same cases and prints the results side by side, as in
# docs/COMPARISON.md.
# Usage: tools/sim-fw16/compare.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
SIM=$ROOT/tools/sim
OUT=$HERE/build
CC=${CC:-cc}
FLAGS="-O2 -std=c99 -D_DEFAULT_SOURCE"
MODEL="$SIM/sim.c $SIM/model.c $SIM/antennas.c"
mkdir -p "$OUT"
echo "NG: $(sed -n 's/.*FW_VERSION "\(.*\)"/\1/p' "$ROOT/src/version.h")"

$CC $FLAGS -w -funsigned-char -I"$HERE" -I"$SIM" -o "$OUT/fw16" \
   $MODEL "$SIM/glue_ref.c" "$HERE/tune.c" "$HERE/swr.c" -lm
$CC $FLAGS -Wall -I"$ROOT/src" -I"$SIM" -o "$OUT/ng" \
   $MODEL "$SIM/glue_new.c" "$ROOT/src/tune.c" "$ROOT/src/meas_math.c" "$ROOT/src/cells.c" -lm

# the scenarios of the Makefile, plus QSY and band changes
HARD="--noise 3 --rs 10 --jitter 0.03 --tol 5 --cal 1.3 0.3"
for b in fw16 ng; do
   sh "$SIM/run.sh" "$OUT/$b" "$OUT/${b}_clean.tsv" "1" &
   sh "$SIM/run.sh" "$OUT/$b" "$OUT/${b}_noise.tsv" "1 2 3 4 5" --noise 3 &
   sh "$SIM/run.sh" "$OUT/$b" "$OUT/${b}_hard.tsv" "1 2 3 4 5" $HARD &
   (for p in -3 -1 1 3; do "$OUT/$b" --suite ant --retune $p --noise 3; done > "$OUT/${b}_retune.tsv") &
   "$OUT/$b" --hop 12 --noise 3 > "$OUT/${b}_hop.tsv" &
done
wait

for s in clean noise hard retune hop; do
   echo "== $s"
   python3 "$SIM/compare.py" "$OUT/fw16_$s.tsv" "$OUT/ng_$s.tsv"
done

# FW 1.6's own target: SWR 1.20, in the cases where 1.20 is possible (noise)
echo "== SWR 1.20 reached where possible (noise)"
for b in fw16 ng; do
   awk -F'\t' -v b=$b '$5 <= 1.2 { n++; if($4 <= 1.2) k++ }
      END { printf "%-5s %5.1f %% of %d\n", b, 100 * k / n, n }' "$OUT/${b}_noise.tsv"
done
# tunes that gave up after waiting for power (estimated time above 20 s)
echo "== tunes longer than 20 s (hard)"
for b in fw16 ng; do
   awk -F'\t' -v b=$b '{ n++ } $8 > 20 { k++ }
      END { printf "%-5s %d of %d\n", b, k, n }' "$OUT/${b}_hard.tsv"
done
