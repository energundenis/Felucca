#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Real storage.c + Sidecar V1 Base-generation binding host test.
set -eu
cd "$(dirname "$0")/.."
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
CC="${CC:-cc}"
echo "== Strict compile Base-generation binding (-std=c11 -Wall -Wextra -Werror)"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src \
    -o "$OUT/sidecar_base_binding" tests/sidecar_base_binding_test.c
echo "== Run Base-generation binding acceptance"
"$OUT/sidecar_base_binding"
echo "PASS: base-generation-binding" >> "$OUT/results.txt"
