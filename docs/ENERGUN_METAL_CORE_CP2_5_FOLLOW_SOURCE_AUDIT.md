# ENERGUN METAL CORE — CP2.5 FOLLOW source-provenance audit

Date: 2026-10-31
Branch: `energun-metal-core`
Status: source-backed design gate; no firmware source changed.

## 1. Purpose

Define the host information required to implement the approved FOLLOW behavior without confusing sequencer notes with live keyboard or MIDI performance. This audit is deliberately separate from the isolated oscillator/LPG primitives.

## 2. Source trace in the pinned Felucca host

The pinned host currently collapses several note sources into the same note API:

- `seq.c` states at file scope that keyboard, MIDI, arpeggiator and sequencer events end in `trk_note_on()` / `trk_note_off()`; engines do not see where a note came from.
- Live input reaches `trk_note_on()` through the input path (the `input_on()` path around line 680).
- Step playback also calls `trk_note_on()` from `seq_step()` (around line 1031), after the sequencer's transpose / quantize processing.
- Arpeggiator note generation calls the same `trk_note_on()` path.
- `voice.c` builds `vmod_t` per active voice. Its pitch is resolved from the voice's current pitch plus host tune, bend, LFO pitch and envelope pitch modulation; `m.inc` is then produced by `pitch_inc(m.pitch16)`.
- The pinned `felucca_engine.h` engine callback payloads (`voice_t` and `vmod_t`) contain pitch and voice state but no note-origin field.

Therefore the current engine callback cannot reliably distinguish a step-sequencer note from a live keyboard/MIDI note or an arpeggiator note.

## 3. FOLLOW implementation consequence

The signal-flow decision defines FOLLOW as sequencer note/octave tracking:

- FOLLOW ON: step-sequencer note pitch and octave changes drive the voice's oscillator-related sources.
- FOLLOW OFF: step-sequencer note pitch is bypassed and the stored base FREQ remains effective.
- The stored FREQ value and all oscillator ratios/detunes remain unchanged when FOLLOW is toggled.

Implementing this only by ignoring Felucca's `vmod_t.inc` when FOLLOW is OFF would also suppress ordinary live keyboard/MIDI pitch tracking. That would exceed the documented FOLLOW scope and would be an incorrect shortcut.

## 4. Proposed host-to-engine contract

Before implementing FOLLOW in the render path, add explicit note provenance in the host-owned path and make it available per sounding voice. The implementation must:

1. Distinguish at least step-sequencer notes from performance-origin notes.
2. Preserve provenance through fresh note allocation, mono legato pitch changes, retrigger/voice reuse, sequencer slide/tie, ratchets, note-off and voice stealing.
3. Keep the metadata host-owned; do not change the pinned Felucca-Engines submodule or the shared `voice_t` ABI merely to carry one Metal Core feature.
4. Expose the source to Metal Core in a bounded, deterministic way that is valid for every render block of that voice.
5. Keep arpeggiator and step-preview behavior explicitly classified rather than silently treating them as step-sequencer events.

Proposed V1 behavior, pending confirmation against the protected v98 reference:

- Step-sequencer source + FOLLOW ON: use the sequencer-resolved note pitch as the tracked base for OSC1, OSC2, Complex FM modulator, METAL FM modulator and METAL ring oscillator.
- Step-sequencer source + FOLLOW OFF: use the stored base FREQ instead of the step note/octave.
- Live keyboard and external MIDI performance: continue to follow their incoming note pitch regardless of FOLLOW, because the documented switch is specifically sequencer tracking.
- Arpeggiator and step preview: keep a distinct source classification until their exact FOLLOW policy is confirmed.
- Host tune, pitch bend and other pitch modulation remain independently testable; FOLLOW must not accidentally disable unrelated host modulation.

The exact frequency-anchor formula for converting the base FREQ and a sequencer MIDI note into a tracked oscillator frequency must be verified against the protected v98 source before it is frozen. Do not assume that A4/MIDI 69 is the reference anchor without that source check.

## 5. Required deterministic tests

| ID | Test | Acceptance |
|---|---|---|
| FOLLOW-01 | Step sequence, FOLLOW ON | Base oscillator tracks sequencer note and octave changes |
| FOLLOW-02 | Step sequence, FOLLOW OFF | Step pitch is bypassed; base FREQ remains effective |
| FOLLOW-03 | Live keyboard / MIDI | Incoming performance pitch remains functional with FOLLOW OFF |
| FOLLOW-04 | Value retention | Toggling FOLLOW does not alter FREQ, OSC2 detune, MOD RATIO or METAL RATIO |
| FOLLOW-05 | Toggle transition | OFF/ON during playback produces the intended next pitch without stale source state or a discontinuity |
| FOLLOW-06 | Oscillator destinations | OSC1, OSC2, Complex modulator, METAL FM and METAL ring use the same tracked fundamental before their independent ratios/detunes |
| FOLLOW-07 | Lifecycle | Source metadata remains correct through mono legato, retrigger, slide/tie, ratchet, note-off and voice reuse |
| FOLLOW-08 | FOLLOW × METAL | All four switch combinations remain independent |

## 6. Entry gate

No FOLLOW DSP helper or engine render change is made in this audit. The next implementation step is a narrowly scoped host provenance design and tests, followed by an isolated bounded pitch-selection helper. The exact v98 frequency mapping and arpeggiator/preview policy must be verified before the helper is treated as final.

No engine registration, parameter ID, UI, storage format, firmware package, factory image, Flash or physical device state changed.
