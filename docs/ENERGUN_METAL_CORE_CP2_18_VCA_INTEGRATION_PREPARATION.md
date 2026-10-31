# ENERGUN METAL CORE — CP2.18 integration preparation: VCA state, host lifecycle and render boundary

Date: 2026-10-31
Branch: `energun-metal-core`
Status: integration design / source audit. No engine registration or audio-path change.

## 1. Scope

Prepare the first safe integration slice: connect the already isolated sample-domain VCA to a future METAL CORE per-voice render path without yet registering a playable engine or changing the host audio graph.

This record is not a claim that METAL CORE DSP is integrated. The current `firmware/src/eng_metal_core.c` remains an unregistered scaffold. No production engine callbacks, parameter IDs, UI, persistence, Flash layout or device state are changed here.

## 2. Source-backed Felucca contract

Pinned Felucca-Engines interface:
- Repository: `hugelton/Felucca-Engines`
- Commit: `d0260074ce100a47675814b7f38d61f11176c926`
- Header: `src/felucca_engine.h`

Host facts confirmed from the pinned header and current Felucca source:
- `engine_t.note_on(track_t*, voice_t*)` runs when a voice starts or is retriggered.
- `engine_t.render(track_t*, voice_t*, int32_t *out, uint32_t n, const vmod_t *m)` renders a block of at most `CTL=32` samples and must add to `out[]`, never overwrite it.
- `engine_t.ownenv=1` bypasses host ADSR amplitude and host velocity multiplication for ordinary voice stages. The host still applies its LFO-to-AMP and engine-switch fade through `m.amp0/amp1`.
- `engine_t.done()` is called once per control tick before rendering. It controls normal voice lifetime for own-envelope engines.
- `voice_t.gate` reflects whether the key is currently held; `voice_t.active` tracks host voice lifetime. `voice_t.note`, `vel`, `mvel`, `stage`, `env_out`, and the voice index are available to callbacks.
- On a retrigger, host calls `note_on`, then restores phase and selected `voice_t.s[]` slots according to `engine_t.keep`. External engine state in `engine_t.mem` is not part of that restoration.
- Engine memory is per part, zeroed when the host claims the region for a different engine. `ENG_MEM_PART` is 12,888 bytes in the default build and 13,344 bytes with ADPCM.

The CP2.17 shared voice-core optimization is part of this contract: stage 4 remains host-controlled for the shared voice-steal fade; ordinary ownenv voices bypass host ADSR and use the engine `done()` callback.

## 3. VCA state placement and memory

The isolated `eng_metal_vca_t` has a target-verified size of 36 bytes with the pinned AC79 `pi32v2` compiler.

Recommended first integration layout:
- One `eng_metal_vca_t` for each of the host's 8 voice slots in the current part's `engine_t.mem`.
- State array size: `8 × 36 = 288 bytes` per part.
- The host engine-memory region is aligned by a `uint64_t` union; the VCA state requires only ordinary 4-byte alignment.
- This is far below either current `ENG_MEM_PART` configuration, but the final engine state must include every additional DSP state object before `ENG_MEM_FITS` is accepted.
- Do not put the VCA object into `voice_t.s[8]`: that array is 32 bytes and cannot hold the 36-byte structure.
- Resolve a voice's state by validated index `(uint32_t)(v - t->v)`; reject an index outside `0..NVOICE-1` before accessing the array.

The final engine should declare its complete state structure and use `ENG_MEM_FITS("METAL CORE", sizeof(state_t))`. Its `engine_t.mem` value must be `sizeof(state_t)`, not a hand-maintained partial estimate.

## 4. Callback responsibilities

### note_on

- Validate track and voice indices.
- Resolve that voice's VCA state from the part's engine-memory region.
- Preserve the current VCA level on a sounding-voice retrigger, matching the isolated model's cancel-and-hold behavior.
- Reset only when the voice is genuinely starting from silence or when the host has assigned a fresh voice state.
- Build V98 peak/sustain targets from approved parameter values and explicit velocity/accent inputs.
- Prepare Attack, Decay, Release coefficients and timing outside the sample loop.
- Start the contour once the Gate source and timing policy have been resolved.

### render

- Keep the VCA sample-domain processor inside the engine's render callback, after the LPG gain/envelope stage and before adding the voice to the host mix.
- For each sample, advance the internal VCA once and multiply it with the host's `m.amp0 -> m.amp1` ramp. This retains the host's LFO-to-AMP and engine-switch fade while preventing the host ADSR from becoming a second envelope.
- Keep all signal generation additive into `out[]`.
- Do not run the VCA through `engine_t.amp()` as well. That would create a second, block-rate amplitude path and would not match V98.
- Handle `v->gate == 0` as a Gate-off event exactly once per release. The isolated model currently supports a sample countdown; an explicit Gate-off entry point or equivalent one-shot transition is required for host-driven Gate.
- If the host invokes the renderer with a block after Gate-off, the transition can be applied at the first sample of that block. Exact sub-block Gate timing requires a host event offset and is not exposed by the current callback contract.

### done

- Return not-done while the internal VCA remains active.
- Return done only after the internal VCA has reached its idle/silent state.
- Since the host checks `done()` before rendering each block, a VCA that reaches idle inside a block will be retired at the next control tick. Add a test for that one-block boundary.
- Preserve stage-4 voice stealing: the host's stage-4 fade must remain able to retire a voice even if its internal VCA has not naturally completed. Verify exact callback order in an integration test; do not change the shared host path without a measured need.

## 5. Amplitude arithmetic and target-cost gate

The render path must preserve three independent amplitude contributions:
1. LPG output/envelope.
2. Internal V98 VCA (Q16).
3. Host-owned LFO-to-AMP and engine-switch fade (Q15 block ramp).

The isolated VCA sample processor currently uses one 64-bit multiply per sample. Integration adds the output gain multiplication and the combination with the host Q15 ramp. The complete per-sample arithmetic must be measured, not inferred from the isolated VCA probe.

Acceptance gates:
- Add a target probe for the combined VCA + host ramp + sample output operation.
- Compile with the pinned AC79 toolchain and inspect generated code.
- Run the unchanged target-budget check; do not raise `mix_part` or IRQ thresholds to accommodate METAL CORE.
- Add a repeatable target-side cycle/timing benchmark before claiming real-time margin.
- Measure worst-case supported voice count and simultaneous part activity, not just a single voice.
- Record complete per-part state size and whole-firmware RAM/pool deltas.

The CP2.17 result `mix_part=3431 / 3549` leaves 118 static-cost units of headroom in that specific baseline measurement. This is a narrow margin, not permission to spend it without measurement. The isolated VCA compile probe is not a linked render benchmark.

## 6. Integration blockers that must be resolved before wiring note_on

The source audit identifies three behavior decisions that must not be guessed:

1. **Performance/manual mode versus sequencer mode.** V98 sequencer mode uses a sixteenth-note Gate duration derived from BPM; V98 manual mode holds a constant LEVEL rather than running the sequencer VCA contour. FM-1 performance notes and the integrated sequencer need an explicit mapping to those modes.
2. **Accent provenance.** The host exposes `v->vel` and `v->mvel`, but does not provide a dedicated `accent` boolean in the current render callback. A velocity threshold is not equivalent to an explicit sequencer ACCENT flag and would misclassify expressive MIDI velocity. Add a safe source for accent (e.g. a per-voice tag with defined lifecycle) or obtain an explicit product decision to derive Accent from velocity.
3. **Gate timing.** Current callback provides Gate state at render-block entry, not the exact sample offset of a note-off inside a block. The first integration slice can release at the first sample of the next render block; sample-accurate event offsets require a host/API extension and its own CPU/regression review.

Until these are resolved, do not connect the VCA to `note_on` and do not claim V98 parity for live MIDI or sequencer playback.

## 7. Integration test plan

Add a dedicated METAL CORE engine host test, not only isolated VCA unit tests:

| Test | Stimulus | Acceptance |
|---|---|---|
| INT-VCA-01 | note-on from silence | VCA state is initialized once; first rendered block is nonzero and bounded |
| INT-VCA-02 | note-on retrigger while sounding | current VCA level is held; no reset-to-zero discontinuity |
| INT-VCA-03 | note-off between blocks | release begins at the first sample of the next render block |
| INT-VCA-04 | short Gate before Decay | CP1.16 Option B: Release suppresses pending Decay; no reopening |
| INT-VCA-05 | Release completion | voice remains allocated through the block that reaches silence and is retired at the next `done()` check |
| INT-VCA-06 | stage-4 voice steal | host fade retires the voice even if the internal VCA is still active |
| INT-VCA-07 | LFO-to-AMP | host modulation multiplies the internal VCA ramp and is not bypassed |
| INT-VCA-08 | engine-switch fade | additive render fades to zero; no stale VCA state survives a new engine claim |
| INT-VCA-09 | additive output | pre-existing `out[]` samples are preserved and voice samples are added |
| INT-VCA-10 | state indexing | all 8 voice slots map to distinct 36-byte states; invalid pointer/index is rejected |
| INT-VCA-11 | memory | complete engine state passes `ENG_MEM_FITS`; 8-voice state bytes are accounted |
| INT-VCA-12 | engine switching | A→METAL CORE→A and repeated switching leaves no stale envelope or host voice |
| INT-VCA-13 | voice budget | global 8-voice budget and steal policy remain unchanged |
| INT-VCA-14 | CPU/static cost | full host regression and target budget pass without threshold changes |

## 8. Planned implementation order

1. Resolve the three behavior blockers in Section 6.
2. Add a small, host-testable per-part VCA state accessor / callback harness; verify index and memory behavior before real engine wiring.
3. Add explicit Gate-off support to the isolated VCA model and tests; preserve the existing fixed-countdown API tests.
4. Build a minimal VCA-only engine integration harness with a constant source signal, proving additive render, host ramp multiplication and done lifecycle.
5. Only then combine VCA with the METAL CORE oscillator/FM/fold/LPG render chain.
6. Register the engine append-only only after the complete DSP, parameter map, memory, CPU and regression gates are satisfied.

## 9. Safety boundary

No source engine, `engines.c` registry, engine ID, UI, parameter ID, preset/persistence format, Flash layout, firmware package or physical device state was changed for this preparation. The current engine remains unregistered. No device was flashed. `main` and factory firmware remain untouched.
