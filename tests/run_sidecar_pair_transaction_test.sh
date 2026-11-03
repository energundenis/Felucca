#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="$ROOT/build/sidecar-pair-test"
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Werror \
  "$ROOT/tests/sidecar_pair_transaction_test.c" -o "$OUT"
"$OUT"
