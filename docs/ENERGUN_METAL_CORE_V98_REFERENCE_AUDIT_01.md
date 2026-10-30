# ENERGUN METAL CORE — v98 reference audit 01

Date: 2026-10-31
Branch: energun-metal-core
Status: recovered from prior project discussions; exact source-file verification is pending. This is a reference inventory, not DSP implementation approval.

## 1. Protected reference

Original filename recorded in the project history:

energun_westcoast_synth_v98_metal_core_follow_metal_on.html

v98 is the immutable sound reference. Do not overwrite it or substitute a later test build.

Reference state:
- FOLLOW: ON.
- METAL: ON.
- FREQ: 160.
- METAL RATIO: 4.20.
- TIMBRE: 0.
- DRIVE: 0.
- FM INDEX: 0.
- FM FEEDBACK: 0.

The reference target is the hard industrial metallic hit with a short metallic tail, while retaining the thick bass foundation. The values above are known reference settings; they are not a complete control inventory.

## 2. Architecture recovered from earlier v98 discussion

The described v98 voice includes:

1. Main oscillator and second oscillator.
2. FM / AM modulation paths.
3. Delayed FM feedback.
4. Two-stage wavefolder (wcShaper and wcFolder2Shaper).
5. 4× oversampling on the second wavefolder stage.
6. SOFT / CLASSIC / HARD wavefolder modes.
7. A biquad low-pass filter.
8. LPG stage.
9. Sequencer-triggered modulation and per-step control.

The approved high-level order for the FM-1 engine remains:

OSCILLATORS → FM → FOLD → LPG → OUTPUT

This is the project-level signal order. Exact internal taps, direct OSC2 mix point, and filter placement must be verified against the actual v98 source before implementation.

## 3. Do not import v99 test changes into v98

Prior discussions recorded the following v99 attack/tail test values relative to v98:

| Parameter | v98 | v99 test |
|---|---:|---:|
| METAL DECAY | 0.40 | 0.18 |
| LPG RESPONSE | 0.50 | 0.72 |
| LPG DECAY | 0.25 | 0.16 |
| FILTER CUTOFF | 1200 Hz | 3600 Hz |
| FILTER RESONANCE | 2.0 | 1.2 |

The core reference values remained FREQ 160, METAL RATIO 4.20, TIMBRE 0, DRIVE 0, FM INDEX 0, and FM FEEDBACK 0.

A separate v99 filter experiment was also described as changing the v98 single 12 dB/oct low-pass into two serial low-pass biquads (24 dB/oct) with softer nonlinear Q scaling. Treat that as a separate experiment, not as part of the protected v98 architecture.

## 4. WAVEFOLDER control inventory — verified directly in v98

The exact v98 HTML is now available in the project Library and has been inspected.

The dedicated WAVEFOLDER module contains, in order:

1. FOLD — blend between the unprocessed input and the second-stage folded transfer; range 0..1; default 0.25.
2. SYMMETRY — shifts the second-stage transfer curve; range -1..1; default 0.
3. DRIVE — pre-fold gain inside the second-stage transfer; range 0..1; default 0.20.
4. MODULE VOLUME — second-stage output level; range 0..1; default 1.
5. MODE selector — SOFT / CLASSIC / HARD; default CLASSIC.

The two-stage structure is confirmed:
- Stage 1 is the fixed wcShaper reflection curve, generated over x=-2..+2 with up to four reflections. Its input gain is controlled by TIMBRE (1 + TIMBRE × 6); TIMBRE is not the same control as the WAVEFOLDER DRIVE.
- Stage 2 is wcFolder2Shaper, with the adjustable FOLD / SYMMETRY / DRIVE transfer curve and 4× oversampling.
- A unity-gain stage sits between the two shapers.
- MODULE VOLUME is after the second shaper.
- Pre-fold and post-fold scopes are visual monitors, not additional DSP stages.

The separate WEST COAST DRIVE control is also not the same as WAVEFOLDER DRIVE: it scales the FM amount and the METAL FM contribution through wcDrv(). Keep these three controls distinct: TIMBRE (stage-1 input gain), WEST COAST DRIVE (FM scaling), and WAVEFOLDER DRIVE (stage-2 fold drive).

The v98 source therefore confirms the exact WAVEFOLDER control labels, order, ranges, defaults, modes, and two-stage structure. The v104 evidence is no longer needed to establish this part of the design.

## 5. FOLLOW and METAL behavior

The saved v99 reference copy is explicitly based on v98 and provides code evidence for the two independent switches:

- FOLLOW ON: sequencer note/octave updates the main oscillators and related modulation/ring oscillator frequency sources.
- FOLLOW OFF: skips sequencer pitch tracking; the base FREQ remains.
- METAL ON: enables the additional METAL FM and Ring Modulation contributions and selects the METAL-specific trigger attack/decay behavior.
- METAL OFF: bypasses those added METAL contributions while retaining METAL RATIO and METAL DECAY values.

Current UI decision: both switches live in the OSCILLATORS / SYNTH VOICE block, not on the FOLD page.

## 6. Required source verification

Before finalizing the parameter/page map:
1. Obtain the exact v98 HTML file named above.
2. Verify its WAVEFOLDER controls, mode selector, ranges, defaults, and DSP transfer functions.
3. Verify the v98 oscillator, FM/AM, feedback, filter, LPG, and modulation control lists.
4. Compare the actual v98 source with the v99 attack/tail test and the separate v99 filter experiment.
5. Freeze a control inventory before adding parameter IDs or implementing DSP.

No source file was modified, no DSP was implemented, and no firmware or physical device was written by this audit.
