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
    "$CC" -O2 -w -Ibuild/gen -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 -Ifirmware/src -o "$OUT/$name" "$source" "$@"
}
compile_test_strict() {
    name="$1"
    source="$2"
    shift 2
    echo "== Strict compile $name (-std=c11 -Wall -Wextra -Werror)"
    "$CC" -std=c11 -O2 -Wall -Wextra -Werror -Ibuild/gen -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 -Ifirmware/src -o "$OUT/$name" "$source" "$@"
}
run_test() {
    name="$1"
    echo "== Run $name"
    "$OUT/$name"
    echo "PASS: $name" >> "$OUT/results.txt"
}

: > "$OUT/results.txt"

# Persistence is a first-class fast lane: exercise the real storage.c against
# simulated NOR, then the FM6 voice-bank staging path against the same host model.
compile_test storage tests/storage_test.c
run_test storage

# The pair harness includes the real storage.c and Sidecar V1 serializer; its
# Sidecar sectors live only in a separate test backend, never in target Flash.
compile_test_strict sidecar_pair_transaction tests/sidecar_pair_transaction_test.c -Wno-unused-function
run_test sidecar_pair_transaction

# All seven logical Sidecar identities are checked for strict identity isolation.
# Preset-bank slot updates/erases must preserve unrelated slots.
compile_test_strict sidecar_identity_matrix tests/sidecar_identity_matrix_test.c firmware/src/energun_sidecar_v1.c
run_test sidecar_identity_matrix

compile_test fm6bank tests/fm6bank_test.c -lm
run_test fm6bank

compile_test metal_core_osc tests/metal_core_osc_test.c -lm
run_test metal_core_osc

compile_test metal_core_osc_source tests/metal_core_osc_source_test.c -lm
run_test metal_core_osc_source

compile_test metal_core_osc_voice tests/metal_core_osc_voice_test.c -lm
run_test metal_core_osc_voice
compile_test metal_core_osc_real_dispatch tests/metal_core_osc_real_dispatch_test.c -lm
run_test metal_core_osc_real_dispatch

# Real OSC host behavior is covered by metal_core_osc_real_dispatch; the AC79
# compile-only probe runs in the full baseline workflow.

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

compile_test metal_core_cp3_real_dispatch tests/metal_core_cp3_real_dispatch_test.c -lm
run_test metal_core_cp3_real_dispatch

compile_test metal_core_cp3_real_vca_dispatch tests/metal_core_cp3_real_vca_dispatch_test.c -lm
run_test metal_core_cp3_real_vca_dispatch

compile_test_strict metal_core_feedback_delay_candidate tests/metal_core_feedback_delay_candidate_test.c
run_test metal_core_feedback_delay_candidate

compile_test metal_core_follow tests/metal_core_follow_test.c -lm
run_test metal_core_follow

compile_test metal_core_follow_host tests/metal_core_follow_host_test.c
run_test metal_core_follow_host

compile_test_strict metal_core_parameter_quantization tests/metal_core_parameter_quantization_test.c -lm
run_test metal_core_parameter_quantization

compile_test_strict metal_core_parameter_inventory tests/metal_core_parameter_inventory_test.c
run_test metal_core_parameter_inventory

python3 tests/test_cp1_test_queue.py
echo "PASS: cp1_test_queue" >> "$OUT/results.txt"

echo "== ENERGUN METAL CORE fast regression: all 24 tests passed"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "## ENERGUN METAL CORE fast host regression"
        echo
        echo "Generated headers and compiled/executed all 24 focused host tests."
        echo
        cat "$OUT/results.txt"
        echo
        echo "No AC79 target build, full firmware build, hardware access, or flashing is performed by this fast lane."
    } >> "$GITHUB_STEP_SUMMARY"
fi
