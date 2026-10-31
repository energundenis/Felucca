# ENERGUN METAL CORE — CP1.7 DSP verification and acceptance matrix

Date: 2026-10-31
Branch: energun-metal-core
Status: test specification; no DSP implementation and no test results claimed.

## 1. Purpose and current boundary

This matrix defines the evidence required before ENERGUN METAL CORE is registered as a playable engine. The current `firmware/src/eng_metal_core.c` remains an isolated scaffold and `tests/metal_core_scaffold_test.py` verifies only that it stays unregistered. That guard is not an audio test.

The reference is the protected v98 browser implementation. Browser/Web Audio output is a behavioral reference, not a bit-exact fixed-point oracle. Fixed-point tolerances must be calibrated using deterministic host renders and listening comparisons.

## 2. Test layers

| Layer | Where | What it proves | What it does not prove |
|---|---|---|---|
| T0 static/contract | Python source tests | bounds, no allocation, no accidental registration, state sizes | audible quality |
| T1 deterministic DSP unit | host C tests | transfer curves, phase, envelope math, saturation, state bounds | target CPU / hardware output |
| T2 engine host integration | Felucca host simulator | additive mix, note lifecycle, retrigger, voice stealing, engine switching | target timing and analog output |
| T3 target build/static cost | JieLi build + disassembly | compiler compatibility, target instruction budget, memory fit | actual real-time margin |
| T4 device validation | physical FM-1 | real-time audio, controls, persistence, recovery, noise/artifacts | untested future configurations |

A layer cannot substitute for a later layer.

## 3. DSP unit matrix

### Oscillators and pitch

| ID | Test | Stimulus | Acceptance |
|---|---|---|---|
| OSC-01 | Phase accumulator bounds | min/max frequency, all supported waveforms, long render | no signed overflow, wrap is defined, phase remains bounded |
| OSC-02 | Pitch accuracy | low/mid/high notes and semitone sweeps | frequency error within declared fixed-point tolerance; no register-dependent jumps |
| OSC-03 | FOLLOW ON | sequencer notes, octave offsets, empty/rest steps | OSC1, OSC2, Complex modulator, and METAL oscillators track the v98 note mapping |
| OSC-04 | FOLLOW OFF | same sequence with FOLLOW disabled | per-step pitch updates stop; base FREQ retained |
| OSC-05 | FOLLOW state retention | toggle OFF/ON around pitch changes | no loss of FREQ or ratio values; ON resumes correct tracking |
| OSC-06 | OSC2 separation | OSC1-only, OSC2-only, both | OSC2 is direct audio, not the Complex FM modulator; correct balance and no accidental doubling |

### Complex FM / AM / feedback

| ID | Test | Stimulus | Acceptance |
|---|---|---|---|
| FM-01 | FM character curves | index 0..1 for SMOOTH/CLASSIC/METALLIC | monotonic, bounded curves match documented formulas within quantization tolerance |
| FM-02 | FM depth scaling | vary FM INDEX and WEST COAST DRIVE independently | INDEX=0 or DRIVE=0 removes Complex FM contribution; controls are not aliased |
| FM-03 | Modulator identity | vary MOD RATIO and OSC2 independently | only dedicated Complex modulator frequency changes with MOD RATIO |
| FM-04 | AM mode | sweep AM DEPTH, compare FM mode | mode switch routes amplitude modulation, does not leave stale FM amount |
| FM-05 | Feedback boundedness | FM FEEDBACK 0..0.8, low/high pitch, long render | no unstable growth, state remains bounded, no NaN-equivalent or overflow |
| FM-06 | FM transient envelope | trigger at several gate lengths and ENV settings | envelope moves Complex FM amount only; no unintended VCA/LPG modulation |
| FM-07 | mode transition | switch FM↔AM at nonzero modulation | no discontinuous output spike; smoothing or zero-cross-safe transition |

### METAL and ring modulation

| ID | Test | Stimulus | Acceptance |
|---|---|---|---|
| MET-01 | METAL FM path | METAL ON, ratio sweep | independent sine modulator affects OSC1 as documented |
| MET-02 | METAL ring tap | compare METAL OFF/ON with stable carrier | ring modulation occurs post-VCA, not before folder |
| MET-03 | METAL bypass | toggle METAL at nonzero ratio/decay | only METAL paths and trigger envelope selection change |
| MET-04 | METAL retention | OFF, edit/retain ratio and decay, ON | values persist while OFF and restore on ON |
| MET-05 | FOLLOW × METAL | all four ON/OFF combinations during sequencer pitch changes | independent switches; correct frequency updates in each combination |
| MET-06 | ring modulation bounds | max ratio/depth and high register | bounded output, no overflow, no unexpected DC or level jump |

### Wavefolder and gain staging

| ID | Test | Stimulus | Acceptance |
|---|---|---|---|
| FOLD-01 | Stage-1 transfer | TIMBRE sweep, OSC1/OSC2 isolated | fixed reflection curve bounded and correctly driven by TIMBRE |
| FOLD-02 | Stage-2 transfer | SOFT/CLASSIC/HARD; FOLD, SYMMETRY, DRIVE sweeps | modes remain distinct, bounded, smooth; no discontinuities |
| FOLD-03 | Oversampling/aliasing | high-register sine and FM-rich input | alias energy measured against declared baseline; no unstable high-frequency artifacts |
| FOLD-04 | Module gains | COMPLEX MODULE VOLUME and WAVEFOLDER MODULE VOLUME | each affects only its documented signal branch |
| FOLD-05 | TIMBRE vs DRIVE | vary one at a time | TIMBRE is stage-1 input gain; WAVEFOLDER DRIVE is stage-2 drive |
| FOLD-06 | mixed oscillators | OSC1+OSC2 with max safe TIMBRE | no unintended doubling or fixed-point overflow |
| FOLD-07 | gain calibration | stable v98 output scale ×5 | output reference level documented; no transient ×12 writer |

### Filters, LPG and VCA

| ID | Test | Stimulus | Acceptance |
|---|---|---|---|
| LPG-01 | Main filter | cutoff/resonance sweep | bounded stable filter; no coefficient instability or denormal-like stalls |
| LPG-02 | LPG filter | short/long gates and RESPONSE sweep | transient brightness returns to base cutoff; no resonant spikes |
| LPG-03 | LPG gain/envelope | DECAY and MODULE VOLUME sweep | reproduce approved v98 shared/independent gain semantics; no competing automation |
| LPG-04 | VCA normal envelope | ATTACK/DECAY/SUSTAIN/RELEASE, gate lengths | internal contour follows v98 timing without owning host voice lifetime |
| LPG-05 | METAL envelope | METAL ON/OFF with repeated triggers | 2 ms METAL attack and METAL DECAY selection; normal envelope values retained |
| LPG-06 | LPG × VCA interaction | independent envelope extremes | bounded multiplication, no clicks at gate/release transitions |
| LPG-07 | retrigger/legato | repeated note, mono legato, release/retrigger | internal envelopes follow host lifecycle contract and do not hang voices |

**Open semantic blocker:** CP1.5 source audit found the v98 LPG knob and per-trigger envelope writing the same gain AudioParam; LPG design 02 describes MODULE VOLUME as an independent gain. This conflict must be resolved from the exact source and an explicit approved behavior decision before LPG-03 can have a final expected waveform. Do not silently choose one interpretation.

## 4. Whole-engine and host integration matrix

| ID | Test | Acceptance |
|---|---|---|
| INT-01 | Additive render | engine adds to host mix; never clears/overwrites existing samples |
| INT-02 | Stereo host path | correct stereo routing and no unapproved SPREAD behavior |
| INT-03 | note_on / note_off | no stuck notes; release follows host voice lifecycle |
| INT-04 | voice steal / engine switch | no stale state, no out-of-bounds memory, clean fade/reinitialization |
| INT-05 | block-size invariance | equivalent output within tolerance across supported CTL block boundaries |
| INT-06 | zero-input / silence | silence after release; no persistent DC or noise floor beyond declared bound |
| INT-07 | all controls extremes | bounded output across randomized valid parameter combinations |
| INT-08 | deterministic reset | same parameters + same event stream produce identical host render |
| INT-09 | parameter bypass retention | FOLLOW/METAL OFF retains associated amounts; ON restores them |
| INT-10 | state isolation | separate tracks/parts do not leak oscillator/envelope/delay state |

## 5. Health, numerical and performance gates

Mandatory measurements:
- peak and RMS by stage and final output;
- clipping/saturation count and duration;
- DC offset and post-release residual;
- max absolute state value for phase, filter, delay, envelope and feedback;
- alias-energy comparison for high-register tests;
- host CPU instructions/sample and target disassembly loop cost;
- per-part persistent memory and per-voice state bytes;
- worst-case mix with maximum supported active voices and effects.

Acceptance gates:
1. No undefined signed overflow, out-of-range table access, divide-by-zero or unbounded feedback.
2. All DSP state and output clamps have documented numeric formats and limits.
3. No voice remains allocated after the host release lifecycle completes.
4. CPU and memory fit are measured on the target compiler/build, not estimated from host tests.
5. The engine must not exceed the existing target budget; if new hot loops are added, add their symbols to `tests/target_budget.py` and a justified baseline only after measured review. Do not raise global tolerance to make a failure pass.
6. Any audible nonlinearity is intentional and reference-compared; no automatic limiter/AGC may be added merely to hide an unstable stage.

## 6. Regression fixture set

Create deterministic fixture renders for:
- OSC1 clean sine/saw/triangle/square at low, middle and high registers;
- OSC2-only and dual-oscillator mix;
- Complex FM at each character and at several ratios;
- AM at shallow/medium/full depth;
- feedback at 0, midpoint and max;
- METAL OFF/ON and FOLLOW × METAL state combinations;
- each folder mode at neutral and aggressive settings;
- filter/LPG/VCA envelope combinations;
- worst-case complete patch.

Each fixture records source revision, sample rate, block size, event sequence, parameter snapshot, output hash, peak/RMS/DC and test tolerance. Golden hashes are used only for deterministic fixed-point builds; perceptual comparison metrics are used when compiler or architecture differences legitimately alter low bits.

## 7. Current status

- Existing scaffold guard: present; proves only isolation/unregistered status.
- Existing Felucca generic host, audio, CPU and target-budget suites: available and must remain passing.
- ENERGUN-specific DSP unit tests: not implemented.
- ENERGUN-specific host integration tests: not implemented.
- Target CPU budget symbol: not yet registered because there is no render function.
- Physical FM-1 tests: not started.
- No firmware engine registration, parameter IDs, DSP implementation, storage changes, Flash writes or device flashing.

## 8. Entry criteria for DSP implementation

Before first DSP source implementation:
1. Approve the 37-control inventory and final 8/16/13 access allocation.
2. Resolve LPG MODULE VOLUME semantics.
3. Freeze exact fixed-point formats, oscillator table/phase conventions, filter topology and fold transfer approximations.
4. Define deterministic test vectors and numerical tolerances.
5. Confirm per-part and per-voice memory fit.
6. Keep the engine isolated until host tests pass; register only after a real render implementation exists.
