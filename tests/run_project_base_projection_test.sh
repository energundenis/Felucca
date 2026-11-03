#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$ROOT/build/cp1-project-projection"
mkdir -p "$OUT"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware/src" \
  "$ROOT/firmware/src/energun_project_base_projection.c" \
  "$ROOT/tests/project_base_projection_test.c" -o "$OUT/project_base_projection_test"
"$OUT/project_base_projection_test"
