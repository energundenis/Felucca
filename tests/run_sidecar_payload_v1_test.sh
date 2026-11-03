#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Focused strict host acceptance for typed Sidecar V1 payload codecs.
set -eu
cd "$(dirname "$0")/.."
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
CC="${CC:-cc}"
echo "== Strict compile Sidecar V1 payload codecs (-std=c11 -Wall -Wextra -Werror)"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src \
    -o "$OUT/sidecar_payload_v1" \
    tests/energun_sidecar_payload_v1_test.c firmware/src/energun_sidecar_payload_v1.c
echo "== Run Sidecar V1 payload codec acceptance"
"$OUT/sidecar_payload_v1"
echo "PASS: sidecar-payload-codec" >> "$OUT/results.txt"
