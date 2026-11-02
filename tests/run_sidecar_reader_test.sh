#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Exact-generation Sidecar reader acceptance suite.
set -eu
cd "$(dirname "$0")/.."
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
CC="${CC:-cc}"
echo "== Strict compile Sidecar reader (-std=c11 -Wall -Wextra -Werror)"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src \
    -o "$OUT/sidecar_reader" tests/sidecar_reader_test.c
echo "== Run exact-generation Sidecar reader acceptance"
"$OUT/sidecar_reader"
echo "PASS: sidecar-reader" >> "$OUT/results.txt"
