#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# CP1.4.2 standalone quantizer correctness lane; host-only, no device access.
set -eu
cd "$(dirname "$0")/.."
OUT="build/cp1-test-queue"
mkdir -p "$OUT"
echo "== Strict compile CP1.4.2 quantization reference (-std=c11 -Wall -Wextra -Werror)"
cc -std=c11 -O2 -Wall -Wextra -Werror \
  -o "$OUT/parameter-quantization" \
  tests/metal_core_parameter_quantization_test.c -lm
echo "== Run CP1.4.2 quantization reference"
"$OUT/parameter-quantization"
