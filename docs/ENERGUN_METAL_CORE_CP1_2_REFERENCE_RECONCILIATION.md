# ENERGUN METAL CORE — CP1.2 reference control and signal-map reconciliation

Date: 2026-10-31
Branch: energun-metal-core
Status: reconciliation audit; DSP and parameter allocation remain unapproved.

## 1. Confirmed oscillator roles

The v98-faithful design documents establish five separate oscillator/modulator roles plus the optional RING2 path:
- `wcOsc`: OSC 1 main carrier.
- `wcOsc2`: OSC 2 direct-audio source mixed into the shared pre-fold input.
- `wcMod`: independent Complex FM/AM modulator.
- `wcMMod`: METAL FM modulator, enabled only while METAL is ON.
- `wcRingOsc`: METAL ring-mod oscillator, enabled only while METAL is ON.
- `wcRing2Osc`: optional independent RING2 oscillator (deferred from V1).

OSC 2 is not the Complex FM modulator. Complex FM and METAL FM are separate paths.

## 2. Confirmed modulation controls and defaults

Complex Oscillator:
- MOD RATIO 0.25..16, default 3.07.
- Modulator waveform SINE / TRIANGLE / SAW / SQUARE, default SINE.
- Mode FM / AM, default FM.
- FM CHARACTER SMOOTH / CLASSIC / METALLIC, default METALLIC.
- FM INDEX 0..1, default 0.
- FM FEEDBACK 0..0.8, default 0; bounded feedback delay reference is 3 ms.
- FM ENV AMT 0..1, default 0.50; ENV ATTACK 0.001..1 s, default 0.005; ENV DECAY 0.01..3 s, default 0.25.
- AM DEPTH 0..1, default 0.
- Complex module volume 0..1, default 1 (listed in FM design 02).

METAL:
- METAL RATIO 0.5..12, default 4.20.
- METAL DECAY 0.05..3 s, default 0.40.
- Independent METAL ON/OFF.
- WEST COAST DRIVE scales the METAL FM depth; it is not WAVEFOLDER DRIVE or TIMBRE.
- METAL ON also selects the short 2 ms trigger attack and METAL DECAY; METAL OFF returns to normal ATTACK/DECAY.
- METAL Ring Mod is distinct from optional RING2.

Never alias MOD RATIO (3.07 default) and METAL RATIO (4.20 default).

## 3. Confirmed wavefolder and LPG

WAVEFOLDER:
- Stage 1: fixed reflection curve; input gain is TIMBRE × 6 + 1.
- Stage 2: FOLD 0..1 default 0.25, SYMMETRY -1..1 default 0, DRIVE 0..1 default 0.20, MODE SOFT / CLASSIC / HARD (CLASSIC default), MODULE VOLUME 0..1 default 1.
- Stage 2 uses 4× oversampling in the browser reference. The fixed-point port needs a deliberate alias-aware implementation; it must not claim equivalence merely by copying the floating-point Web Audio curve.
- TIMBRE, WEST COAST DRIVE, and WAVEFOLDER DRIVE are three different controls.

Main filter and LPG:
- Main low-pass CUTOFF / RESONANCE are separate from LPG.
- The v98 detailed path places the main filter after FOLD, then a separate LPG low-pass filter and LPG gain envelope.
- LPG RESPONSE 0..1 default 0.50; DECAY 0.02..2 s default 0.25; MODULE VOLUME 0..1 default 1.
- LPG filter starts at 1800 Hz, Q 0.7; trigger temporarily raises cutoff above main cutoff then returns toward base cutoff.
- LPG envelope must not own or extend Felucca voice lifetime.

## 4. FOLLOW and METAL

FOLLOW is sequencer pitch tracking, not a fold/envelope follower:
- ON updates OSC1, OSC2, Complex modulator and RING2 frequency from the sequencer; while METAL is ON it also updates METAL FM and METAL ring oscillator frequencies.
- OFF skips per-step pitch tracking and leaves base FREQ.
- FOLLOW and METAL are independent switches in OSCILLATORS / SYNTH VOICE.
- METAL OFF bypasses only the METAL-specific FM/ring paths and retains METAL RATIO / DECAY values.

Sequencer step FOLD modulation, motion FOLD modulation, and patch-bay FOLD CV are distinct from FOLLOW and METAL. The first V1 scope may defer the full patch bay, but must not conflate these functions.

## 5. Parameter inventory discrepancy — must resolve before allocation

The existing `ENERGUN_METAL_CORE_V1_PARAMETER_INVENTORY_01.md` reports 35 independent controls, but cross-checking the v98-faithful design documents reveals at least two explicit controls that are not counted there:
1. COMPLEX MODULE VOLUME (FM design 02).
2. WEST COAST VOLUME / final output gain (OSC design 02). The v98 implementation notes identify conflicting gain assignments (VOLUME × 12 in the direct handler versus VOLUME × 5 in the recurring volume loop), so its intended scale must be calibrated, not guessed.

The inventory also deliberately defers SPREAD and optional RING2; OSC2 direct level and voice VCA level are included. The exact parameter count must therefore be reconciled against the protected v98 HTML and the approved V1 scope before assigning IDs. The 35 count is not a frozen total.

Additional source nuance:
- LPG MODULE VOLUME and LPG envelope share the same AudioParam gain node in v98; a separate multiplier would be a behavior change requiring approval.
- SPREAD routing is not established as an independent stereo OSC2 path.
- The high-level project chain OSCILLATORS → FM → FOLD → LPG → OUTPUT is a simplified block diagram. The detailed v98 internal path includes main filter, LPG filter/gain, voice VCA, and METAL ring path. Preserve the detailed tap/order where confirmed.

## 6. V1 scope boundary

Current recommended V1 keeps:
- OSC1 and OSC2 direct-audio sources;
- independent Complex FM/AM;
- independent METAL FM and METAL ring;
- FOLLOW and METAL switches;
- two-stage wavefolder;
- main filter and LPG;
- normal voice ADSR and internal level controls.

Current proposal defers:
- RING2 (RING MIX / RING RATIO);
- full custom patch bay;
- unverified SPREAD routing;
- generator-only controls and other unrelated features.

No new engine parameters are assigned by this audit. The scope and exact count remain subject to the user's approval after the discrepancy list is resolved.

## 7. Next gate

Before CP1 is considered complete:
1. Reconcile every visible v98 control with an explicit V1 include/defer decision.
2. Correct the inventory count and classify each control as engine-local, common parameter, or host function.
3. Reconfirm exact signal tap points and gain staging, including the two VOLUME assignments.
4. Only then propose the stable E0..E7 and sidecar/common parameter allocation.
5. Keep sidecar flash-sector selection blocked until the user's full device dump has been audited.

No DSP, parameter IDs, engine registration, storage implementation, firmware package, or hardware write was performed.
