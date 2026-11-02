#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# CP3 host lifecycle/dispatch lane. No production registration or device access.
set -eu
cd "$(dirname "$0")/.."

CC="${CC:-cc}"
OUT="build/cp3-host-lifecycle"
mkdir -p "$OUT"
# The parent fast-regression step generates these headers once before dispatch.
# Read-only sharing avoids concurrent writers in the CP1/CP2/CP3 worker pool.
if [ ! -f build/gen/felucca_tables.h ]; then
    echo "Missing generated build/gen/felucca_tables.h; run the parent fast-regression setup first." >&2
    exit 2
fi

compile_run() {
    name="$1"; source="$2"; libs="${3:-}"
    echo "== CP3 compile: $name"
    # shellcheck disable=SC2086
    "$CC" -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/$name" "$source" $libs
    echo "== CP3 run: $name"
    "$OUT/$name"
    echo "PASS: $name" >> "$OUT/results.txt"
}
: > "$OUT/results.txt"
compile_run metal_core_vca_harness tests/metal_core_vca_harness_test.c "-lm"
compile_run metal_core_vca_real_track_adapter tests/metal_core_vca_real_track_adapter_test.c "-lm"
compile_run metal_core_cp3_memory_accounting tests/metal_core_cp3_memory_accounting_test.c
compile_run metal_core_cp3_real_dispatch tests/metal_core_cp3_real_dispatch_test.c "-lm"
compile_run metal_core_cp3_real_vca_dispatch tests/metal_core_cp3_real_vca_dispatch_test.c "-lm"
compile_run metal_core_osc_real_dispatch tests/metal_core_osc_real_dispatch_test.c "-lm"
echo "CP3 host lifecycle lane: all 6 tests passed"
