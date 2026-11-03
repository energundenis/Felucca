#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="$ROOT/build/cp1-pair-storage-adapter"
mkdir -p "$OUT"
CC="${CC:-cc}"
"$CC" -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/firmware/src" "$ROOT/tests/pair_storage_adapter_test.c" \
  -o "$OUT/pair_storage_adapter_test"
"$OUT/pair_storage_adapter_test"
