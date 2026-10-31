# ENERGUN METAL CORE — CP2.15 AC79 target compile probe and VCA callback decision

Date: 2026-10-31
Branch: `energun-metal-core`
Scope: compile-only target ABI/code-generation probe and source-backed comparison of Felucca amplitude callbacks. No engine integration.

## 1. Target ABI and code-generation probe

Added `tests/metal_core_vca_target_probe.c`:
- includes the isolated VCA implementation;
- uses a compile-time assertion that `sizeof(eng_metal_vca_t) == 36`;
- exports noinline probe functions for one sample, a 32-sample block, and the state size.

The baseline CI workflow now compiles this file with the pinned JieLi `pi32v2` Clang target and stores the object, disassembly, and symbol table in the workflow artifact.

Acceptance for this probe:
- target compilation succeeds;
- the target ABI satisfies the 36-byte state assertion;
- the disassembly contains the one-sample and 32-sample probe symbols.

This is target-compiler/ABI/code-generation evidence only. It is not execution on the AC79, a cycle benchmark, a full firmware link with the ENERGUN engine, or physical-device audio validation. Inspect the generated object before drawing conclusions about helper calls or instruction cost.

Expected state use remains 36 bytes per voice, or 288 bytes for eight voices, excluding any containing-structure alignment/placement overhead. The compile-time assertion now checks the target compiler's view of the standalone structure.

## 2. Felucca callback comparison

Pinned Felucca-Engines API at `d0260074ce100a47675814b7f38d61f11176c926` defines:
- `engine_t.amp(track_t*, voice_t*, int32_t adsr)`: called once per control tick, receives the host ADSR Q15 value and returns Q15 amplitude;
- `vmod_t.amp0/amp1`: host amplitude ramp endpoints interpolated across the render block;
- `engine_t.render(..., n, ...)`: per-voice sample-block callback;
- `engine_t.ownenv=1`: bypasses host ADSR and host velocity multiplication for the engine amplitude path; `done()` controls voice lifetime once per control tick before render.

The V98 source envelope is a scheduled one-pole contour. Its Attack/Decay/Release curves, Gate edge, and retrigger hold are sample-time behaviors. A once-per-CTL `amp()` callback followed by the host's linear `amp0 -> amp1` block ramp cannot reproduce that curve exactly; it substitutes a block-rate endpoint/ramp approximation and quantizes envelope transitions to control ticks.

## 3. Preferred integration point

**Preferred for V98 parity: process the VCA sample-by-sample inside the METAL CORE `render()` callback, after the LPG gain stage, as the final per-voice amplitude stage.**

Reasons:
1. Preserves the one-pole sample-domain contour already implemented and tested in isolation.
2. Preserves Gate/Decay priority and retrigger hold at sample boundaries rather than converting them to CTL-rate endpoints.
3. Keeps VCA placement consistent with the V98 graph: LPG filter -> LPG gain/envelope -> VCA -> voice output/ring stage (subject to final signal-path review).

Expected lifecycle contract if adopted:
- set `ownenv=1` so host ADSR amplitude and host velocity multiplication do not cascade with the internal VCA;
- initialize/retrigger the internal VCA from the engine's note callback;
- store one VCA state per active voice in engine-owned per-part memory (the 36-byte state does not fit in `voice_t.s[8]`, which is 32 bytes);
- supply velocity/accent explicitly to the VCA target calculation;
- use `done()` to end the host voice after the internal VCA reaches idle. Since `done()` is checked before each block, a VCA that reaches silence during a block is retired on the next control tick; verify this boundary in integration tests.

This is a preferred architecture decision for implementation planning, not a completed integration. The exact mapping of sequencer Gate, MIDI note-off, retrigger/ratchet, and manual mode still needs to be reconciled with CP1.15/CP1.16 and the FM-1 host event lifecycle. Do not infer that every MIDI note must use the V98 sixteenth-note sequencer Gate.

## 4. Why not use `amp()` as the primary VCA

The `amp()` route remains useful for an envelope whose shape is intentionally control-rate or for a deliberate approximation. It is not selected as the primary METAL CORE V1 VCA because the source parity target is sample-domain. Do not run both `amp()` and the sample-domain VCA; that would cascade two amplitude stages.

## 5. Remaining CP2 gates

- [ ] Inspect the target probe object/disassembly from a successful CI run; record whether the 64-bit multiply is native or lowered to helper calls.
- [ ] Measure actual AC79 cycle cost with a target-side benchmark or a defensible target timing harness; disassembly/instruction counts alone are not cycle measurements.
- [ ] Confirm target state placement/alignment and total per-part memory once the voice-state container is designed.
- [ ] Add integration-level tests for note-on, note-off, Gate, retrigger, block-boundary completion, and additive output.
- [ ] Keep manual-mode VCA mapping open until the performance/sequencer contract is agreed.

## 6. Safety and validation boundary

No change to `engines.c`, engine IDs, parameter IDs, UI, persistent storage, flash layout, firmware package, or device state was made. No firmware was flashed. The target probe is a standalone object compiled by CI and included as evidence; it is not linked into the firmware image.
