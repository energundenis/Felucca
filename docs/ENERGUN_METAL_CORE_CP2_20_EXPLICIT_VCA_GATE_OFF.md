# ENERGUN METAL CORE — CP2.20 explicit VCA Gate-off event

Date: 2026-10-31
Branch: energun-metal-core
Status: isolated model/API and host tests updated; no engine integration.

## Change

Added `eng_metal_vca_gate_off(eng_metal_vca_t *vca)` to `firmware/src/eng_metal_core_vca.h`.

- A live MIDI Note Off or sequencer step Gate end can explicitly switch the envelope to Release.
- The helper immediately changes the phase and target but does not advance the level; the next `process()` call advances the first Release sample from the instantaneous current level.
- Pending Decay is suppressed, preserving CP1.16 Option B.
- Repeated Gate-off calls during Release are idempotent and do not restart or reshape the Release trajectory.
- Gate-off on an idle VCA is harmless.
- A subsequent trigger retains cancel-and-hold retrigger behavior.
- Existing fixed `gate_remaining` countdown remains supported for the isolated model's prior tests and use cases.
- No state fields were added; the target-verified VCA state remains 36 bytes.

## Tests added

`tests/metal_core_vca_test.c` now covers:

1. Explicit Gate-off during Attack selects Release while preserving the instantaneous level.
2. The first following sample descends from the held level.
3. Repeated Gate-off does not restart or alter Release.
4. Explicit short Gate suppresses pending Decay through silence.
5. Gate-off on idle is harmless.
6. Retrigger after Gate-off holds the current level and does not reset to zero.

These tests exercise the isolated VCA helper only. They do not yet exercise MIDI event dispatch, sequencer callbacks, the Felucca engine render callback, sample-accurate sub-block events, or physical FM-1 audio.

## Integration boundary

- The pinned Felucca host API still exposes `voice_t.gate` as state observed by the render callback; it has no engine `note_off` callback or sub-block event offset.
- A future METAL CORE render adapter can detect `v->gate` falling at the start of a render block and call `eng_metal_vca_gate_off()` once for that voice.
- This gives block-boundary Note Off behavior, not sample-accurate Note Off. The exact event offset would require a separately reviewed host API extension.
- The live MIDI and sequencer envelope behavior remains the CP2.19 full-envelope decision.
- Accent provenance remains open; do not use expressive MIDI velocity as a proxy for sequencer Accent.

## Verification status

Source/test changes are committed, but CI has not yet been used to claim success for this change. Await the branch CI result before accepting CP2.20 as verified.

## Safety boundary

No production engine callbacks, engine registry, DSP render chain, parameter IDs, UI, persistence/Flash layout, firmware package or device state changed. The METAL CORE engine remains unregistered. No device flashing. `main` and factory firmware remain untouched.