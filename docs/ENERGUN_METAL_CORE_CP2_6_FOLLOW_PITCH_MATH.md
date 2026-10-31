# ENERGUN METAL CORE — CP2.6 isolated FOLLOW pitch math

Date: 2026-10-31
Branch: `energun-metal-core`
Status: implemented as an isolated host-tested primitive; not connected to the engine.

## 1. Reference-copy mapping

The available v99-derived reference-copy evidence defines:

- Explicit NOTE: `FREQ × 2^(NOTE / 12) × 2^(NOTE OCTAVE)`.
- ROOT: `FREQ × 2^OCTAVE`.
- NOTE is a chromatic semitone offset 0..11 from base FREQ, not an absolute MIDI note.
- Octave values are represented in tenths of an octave; the source UI changes them in 0.2-octave increments.

Direct v98 source verification, host absolute-MIDI-to-relative-note adaptation, and the MOD RATIO discrepancy remain open. This helper accepts the already-resolved relative note offset; it does not decide how a Felucca step MIDI note becomes that offset.

## 2. Implementation

Added `firmware/src/eng_metal_core_follow.h`:

- Q24 lookup tables for 12 equal-tempered semitone ratios and 41 octave ratios spanning -2.0..+2.0 octaves.
- ROOT sentinel `-1`; explicit NOTE range 0..11.
- Integer octave input in tenths; out-of-range octave values clamp to the documented range.
- Base FREQ clamps to 30..500 Hz.
- Converts base frequency to Q0.32 phase increment using the selected sample rate.
- Uses 64-bit intermediates and integer arithmetic only; no floating point, allocation or division in a sample loop. The sample-rate division occurs only when the note increment is calculated.
- Includes a source-aware selector: FOLLOW OFF selects base FREQ only for step-sequencer events; performance-origin notes retain their host-resolved pitch.

Added `tests/metal_core_follow_test.c` and integrated it into `tests/run_tests.sh`.

## 3. Test coverage

- ROOT unity, octave up/down.
- Equal-tempered semitone ratios (G and B offsets).
- Fractional octave ratio (0.2 octave).
- Maximum documented range.
- Invalid-note rejection and octave/FREQ clamping.
- 44.1 kHz and 32 kHz phase-increment conversion.
- Nyquist bound for the tested maximum base-note mapping.
- FOLLOW ON/OFF selection for sequencer and performance note sources.

## 4. Boundaries

This is a deterministic math primitive only:

- No host note-source metadata has been added.
- No conversion from Felucca's absolute MIDI step note to the Metal Core relative NOTE/OCTAVE representation has been implemented.
- No oscillator, FM, METAL, filter, LPG or render callback consumes this helper.
- No engine registration, parameter ID, UI, persistence, storage, package, factory firmware or device state changed.
- Target CPU cost, target compiler behavior and direct v98 parity remain unverified.

The next gate is the host-owned note-provenance adapter and the absolute-MIDI-to-base-relative mapping specification, followed by isolated tests before any render integration.
