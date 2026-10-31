# ENERGUN METAL CORE — CP2.11 host pitch-delta math

Date: 2026-10-31
Branch: `energun-metal-core`
Status: isolated fixed-point helper and host tests added; CI validation pending.

## 1. Decision

Do not reconstruct pitch bend, tune, glide, LFO, envelope, fine pitch or modulation-matrix contributions independently. Felucca has already combined them into the final host `vmod_t.inc`.

Instead, calculate a Q24 ratio between two Q0.32 host phase increments:
`ratio_q24 = round((final_inc << 24) / reference_inc)`.

This retains the complete host pitch result and avoids duplicating the host's pitch arithmetic or modulation-matrix implementation.

## 2. Host inputs

At the engine render callback:
- `reference_inc`: `pitch_inc(60 * 16)`, the host C4 reference.
- `note_inc`: `pitch_inc(v->note * 16)`, the current target MIDI note before glide and independent pitch offsets.
- `final_inc`: `m->inc`, after glide, track tune, pitch bend, LFO pitch, envelope pitch, fine pitch and modulation matrix.
- `base_inc`: Metal Core's stored base FREQ converted to Q0.32 at the active sample rate.

All host increments must be calculated at the same sample rate. The callback already provides final `m->inc`; `v->note` is the target note while `v->pitch_cur` carries glide state.

## 3. Selection

- PERFORMANCE source: denominator = C4 reference; FOLLOW does not alter keyboard/MIDI or arpeggiator pitch.
- SEQUENCER source + FOLLOW ON: denominator = C4 reference; step note is tracked.
- SEQUENCER source + FOLLOW OFF: denominator = target step note; sequencer's absolute note is removed, but the ratio between final host pitch and target note remains. This preserves glide and independent host pitch effects.

Selected increment is `base_inc * ratio_q24 >> 24`, rounded. Result is clamped to Q0.32 Nyquist (`0x80000000`) to prevent phase wrap above half the sample rate.

## 4. Implementation and tests

Added:
- `eng_metal_follow_inc_ratio_q24()`: bounded 64-bit intermediate division; zero reference rejected.
- `eng_metal_follow_host_phase_inc()`: source/FOLLOW selection, host pitch-delta retention and Nyquist clamp.
- Host tests for +10% pitch delta, all three source/FOLLOW combinations, zero sample rate, zero reference and Nyquist saturation.

No floating point or dynamic allocation is used by the helper. Division is at control/block rate, not per sample.

## 5. Limits

- This is an isolated math primitive, not connected to `eng_metal_core.c`, the host callback or an oscillator.
- The host adapter still needs a verified way to calculate `reference_inc` and `note_inc` using the exact target sample-rate and Felucca pitch tables.
- The absolute MIDI mapping and ratio range must be verified for all 0..127 notes, including high-note Nyquist behavior.
- A target CPU measurement and render-level tests are required before integration.
- The current CP2.9 octave clamp is not silently removed by this helper; it remains documented as a separate adapter limitation pending replacement/verification.

## 6. Scope boundary

No engine registration, parameter ID, UI, preset/storage layout, package, factory image or device state changed.
