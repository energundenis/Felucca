#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Focused host test of the actual Sidecar V1 encoder/validator.
set -eu
cd "$(dirname "$0")/.."
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
CC="${CC:-cc}"
echo "== Strict compile Sidecar V1 (-std=c11 -Wall -Wextra -Werror)"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src \
    -o "$OUT/sidecar_v1" \
    tests/energun_sidecar_v1_test.c firmware/src/energun_sidecar_v1.c
echo "== Run Sidecar V1 acceptance"
"$OUT/sidecar_v1"
echo "PASS: sidecar-format" >> "$OUT/results.txt"
