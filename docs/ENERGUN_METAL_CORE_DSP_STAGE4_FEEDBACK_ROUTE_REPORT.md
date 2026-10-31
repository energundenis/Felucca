# ENERGUN METAL CORE — DSP stage 4: v98 wcMod feedback route

Date: 2026-10-31
Branch: `metal-core-dsp-stage1-osc`
Status: isolated fixed-point feedback route implemented; host CI validation in progress. Not registered as an engine and not placed into `engine_t.mem`.

## 1. Source-grounded v98 topology

The preserved Library source `energun_westcoast_synth_v98_metal_core_follow_metal_on(1).html` contains this graph in `wcInit()`:

`wcMod -> Delay(3 ms) -> Gain(FM_FEEDBACK * FREQ * 3) -> wcMod.frequency`

The direct FM path is separate:

`wcMod -> wcIndexG -> wcOsc.frequency`

Therefore feedback changes the instantaneous frequency of the **modulator wcMod**, not OSC1 directly. The modified wcMod waveform then reaches OSC1 through the normal FM INDEX path.

The v98 feedback control range is 0..0.8. Its gain changes use `setTargetAtTime(..., 0.025)`; the fixed-point implementation uses a per-sample one-pole approximation of a 25 ms transition.

## 2. Implementation

File: `firmware/src/eng_metal_core.c`

Added:
- per-voice smoothed feedback amount in `metal_core_feedback_state_t`;
- signed feedback phase-step calculation from the v98 gain equation;
- bounded signed wcMod phase increment, including negative frequency;
- direction-aware reverse BLEP handling for reverse-running wcMod SAW and SQUARE;
- a delay peek so the delayed sample determines the current wcMod phase step before the current modulator sample is generated;
- `metal_core_complex_fm_feedback_pair_block()`, which routes delayed wcMod waveform back to wcMod frequency.

The old no-feedback renderer remains unchanged. A zero-feedback regression compares output and all oscillator phases against the no-feedback renderer.

## 3. Tests

File: `tests/metal_complex_fm_test.c`

Checks:
- zero feedback is sample-exact against the prior no-feedback renderer;
- feedback has no effect before the 3 ms delay boundary;
- the first delayed feedback appears at the configured delay boundary;
- feedback changes wcMod phase after the delay;
- feedback amount remains in the v98 0..0.8 range;
- positive and negative feedback signs and signed phase-step bounds;
- pre-fold output remains within the tested oscillator headroom.

CI compiles and runs the feedback-route test at both 44.1 kHz and 96 kHz.

## 4. Explicit limits

- Standalone DSP only; `eng_metal_core.c` remains excluded from `engines.c`.
- No `engine_t.mem` integration, engine registration, UI, parameter IDs, persistence, or firmware image build.
- Note retrigger and legato reset policy is not frozen.
- The per-part memory layout is not yet allocated; the helper's standalone size assertion is not a full engine-state budget proof.
- No physical FM-1 query, SysEx, write, erase, restore, or flash operation was performed.
- Factory firmware V1.5 remains untouched.
