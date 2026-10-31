# ENERGUN METAL CORE — CP2.16 AC79 VCA code-generation and CI evidence audit

Date: 2026-10-31
Branch: `energun-metal-core`
Source commit tested: `cf9e6cba31f3795780cfb8114c5f9b6a8a7d126a`
CI run: [41225293878](https://github.com/energundenis/Felucca/actions/runs/41225293878)
Job: `124608253822`
Artifact: [13052121394](https://github.com/energundenis/Felucca/actions/runs/41225293878/artifacts/13052121394)

## 1. CI result

Run 41225293878 completed with conclusion **success** on `energun-metal-core`. All 15 primary workflow steps completed; the SDK cache-save step was skipped because the pinned cache was restored, which is expected.

The run verified:
- Unmodified Felucca target firmware build succeeded.
- The standalone ENERGUN VCA probe compiled with the pinned JieLi `pi32v2` target compiler.
- Full host regression, web tests and backup web tests completed successfully.
- The isolated METAL CORE VCA test printed `PASS (0 failures)`; its test source reports 20 checks.
- Regression log reported 98 golden renders with 0 changed, 0 gone, 0 health failures, 0 voice/routing failures, 0 CPU entries over budget and 0 crashes. These are Felucca host regression results, not an ENERGUN render benchmark.

The firmware build log reported:
- `felucca.bin`: 513,756 bytes.
- `loader/ota.bin`: 7,130 bytes.
- `felucca.fwsc`: 610,286 bytes, identity `FM-1_900`.
- Baseline image/RAM report: image 513,756 B; `.data + .bss` 61,908 B of 73,728 B; pool 326,208 B of 368,640 B.

These are the unmodified Felucca baseline build values. They do not include a registered ENERGUN engine or prove its future memory headroom.

The artifact contains six files: the three baseline build outputs and the VCA target object, disassembly and symbol table. Artifact ZIP size: 949,324 bytes; SHA-256: `f43011970d4545b104c64c70f6627bff61c71707d6434b413c00fbc8f170d764`. Artifact ID 13052121394 is retained until 2026-11-14.

## 2. AC79 target object inspection

The downloaded object is `ELF32-pi32v2`. Inspection of `metal-core-vca-target/vca_probe.dis` and `vca_probe.symbols` found:

- The compile-time target ABI assertion passed: `sizeof(eng_metal_vca_t) == 36` bytes.
- The exported size probe returns 36.
- The optimized `eng_metal_vca_process` body is 0xB8 bytes (184 bytes) in this compile-only object.
- The sample update contains one target instruction performing the unsigned wide multiply/accumulate into the register pair (`r7_r6 += r4 * r2 (u)`), followed by the Q30 shift. No separate multiply helper symbol or unresolved arithmetic helper appears in the object symbol table.
- The 32-sample wrapper is a bounded loop that calls the sample processor 32 times.

This confirms target compiler acceptance, ABI size and the inspected code shape. It does **not** establish cycle count, worst-case latency, interrupt interaction, target execution, or actual audio quality. The object is standalone and is not linked into `felucca.bin`.

## 3. Warnings observed

The successful host regression log includes compiler warnings, including a sequence-point warning at `tests/input_test.c:47` for multiple operations on `host_now`, along with unused-variable and signedness warnings in other existing test/firmware sources. They did not fail this run. The sequence-point warning should be reviewed separately; this audit does not silently classify it as harmless or change unrelated tests.

## 4. Updated evidence status

- CP2.15's target compile/ABI gate is now passed and its object/disassembly inspection gate is passed.
- VCA host test set: 20 checks, 0 failures.
- The target cycle benchmark remains open.
- Per-part state placement/alignment and memory ownership remain open.
- Integration tests for actual engine note lifecycle, render-block completion and additive audio remain open.
- Manual-mode versus sequencer/performance Gate mapping remains open.

## 5. Safety boundary

No ENERGUN engine registration, `engines.c` integration, parameter IDs, UI, persistent storage, flash layout, firmware package logic, factory firmware, or physical device state was changed. No firmware was flashed. No change was made to `main`.
