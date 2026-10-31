# ENERGUN METAL CORE — CP2.10 host-to-engine FOLLOW pitch contract audit

Date: 2026-10-31
Branch: `energun-metal-core`
Status: source audit and integration contract; no render integration.

## 1. Confirmed host render sequence

Source inspected at the current branch HEAD:

- `firmware/src/voice.c` — `track_render()`
- `firmware/src/mod.c` — `mod_voice()`
- `firmware/src/core.h` — `track_t` and host-owned voice-source metadata

For each active host voice, `track_render()` constructs `vmod_t` before calling the engine render callback:
1. Host glide updates `v->pitch_cur`.
2. Track tuning, pitch bend, LFO pitch and envelope pitch are added.
3. `m.pitch16` and `m.inc` are calculated.
4. Fine tuning and residual pitch bend are applied through `m.fine` / `m.inc`.
5. If the modulation matrix is active, `mod_voice()` adds matrix pitch and recalculates `m.pitch16` and `m.inc`, preserving fine pitch.
6. The engine receives `render(t, v, out, n, &m)`.

The callback therefore has both:
- `t`: track-level state, including `voice_source[NVOICE]`;
- `v`: the exact voice being rendered;
- `m`: the final host-resolved pitch and modulation state for this block.

The source tag is host-owned at `t->voice_source[v - t->v]`; it does not require modifying the shared `voice_t` ABI. This access is valid only for a voice pointer belonging to that track's `t->v[]` array.

## 2. Pitch components that must remain independent

The final host pitch includes more than the sequencer's MIDI note:
- glide / legato transition;
- track tune;
- MIDI pitch bend;
- track LFO pitch;
- envelope pitch;
- modulation-matrix pitch;
- fine tuning / residual bend.

FOLLOW must distinguish the *note-origin pitch* from these independent pitch offsets.

## 3. Required selection behavior

For a Metal Core render voice:

- PERFORMANCE source: use the host-resolved note pitch. FOLLOW must not change live keyboard/MIDI or arpeggiator pitch.
- SEQUENCER source + FOLLOW ON: use the Metal Core base FREQ plus the mapped sequencer NOTE/OCTAVE ratio.
- SEQUENCER source + FOLLOW OFF: use stored Metal Core base FREQ, not the last sequencer note's absolute pitch.
- In both sequencer cases, preserve independent host pitch offsets (glide, tune, bend, LFO, envelope and modulation matrix) unless a later approved Metal Core-specific control explicitly overrides them.
- The note-source tag is per voice and must follow POLY, MONO, LEGATO, UNISON, preview and engine-switch queue behavior.

A direct selection between a bare `base_inc` and final `m.inc` is insufficient if it drops the independent pitch offsets. The integration must derive or receive the host pitch delta separately, then apply that delta to the selected Metal Core base/step frequency. The exact fixed-point method and target cost remain to be designed and tested.

## 4. CP2.9 MIDI-range limitation discovered

The current adapter maps MIDI 60 (C4) to base FREQ and clamps NOTE OCTAVE to -2..+2. This means MIDI notes below C2 or above C6 are octave-clamped and do not preserve their original pitch. The endpoint tests currently verify the clamp, not full MIDI-range tracking.

Before using this adapter in the render path:
- either extend the fixed-point ratio range for the full required MIDI span and define Nyquist / maximum-frequency behavior;
- or explicitly reject/fallback for out-of-range notes without silently changing their pitch.

The current adapter is therefore only a bounded prototype. This does not invalidate its in-range arithmetic tests, but it prevents claiming complete MIDI 0..127 mapping.

## 5. Validation required before render integration

- Unit tests for source selection and preservation of host pitch deltas.
- Tests with FOLLOW ON/OFF crossed with pitch bend, tune, glide, LFO pitch, envelope pitch and modulation-matrix pitch.
- Verify live keyboard/MIDI and arpeggiator remain host-pitched.
- Verify step preview is sequencer-pitched.
- Verify no stale sequencer pitch after FOLLOW OFF, stop, engine switch or voice reuse.
- Verify all supported MIDI notes and explicit behavior outside the selected Metal Core pitch range.
- Measure target instruction cost before approving per-block pitch-ratio math.

## 6. Scope boundary

No engine render callback, engine registration, parameter ID, UI, preset/storage layout, package, factory image or device state changed by this audit. CP2.8 and CP2.9 still require a successful CI run at their implementation HEAD; CP2.9 additionally requires resolution of the range and host-pitch-delta items above.
