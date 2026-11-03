#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="$ROOT/build/cp1-pair-transaction"
mkdir -p "$OUT"
CC="${CC:-cc}"
"$CC" -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/firmware/src" "$ROOT/tests/pair_txn_test.c" \
  "$ROOT/firmware/src/energun_pair_txn.c" \
  "$ROOT/firmware/src/energun_sidecar_v1.c" -o "$OUT/pair_txn_test"
"$OUT/pair_txn_test"
