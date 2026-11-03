#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="$ROOT/build/sidecar-identity-matrix"
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Werror -I"$ROOT/firmware/src" \
  "$ROOT/tests/sidecar_identity_matrix_test.c" \
  "$ROOT/firmware/src/energun_sidecar_v1.c" -o "$OUT"
"$OUT"
