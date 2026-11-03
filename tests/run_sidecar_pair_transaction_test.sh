#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="$ROOT/build/sidecar-pair-test"
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -w -I"$ROOT/build/gen" -I"$ROOT/firmware/hal" -I"$ROOT/firmware/bsp/chip" -I"$ROOT/firmware/bsp/boards/fm1" -I"$ROOT/firmware/src" \
  "$ROOT/tests/sidecar_pair_transaction_test.c" -o "$OUT"
"$OUT"
