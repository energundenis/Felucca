# ENERGUN METAL CORE — DSP stage 2: Complex Oscillator source and curves

Date: 2026-10-31
Branch: `metal-core-dsp-stage1-osc`
Status: isolated wcMod source and fixed-point character/drive curves implemented; host tests passed; not yet routed into a registered engine.

## 1. Implemented source

File: `firmware/src/eng_metal_core.c`

### Independent Complex Oscillator modulator (wcMod)

- SINE, TRIANGLE, SAW, SQUARE.
- Uses voice phase slot `ph[2]`, independent of OSC1 and OSC2.
- MOD RATIO is supplied as Q16 and clamped to the v98 range 0.25..16.0.
- Phase increment is calculated once per block.
- The increment is capped just below Nyquist to prevent phase-step wrap.
- SQUARE is scaled to full Q15 from Felucca's half-scale pulse primitive.
- Writes a bounded CTL-sized Q15 modulator scratch block.

### Fixed-point FM CHARACTER / WEST COAST DRIVE

- Added bounded integer square root for Q15 values.
- SMOOTH: `0.72 * sqrt(index)`.
- CLASSIC: `index`.
- METALLIC: `0.55 * index + 1.15 * index^2`.
- Added `wcDrv() = WEST COAST DRIVE ^ 1.5`.
- Added effective Complex FM depth as character curve multiplied by wcDrv.
- METALLIC uses extended Q15 and correctly reaches approximately 1.70 at full index.

These are control-rate calculations. No floating point is used.

## 2. Tests

New tests:
- `tests/metal_fm_modulator_test.c`
- `tests/metal_fm_curve_test.c`

The existing `tests/metal_osc_test.c` remains in the suite.

Coverage includes waveform landmarks, square amplitude, ratio values and bounds, phase continuity, CTL bounds, integer square root, all three character curves, index/drive clamping, and the WEST COAST DRIVE power curve.

GitHub Actions:
- Run ID: `41155691495`
- Result: **success**
- All three isolated test programs compiled and passed.
- Run: https://github.com/energundenis/Felucca/actions/runs/41155691495

The new tests are also included in `tests/run_tests.sh`. The full Felucca firmware suite has not yet been run for this branch.

## 3. Not yet implemented

This stage does not yet perform audio-rate frequency modulation of OSC1. It only generates wcMod and computes the reference FM depth curves.

Still outstanding:
- route wcMod into OSC1 instantaneous frequency;
- implement FM / AM mode switching;
- implement the 3 ms feedback path and per-voice delay memory;
- implement FM ENV amount / attack / decay;
- add click-safe transitions and gain/headroom tests;
- compare low/mid/high register renders against the v98 reference;
- measure target CPU and memory cost.

No METAL FM (wcMMod), METAL Ring (wcRingOsc), wavefolder, main filter, LPG, engine registration, UI, parameter IDs, or sidecar changes are included.

## 4. Integration and safety status

- `eng_metal_core.c` is still not included by `engines.c`.
- Existing Felucca engines and the pinned Felucca-Engines submodule are unchanged.
- No firmware image was built or flashed.
- No physical FM-1 was queried or written.
- Factory firmware V1.5 remains untouched.

## 5. Next implementation gate

Before connecting FM to the carrier, define and test the signed instantaneous-frequency arithmetic, including negative instantaneous frequency and polyBLEP behavior at extreme modulation. Then implement the audio-rate FM path with feedback held at its v98 default of zero. Feedback and the FM trigger envelope can be added as separate, measured stages.
