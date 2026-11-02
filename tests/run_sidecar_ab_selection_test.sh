#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Real storage.c + Sidecar V1 A/B selection test against simulated NOR.
set -eu
cd "$(dirname "$0")/.."
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
CC="${CC:-cc}"
echo "== Strict compile Sidecar A/B selection (-std=c11 -Wall -Wextra -Werror)"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src \
    -o "$OUT/sidecar_ab_selection" tests/sidecar_ab_selection_test.c
echo "== Run Sidecar A/B selection acceptance"
"$OUT/sidecar_ab_selection"
echo "PASS: sidecar-ab-selection" >> "$OUT/results.txt"
