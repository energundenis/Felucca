# ENERGUN METAL CORE — FOLD DESIGN 01

**Status:** Draft for user review — not yet approved for DSP implementation  
**Target branch:** `energun-metal-core`  
**Scope:** FOLD stage and the independent FOLLOW / METAL enable controls

## 1. Design intent

The FOLD stage must extend the established ENERGUN METAL CORE voice without replacing its existing identity. The reference character is a weighty, usable metal bass/lead with a stable low-frequency foundation and controllable upper harmonics.

The first implementation must prioritize:
- stable bass fundamentals;
- gradual, musically useful harmonic growth;
- no uncontrolled high-frequency whistle, crackle, clicks, or harsh overload;
- predictable behavior across low, middle, and high registers;
- smooth parameter changes and safe signal headroom.

This document defines a conservative control and integration contract. It does **not** claim that the exact original Metal Core transfer function has already been recovered or implemented.

## 2. Signal-chain position

Approved high-level chain:

`OSCILLATORS → FM → FOLD → LPG → OUTPUT`

FOLD receives the combined oscillator/FM signal and passes its processed result to LPG. The exact oscillator mix point and internal fixed-point scaling must be confirmed during the DSP source audit before coding.

FOLD must not be confused with:
- the PHASE engine's existing E7 fold parameter;
- Felucca's global/insert FX fold effect.

Those are separate existing features and must remain untouched.

## 3. FOLD page controls

Proposed four primary controls:

| Control | Purpose | Initial implementation constraint |
|---|---|---|
| FOLD | Main waveshaping amount | Start at neutral; bounded, monotonic control response; no abrupt gain jump |
| BIAS | Symmetry / harmonic-balance offset | Center position is neutral; bounded around neutral |
| DRIVE | Level entering the nonlinear stage | Conservative range; must not be used as an uncontrolled output-volume boost |
| TONE | Post-fold spectral tilt / brightness | Neutral center; must not create narrow resonant peaks or self-oscillation |

These are functional descriptions, not a commitment to a specific waveshaper, transfer curve, or numeric range. The final numeric mapping depends on the fixed-point engine conventions and headroom audit.

### Neutral state

The neutral FOLD state must be effectively transparent: no unintended gain change, DC offset, or audible coloration when FOLD is at minimum and BIAS is centered. DRIVE and TONE must also have documented neutral positions.

The FOLD stage must not silently normalize or compensate the user's level in a way that changes the established voice balance.

## 4. FOLLOW and METAL switches

FOLLOW and METAL remain independent ON/OFF controls on the FOLD page.

### Required behavior

- Each switch has its own persistent state.
- OFF bypasses only the contribution associated with that switch.
- OFF must not clear, overwrite, or reset any associated parameter value.
- Turning ON restores processing using the retained value.
- Switching must not produce clicks, discontinuities, stuck state, or a sudden level jump.
- UI state must visibly distinguish ON from OFF.
- FOLLOW must not implicitly switch METAL, and METAL must not implicitly switch FOLLOW.

### DSP definition still required

The exact signal tap, control source, transfer function, and parameter set for FOLLOW have not yet been specified. The exact processing role and transfer function for METAL have not yet been specified either.

Do not infer these algorithms from their names, and do not substitute a generic envelope follower or generic metallic waveshaper without approval. Before implementation, define each feature's:
1. input/tap point;
2. control or modulation source, if any;
3. processing operation;
4. exposed parameter(s);
5. neutral and bypass behavior;
6. smoothing and transition behavior;
7. expected audible result and regression tests.

Until those definitions are approved, the switches are an interface/state contract only; they are not permission to invent DSP behavior.

## 5. Stability and signal-integrity requirements

The implementation must be designed around the existing Felucca fixed-point audio path and its actual numeric conventions.

Required safeguards to investigate and specify before implementation:
- bounded internal signal levels at every nonlinear stage;
- explicit headroom budget before and after waveshaping;
- DC-offset control where the chosen algorithm requires it;
- parameter smoothing or block-safe interpolation;
- safe handling of extreme parameter combinations;
- no denormal/undefined arithmetic assumptions imported from floating-point desktop DSP;
- no allocation, blocking, or unbounded work in the audio interrupt;
- no new clipping stage hidden behind the DRIVE control.

Do not add a limiter or automatic gain control merely to conceal an unstable transfer function. Any output-level compensation must be measured, documented, and separately approved.

## 6. Proposed test sequence

1. **Transparent baseline:** FOLD neutral/bypassed; verify the existing oscillator/FM signal remains unchanged in level and character.
2. **Amount sweep:** raise FOLD slowly at low, middle, and high notes; check harmonic growth and bass retention.
3. **Bias sweep:** test both sides of center at restrained FOLD values; check DC offset, asymmetry, and level jumps.
4. **Drive sweep:** test only after the internal headroom budget is measured; reject settings that create uncontrolled harshness.
5. **Tone sweep:** verify smooth spectral change without narrow whistling peaks.
6. **Switch matrix:** FOLLOW and METAL independently ON/OFF in all four combinations; verify retained values and no clicks.
7. **Register and sequence tests:** sustained low notes, high notes, fast note changes, and repeated sequences.
8. **Regression with FM:** repeat the tests with FM amount at zero and at several moderate values, including the approved 4.20:1 ratio landmark.
9. **Resource checks:** measure target CPU load and worst-case fixed-point headroom on the actual target, not only in the host golden-render harness.

## 7. Acceptance criteria

FOLD DESIGN 01 can move to implementation only after the user approves the control behavior and the unresolved FOLLOW/METAL definitions are settled.

A DSP build is not accepted until:
- neutral FOLD preserves the established clean baseline;
- the low-frequency foundation remains usable as harmonics increase;
- no unwanted whistle, crackle, clicks, or abrupt level jumps are heard;
- FOLLOW and METAL operate independently and retain values while OFF;
- all tests pass on the target hardware with adequate CPU and numeric headroom.

## 8. Explicit non-goals for this revision

- No changes to Felucca's existing PHASE engine or FX fold effect.
- No new oscillator, noise source, hard sync, ring modulation, or unison.
- No speculative implementation of FOLLOW or METAL.
- No DSP source changes in this design-document commit.
- No firmware flashing.

## 9. Decisions needed for approval

Please review these points:
1. Are FOLD, BIAS, DRIVE, and TONE the right four controls for the FOLD page?
2. Should the FOLD amount control begin with a very restrained range, expanding only after listening tests?
3. What should FOLLOW actually track or respond to in the established Metal Core design?
4. What is the intended audible role of METAL, distinct from ordinary FOLD amount?

Items 3–4 are intentionally left open rather than guessed.
