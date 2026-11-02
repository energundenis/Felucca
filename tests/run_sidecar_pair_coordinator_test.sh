#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Backend-neutral Sidecar pair coordinator using real Felucca storage.c and NOR hooks.
set -eu
cd "$(dirname "$0")/.."
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
CC="${CC:-cc}"
echo "== Strict compile Sidecar pair coordinator (-std=c11 -Wall -Wextra -Werror)"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src \
    -o "$OUT/sidecar_pair_coordinator" tests/sidecar_pair_coordinator_test.c
echo "== Run Sidecar pair ordering and power-loss matrix"
"$OUT/sidecar_pair_coordinator" "$@"
echo "PASS: sidecar-pair-coordinator" >> "$OUT/results.txt"
