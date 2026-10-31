# ENERGUN METAL CORE — CP2.9 absolute MIDI to relative FOLLOW mapping

Date: 2026-10-31
Branch: `energun-metal-core`
Status: isolated adapter implemented; host provenance CI validation pending.

## 1. Host source semantics

Felucca sequencer steps store and emit absolute MIDI note numbers (0..127). Before a step reaches the engine, host processing may apply the track's transpose and sequence quantization. The source-aware note-on path therefore receives the final absolute note that should sound, not the raw pattern byte.

The reference-copy-derived Metal Core pitch formula is relative to base FREQ:
- NOTE is a chromatic semitone offset 0..11.
- NOTE OCTAVE is a separate octave offset.
- ROOT mode is a separate base-frequency octave selection.
- P_ROOT in Felucca is a scale/key setting and is not the Metal Core pitch anchor.

## 2. Explicit V1 adapter convention

The adapter uses Felucca's keyboard convention: MIDI 60 = C4. Therefore:
- MIDI 60 maps to NOTE C (0), NOTE OCTAVE 0; its frequency is exactly stored base FREQ.
- MIDI 61..71 map to semitone offsets 1..11 in the same octave.
- MIDI 72 maps to NOTE C, NOTE OCTAVE +1.
- MIDI 59 maps to NOTE B, NOTE OCTAVE -1.

This is an explicit integration convention for translating Felucca's absolute MIDI representation into the v99-derived relative NOTE/OCTAVE representation. It is not a claim that the protected original v98 source has been directly verified. MIDI 69 is not used as the base anchor.

## 3. Implementation

Added `eng_metal_follow_from_midi()` to `firmware/src/eng_metal_core_follow.h`:
- validates MIDI 0..127 and output pointers;
- uses floor division for negative offsets so NOTE always remains 0..11;
- clamps octave to the isolated helper's documented -2..+2 range;
- returns NOTE semitone and octave in tenths, ready for `eng_metal_follow_ratio_q24()`;
- performs no floating point, allocation, or sample-loop work.

Added deterministic host tests for C4, C#4, B4, C5, B3, C3, both octave clamps, and invalid inputs.

## 4. Runtime integration contract

The adapter must consume the final host-resolved absolute MIDI note associated with each sounding voice:
- step-sequencer source + FOLLOW ON: convert the note through this adapter and apply the relative pitch ratio to base FREQ;
- step-sequencer source + FOLLOW OFF: select stored base FREQ (no mutation of the FREQ parameter);
- performance-origin notes: preserve host-resolved performance pitch regardless of FOLLOW;
- arpeggiator remains performance-origin for V1;
- step preview is sequencer-origin because it uses the same step playback path.

The host note-source metadata and this math helper are not yet connected to `eng_metal_core.c` or an engine render callback.

## 5. Remaining gates

- Confirm direct v98 behavior when the original protected source becomes available; this adapter is based on the available v99 experimental copy and the host's C4 convention.
- Run the full target build and host regression at the resulting branch HEAD, including the CP2.8 source-provenance tests and new adapter tests.
- Add render-level FOLLOW ON/OFF, pitch bend/tune interaction, voice lifecycle, and all oscillator destination tests before engine integration.
- Do not mark Phase 2 complete, register the engine, package firmware, or flash the FM-1 on the strength of these isolated tests.

## Review note — full MIDI range and pitch modulation remain open (2026-10-31)

A follow-up audit identified two limits that must not be mistaken for validated behavior:

1. `eng_metal_follow_from_midi()` clamps the octave to -2..+2. Thus it does not preserve the full MIDI 0..127 pitch range relative to C4: notes below C2 or above C6 are octave-clamped. Existing endpoint tests prove deterministic clamping only; they do not prove correct full-range MIDI tracking. Before runtime integration, either expand the supported ratio range with measured Nyquist handling, or define an explicit out-of-range fallback/rejection. Do not silently treat the clamped result as the original MIDI pitch.
2. Felucca's final `vmod_t` pitch increment includes glide, track tune, pitch bend, LFO pitch, envelope pitch and modulation-matrix pitch/fine. Selecting a bare base-FREQ increment when FOLLOW is OFF would also discard those independent pitch controls. The host pitch delta must be separated from the note-origin pitch and reapplied to the selected Metal Core base/step frequency.

These are integration blockers. CP2.9 remains a bounded prototype adapter, not a complete MIDI-range or pitch-control solution.
