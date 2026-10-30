# ENERGUN METAL CORE — FOLD DESIGN 01

**Status:** Corrected draft for user review — not approved for DSP implementation  
**Target branch:** `energun-metal-core`  
**Source references inspected:** ENERGUN West Coast v99 and v104 prototypes; v105 is the protected preferred baseline.

## 1. Important architectural correction

FOLLOW is **not part of WAVEFOLDER** and must not be placed on the FOLD page.

In the original ENERGUN Metal Core / West Coast prototype, FOLLOW is a sequencer-note tracking function:
- the sequencer sends a step object to the voice trigger;
- when FOLLOW is ON, the voice calculates oscillator frequency from the step's note and octave data;
- when the step has a valid note, semitone transposition and note octave are applied to the base frequency;
- otherwise the step's octave offset is used;
- the resulting frequency is applied to the main oscillators and related modulation/ring oscillator frequency sources.

When FOLLOW is OFF, this per-step pitch-following update is skipped. It does not bypass or alter the Wavefolder.

This behavior is visible in the original `wcTrig(p)` implementation in v99. It must be ported as sequencer-to-voice pitch tracking, not reinterpreted as an envelope follower, CV follower, or waveshaper control.

METAL is also separate from WAVEFOLDER. In the original trigger path it independently enables the metal-related frequency-ratio updates (including the METAL RATIO relationship). Do not merge its state or processing with FOLLOW or FOLD.

## 2. Recovered WAVEFOLDER structure

The v104 prototype defines a dedicated WAVEFOLDER module with the following controls:

| Original control | Actual role in the prototype |
|---|---|
| FOLD | Blend / amount of the folding transfer curve |
| SYMMETRY | Offset of the transfer curve around its center |
| DRIVE | Pre-fold input gain into the nonlinear transfer |
| MODULE VOLUME | Output level of the Wavefolder module |

The prototype also has a discrete **MODE** selector:
- **SOFT** — softer folding transfer;
- **CLASSIC** — original folding transfer;
- **HARD** — more intense folding transfer.

The original code constructs a waveshaping curve and applies the selected transfer. The SOFT path uses a repeated reflection fold followed by a tanh softening curve. CLASSIC and HARD use repeated reflection folding with different drive and symmetry scaling and different reflection limits. The final output blends the unprocessed input and folded signal according to FOLD amount.

The module includes pre-fold and post-fold waveform scopes. These are visual monitors, not extra audio-processing stages.

Therefore the recovered first-pass control set is **FOLD / SYMMETRY / DRIVE / MODULE VOLUME + MODE**. The previous proposal of BIAS and TONE was not recovered from the reference and is withdrawn.

## 3. Sequencer interaction with FOLD

FOLD can receive sequencer-synchronous modulation independently of FOLLOW.

In v99, the sequencer trigger reads the step's `foldMod` value and combines it with the motion-fold contribution, scaled by the master modulation depth. That temporary value is added to the base FOLD amount, clamped to the valid 0–1 range, and the waveshaper curve is refreshed. The temporary step contribution is then cleared after the step gate interval.

The patch-bay destination **FOLD CV** is another separate modulation route into the fold input-gain control. It is not FOLLOW.

These mechanisms must remain distinct:
- FOLLOW: sequencer note/octave → oscillator pitch tracking;
- step FOLD modulation: sequencer step data → temporary FOLD amount movement;
- FOLD CV: patch-bay modulation source → fold input gain;
- METAL: independent metal-related oscillator/modulator ratio behavior.

## 4. Signal-chain position

The approved high-level voice chain remains:

`OSCILLATORS → FM → WAVEFOLDER → LPG → OUTPUT`

The exact mix/tap points and fixed-point scaling for the FM-1 implementation must be confirmed against the Felucca engine architecture before coding. Do not use the PHASE engine's E7 fold parameter or Felucca's FX insert fold as a substitute for this dedicated Metal Core module.

## 5. Porting requirements

- Preserve the recovered controls and their meanings: FOLD, SYMMETRY, DRIVE, MODULE VOLUME, MODE.
- Preserve the SOFT / CLASSIC / HARD distinction unless a later listening test demonstrates a concrete reason to change it.
- Preserve the separate FOLLOW sequencer pitch-tracking function.
- Preserve independent METAL behavior and METAL RATIO.
- Keep step-synchronous FOLD modulation separate from FOLLOW.
- Keep the FOLD CV patch route separate from FOLLOW.
- Do not introduce a new TONE control or rename SYMMETRY to BIAS without explicit approval.
- Do not change the protected v105 reference prototype.

## 6. Stability and validation requirements

The browser prototype uses Web Audio's WaveShaper and oversampling. That implementation cannot be copied literally into Felucca's fixed-point audio engine. The transfer curves must be ported deliberately using Felucca's actual numeric conventions, CPU budget, and audio block behavior.

Required checks:
- transparent/neutral baseline and output-level consistency;
- low, middle, and high register sweeps;
- FOLD / SYMMETRY / DRIVE interaction in each mode;
- no unwanted whistle, crackle, clicks, or abrupt level changes;
- safe signal headroom and bounded internal values;
- smooth updates when sequencer step FOLD modulation changes;
- FOLLOW ON/OFF changes pitch tracking only, never FOLD behavior;
- METAL ON/OFF changes only the metal-related contribution;
- CPU and fixed-point headroom measured on target hardware.

Do not add an automatic limiter or gain control merely to conceal an unstable transfer curve.

## 7. Explicit non-goals

- No changes to Felucca's existing PHASE engine or FX insert fold.
- No generic envelope follower under the name FOLLOW.
- No merging of FOLLOW, METAL, step FOLD modulation, or FOLD CV.
- No DSP source changes in this design-document correction.
- No firmware flashing.

## 8. Approval boundary

This document restores the original prototype's control roles and routing. It is a source-based reconstruction of the browser implementation, not proof that its floating-point/Web Audio transfer curve is already suitable for the FM-1 fixed-point target.

DSP implementation remains blocked until the transfer-curve port and engine integration are audited and agreed.


## 9. FM-1 preset and modulation-routing decision

**Approved decision:** use Felucca's existing preset/project system to save and recall ENERGUN METAL CORE sounds. Do not introduce a separate ENERGUN preset manager in this phase.

**Approved non-goal:** do not port the browser prototype's standalone PATCH BAY as a separate FM-1 feature in the initial implementation. References above to the prototype's FOLD CV patch-bay route are historical source-analysis notes only; they are not an approved FM-1 requirement.

The initial FM-1 scope is the Metal Core voice and its explicitly approved controls/modulation behavior, integrated into Felucca's existing part, sequencer, modulation, effects, and preset/project architecture. Any later request for additional user-configurable patch routing requires a separate design and approval.
