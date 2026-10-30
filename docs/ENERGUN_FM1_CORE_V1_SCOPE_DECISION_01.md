# ENERGUN FM-1 CORE — V1 scope decision 01

Date: 2026-10-31
Branch: energun-metal-core
Status: APPROVED SCOPE BASELINE

The user approved the V1 scope proposal.

## 1. V1 objective

Port the main sound-generation path of the browser reference ENERGUN METAL CORE v98 into the Felucca FM-1 engine, preserving the v98 signal architecture and characteristic sound. This is the approved scope baseline for implementation planning.

## 2. Included in V1

1. OSC 1 main carrier.
2. OSC 2 independent direct-audio oscillator mixed into the pre-fold path.
3. Dedicated Complex Oscillator modulator, with FM / AM modes, MOD RATIO, waveform, FM CHARACTER, FM INDEX, AM DEPTH, feedback, and its own modulation envelope.
4. Separate METAL FM modulator and METAL Ring Mod path.
5. Independent FOLLOW and METAL switches, presented in the OSCILLATORS / SYNTH VOICE block.
6. Two-stage WAVEFOLDER:
   - fixed first wcShaper stage, with TIMBRE controlling its input gain;
   - adjustable second wcFolder2Shaper stage, with FOLD, SYMMETRY, DRIVE, MODULE VOLUME, and SOFT / CLASSIC / HARD mode.
7. Main low-pass filter with CUTOFF and RESONANCE.
8. LPG with RESPONSE, DECAY, and MODULE VOLUME.
9. Normal voice amplitude envelope and output-level behavior, reusing Felucca controls only where exact semantics match.

## 3. Deferred to a later phase

The following v98 features are deliberately deferred, not abandoned:
- separate RING2 path (RING MIX / RING RATIO), distinct from METAL's own Ring Mod;
- full custom PATCH BAY and its full source/destination matrix;
- independent SPREAD routing until its exact reference behavior is verified;
- any extra module-volume controls that cannot be represented without changing the reference envelope/gain behavior.

Existing Felucca modulation infrastructure may later cover some PATCH BAY functions, subject to semantic verification.

## 4. Non-negotiable architecture constraints

- Preserve separate OSC 2 direct audio, Complex wcMod, METAL wcMMod, METAL wcRingOsc, and deferred RING2 wcRing2Osc roles.
- Preserve separate MOD RATIO (v98 default 3.07) and METAL RATIO (v98 default 4.20).
- Keep FOLLOW and METAL independent.
- FOLLOW controls sequencer pitch tracking; METAL controls the additional METAL FM/ring paths and METAL-specific trigger envelope behavior.
- Keep TIMBRE, WEST COAST DRIVE, and WAVEFOLDER DRIVE as distinct controls.
- Do not invent or add DSP algorithms.
- Preserve Felucca voice lifecycle and bounded per-part engine memory.
- Keep the factory firmware and the user's physical device untouched until a proven recovery path and a verified full dump from that exact device exist.

## 5. Next work sequence

1. Complete exact mapping of v98 controls to existing Felucca common parameters; reuse only semantically equivalent parameters.
2. Freeze the V1 Metal Core-specific parameter list and stable parameter IDs.
3. Verify parameter, project, preset, motion-lock, editor, and sidecar persistence design.
4. Implement a compile-only skeleton and engine registration without changing audio behavior.
5. Implement one DSP subsystem at a time, with host-side reference comparisons and tests.
6. Build and validate the firmware image; do not flash the physical FM-1 until recovery prerequisites are independently proven.

## 6. Change control

This scope is the approved baseline. Any addition, removal, or change to the included signal architecture requires explicit user approval. No DSP implementation or hardware flashing is authorized merely by approving this scope.
