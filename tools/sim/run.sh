#!/bin/sh
# Runs a simulator binary on the std and antenna suites for several seeds.
# Usage: run.sh BIN OUT.tsv "SEEDS" [sim options...]
BIN=$1; OUT=$2; SEEDS=$3; shift 3
for s in $SEEDS; do
   "$BIN" --suite std --seed "$s" "$@" || exit 1
   "$BIN" --suite ant --seed "$s" "$@" || exit 1
done > "$OUT"
