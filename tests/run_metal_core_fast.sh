#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ENERGUN METAL CORE fast host regression. No device access or firmware flashing.
set -eu
cd "$(dirname "$0")/.."

CC="${CC:-cc}"
OUT="build/metal-core-fast"
mkdir -p "$OUT" build/gen

echo "== Generate the same checked-in-source headers used by the full build"
python3 -c 'import runpy; ns = runpy.run_path("tools/build.py"); ns["generate"]()'

compile_test() {
    name="$1"
    source="$2"
    shift 2
    echo "== Compile $name"
    "$CC" -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/$name" "$source" "$@"
}
run_test() {
    name="$1"
    echo "== Run $name"
    "$OUT/$name"
    echo "PASS: $name" >> "$OUT/results.txt"
}

: > "$OUT/results.txt"

compile_test metal_core_osc tests/metal_core_osc_test.c -lm
run_test metal_core_osc

compile_test metal_core_lpg tests/metal_core_lpg_test.c
run_test metal_core_lpg

compile_test metal_core_lpg_timing tests/metal_core_lpg_timing_test.c
run_test metal_core_lpg_timing

compile_test metal_core_lpg_env tests/metal_core_lpg_env_test.c
run_test metal_core_lpg_env

compile_test metal_core_vca tests/metal_core_vca_test.c -lm
run_test metal_core_vca

compile_test metal_core_vca_harness tests/metal_core_vca_harness_test.c -lm
run_test metal_core_vca_harness

compile_test metal_core_vca_real_track_adapter tests/metal_core_vca_real_track_adapter_test.c -lm
run_test metal_core_vca_real_track_adapter

compile_test metal_core_cp3_memory_accounting tests/metal_core_cp3_memory_accounting_test.c
run_test metal_core_cp3_memory_accounting

compile_test metal_core_follow tests/metal_core_follow_test.c -lm
run_test metal_core_follow

compile_test metal_core_follow_host tests/metal_core_follow_host_test.c
run_test metal_core_follow_host

echo "== ENERGUN METAL CORE fast regression: all 10 tests passed"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "## ENERGUN METAL CORE fast host regression"
        echo
        echo "Generated headers and compiled/executed all 10 focused host tests."
        echo
        cat "$OUT/results.txt"
        echo
        echo "No AC79 target build, full firmware build, hardware access, or flashing is performed by this fast lane."
    } >> "$GITHUB_STEP_SUMMARY"
fi
