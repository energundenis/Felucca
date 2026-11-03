#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Private runtime / Sidecar payload bridge acceptance; host-only.
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
echo "== Strict compile Private Sidecar bridge"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src \
    -o "$OUT/private-payload-bridge" \
    tests/private_payload_v1_test.c \
    firmware/src/energun_private_v1.c \
    firmware/src/energun_private_payload_v1.c
echo "== Run Private Sidecar bridge"
"$OUT/private-payload-bridge"
echo "PASS: private-payload-bridge"
