# ENERGUN METAL CORE — CP2.23 VCA-only integration harness

Date: 2026-11-01  
Branch: `energun-metal-core`  
Scope: isolated host harness only; no production engine registration or device change.

## Purpose

Exercise the pinned Felucca `engine_t` contract around the existing fixed-point VCA before the complete METAL CORE DSP chain is connected. The harness is deliberately not included in `engines.c` and does not create a playable engine.

## Implementation

- `eng_metal_core_vca_render.h` defines the per-part state as eight `eng_metal_vca_t` objects. The AC79-verified object size is 36 bytes; the eight-voice payload is 288 bytes.
- The host harness declares an `engine_t` descriptor with `ownenv=1`, `note_on`, `render`, `done`, and `mem=sizeof(state)`. It uses a uint64-aligned per-part region equivalent to Felucca's `ENG_MEM(p)` pool and enforces the 12,888-byte default `ENG_MEM_PART` bound.
- Voice lookup validates the host voice index and maps it to a separate VCA state. The harness tests all eight voices and verifies that processing one state does not mutate the other seven.
- Render uses a constant source signal, advances the VCA sample-by-sample, multiplies it by the host `m.amp0 -> m.amp1` Q15 ramp using Felucca's CTL=32 ramp convention, and adds the result to `out[]`.
- Explicit Gate-off is observed at the next render-block boundary and starts Release. Retrigger uses the VCA's cancel-and-hold behavior. `done()` reports completion only after the internal VCA reaches idle.
- Accent is read from the explicit per-voice metadata bit, not inferred from expressive velocity.

## Acceptance tests

1. `engine_t.mem` equals 288 bytes and fits `ENG_MEM_PART`.
2. The per-part region is aligned.
3. Eight voices map to eight independent VCA objects.
4. Processing one voice leaves the other seven unchanged.
5. Render is additive and the host amplitude ramp remains active.
6. Gate-off starts Release; retrigger preserves level; Release reaches idle; `done()` then retires the voice.
7. AC79 target compile emits a combined VCA + host-ramp sample and 32-sample block probe.
8. Full host/web regression and the existing target CPU-budget check pass without raising thresholds.

## Limits

- Gate timing is block-boundary only; no sub-block event offset is claimed.
- The AC79 probe is compile/disassembly evidence, not a cycle-accurate runtime benchmark.
- The unchanged Felucca firmware build remains the baseline. The harness is not linked into the firmware.
- The VCA-only harness does not exercise oscillator/FM/fold/LPG integration, full METAL CORE audio, engine registration, parameter/UI/storage, or hardware playback.
- No firmware image is installed, no device is flashed, and factory firmware and `main` remain untouched.

## Next gate

After the exact-head Accent metadata regression and CP2.23 harness CI complete successfully, review target code generation, memory and target-budget output. Only then prepare CP3 full METAL CORE integration.

## Verification result — CP2.23 closed

Verified on exact implementation HEAD `ed8a35421aa185306659c35636bcd7200ca8228e`, GitHub Actions run [41242563432](https://github.com/energundenis/Felucca/actions/runs/41242563432), completed with conclusion `success`.

- Host VCA harness printed `PASS (0 failures)`.
- Memory, alignment, eight-voice isolation, invalid voice pointer, additive output, host amplitude ramp, Gate-off, retrigger, Release and `done()` checks all passed.
- AC79 pi32v2 target compile produced both combined VCA + host-ramp probe symbols and disassembly.
- Full Felucca host/web regression passed. Primary regression reported 98 golden renders unchanged, 0 health failures, 0 voice/routing failures, 0 CPU entries over budget and 0 crashes.
- Existing target render-loop static budget script completed successfully with the repository's existing thresholds; no threshold was raised.
- The unchanged baseline firmware build completed. The workflow artifact contains the baseline firmware package and isolated target probe evidence.

### Measurement limits

- The 288-byte VCA allocation is the isolated VCA-only per-part payload, not the final complete METAL CORE engine memory total.
- The existing target-budget report covers Felucca's existing render loops; it does not yet include the new METAL CORE VCA or the complete METAL CORE chain.
- The combined VCA probe proves AC79 code generation and provides disassembly, not cycle-accurate runtime cost.
- The host regression's second-core-disabled configuration reports that its instruction counter is unavailable; its CPU budget is not checked in that configuration.
- No physical FM-1 playback, flash, factory firmware modification or device write occurred.

**Disposition:** CP2.23 acceptance criteria are met for the isolated VCA-only harness. CP2.23 is closed. Proceed to CP3 integration preparation; do not treat this as full METAL CORE engine integration or hardware validation.
