#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# CP2 DSP primitive qualification lane. Host-only; no device access or flashing.
set -eu
cd "$(dirname "$0")/.."

CC="${CC:-cc}"
OUT="build/cp2-dsp-qualification"
mkdir -p "$OUT" build/gen
python3 -c 'import runpy; ns = runpy.run_path("tools/build.py"); ns["generate"]()'

compile_run() {
    name="$1"; source="$2"; libs="${3:-}"
    echo "== CP2 compile: $name"
    # shellcheck disable=SC2086
    "$CC" -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/$name" "$source" $libs
    echo "== CP2 run: $name"
    "$OUT/$name"
    echo "PASS: $name" >> "$OUT/results.txt"
}
: > "$OUT/results.txt"
compile_run metal_core_osc tests/metal_core_osc_test.c "-lm"
compile_run metal_core_osc_source tests/metal_core_osc_source_test.c
compile_run metal_core_follow tests/metal_core_follow_test.c "-lm"
compile_run metal_core_follow_host tests/metal_core_follow_host_test.c
compile_run metal_core_lpg tests/metal_core_lpg_test.c
compile_run metal_core_lpg_timing tests/metal_core_lpg_timing_test.c
compile_run metal_core_lpg_env tests/metal_core_lpg_env_test.c
compile_run metal_core_vca tests/metal_core_vca_test.c "-lm"
echo "CP2 DSP qualification lane: all 8 tests passed"
