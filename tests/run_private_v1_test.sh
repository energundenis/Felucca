#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ENERGUN Private runtime cache acceptance. Host-only; no device access.
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
echo "== Strict compile Private runtime cache"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src \
    -o "$OUT/private-runtime-cache" \
    tests/energun_private_v1_test.c firmware/src/energun_private_v1.c
echo "== Run Private runtime cache"
"$OUT/private-runtime-cache"
echo "PASS: private-runtime-cache"
