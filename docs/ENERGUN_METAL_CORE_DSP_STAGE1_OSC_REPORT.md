# ENERGUN METAL CORE — DSP stage 1: OSC pair

Date: 2026-10-31
Branch: `metal-core-dsp-stage1-osc`
Status: isolated fixed-point subsystem implemented; host test passed; not yet registered as a playable engine.

## 1. Reference roles preserved

The implementation follows OSC design 02 and the approved V1 scope:

- OSC 1 is the main carrier.
- OSC 2 is an independent direct-audio oscillator.
- OSC 2 is mixed into the shared pre-fold input.
- OSC 2 is not used as the Complex FM modulator.
- The Complex FM source (wcMod), METAL FM source (wcMMod), and METAL ring source (wcRingOsc) remain separate future subsystems.

## 2. Code implemented

File: `firmware/src/eng_metal_core.c`

Implemented static functions:

- `metal_core_osc_sample()`: SINE / TRIANGLE / SAW selection using Felucca's fixed-point oscillator primitives. SAW uses the existing polyBLEP implementation.
- `metal_core_osc2_increment()`: OSC2 cents detune with the host's fine pitch contribution; zero detune reuses the exact host increment.
- `metal_core_osc_pair_block()`: renders both direct-audio oscillators, mixes OSC2 by Q15 level, advances both phase accumulators, and bounds work to CTL samples.

The subsystem:
- uses voice phase slots `ph[0]` and `ph[1]`;
- performs no allocation or floating-point work;
- does not apply voice ADSR, velocity, VCA, filter, TIMBRE, wavefolder, or output gain;
- produces the raw pre-fold oscillator sum, bounded to approximately +/-65535.

## 3. Tests

File: `tests/metal_osc_test.c`

Coverage:
- sine, triangle, and saw waveform landmarks;
- unknown waveform fallback;
- OSC2 level zero leaves OSC1 sample-exact;
- OSC2 phase advances at zero level;
- full-level OSC2 mixing and headroom bounds;
- OSC2 detune and host fine tuning;
- CTL render bound and no write past the block;
- zero-length and null-argument behavior.

GitHub Actions:
- Workflow: `.github/workflows/metal-core-osc-stage1.yml`
- Run ID: `41155060811`
- Result: **success**
- Compile and test steps both completed successfully.
- Run: https://github.com/energundenis/Felucca/actions/runs/41155060811

The same test is also added to `tests/run_tests.sh` for the full Felucca host test suite.

## 4. Deliberate limitations / next gates

This is not yet the complete OSC stage or a playable engine:

1. No engine registration or changes to `engines.c`.
2. No parameter IDs or descriptors have been assigned.
3. The v98 base FREQ range and FOLLOW ON/OFF pitch policy are not yet applied; this first primitive receives the host phase increment. Their exact mapping must be resolved before engine integration.
4. OSC2 detune/level values are inputs to the helper, not yet connected to persistent controls.
5. Triangle currently uses Felucca's existing triangle primitive. High-register aliasing must be measured against the v98 reference before sound acceptance.
6. No Complex FM, METAL, wavefolder, main filter, LPG, note lifecycle, or final gain stage is implemented here.

## 5. Safety and compatibility

- Existing Felucca engines and the pinned Felucca-Engines submodule were not modified.
- The source file is not included by `engines.c`; this work cannot change existing engine behavior.
- No firmware image was built or flashed.
- No physical device was queried or written.
- Factory firmware V1.5 remains untouched.

## 6. Next DSP stage

After completing the remaining OSC control mapping and validating the oscillator behavior, proceed to the dedicated Complex Oscillator source (wcMod), keeping FM and AM modes, MOD RATIO, waveform, CHARACTER, INDEX, feedback, and the FM modulation envelope separate from OSC2 and METAL.
