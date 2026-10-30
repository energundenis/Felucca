# ENERGUN METAL CORE — Felucca parameter mapping audit 01

Date: 2026-10-31
Branch: energun-metal-core
Status: preliminary source audit. No parameter IDs assigned and no code changed.

## 1. Confirmed Felucca common controls

The current params.c defines the following common controls:
- P_LEVEL: track level, F_DB, 0..127, default 104.
- P_ATK / P_DEC / P_SUS / P_REL: common track ADSR.
- P_LRATE / P_LWAVE / P_LPHASE / P_LFADE: common LFO controls.
- P_LD_PIT / P_LD_FLT / P_LD_SHP / P_LD_AMP: common LFO destination depths.
- Four modulation slots, each with source, destination, and signed amount.
- P_GLIDE / P_GLMODE: glide controls.
- P_DETUNE: common detune, integer 0..127, default 40.
- P_PAN and P_SPRD: pan and mix spread.
- P_FTYPE: filter type selector for the ANALOG engine; it is not a generic filter cutoff or resonance parameter.
- P_ESYNC: envelope time synchronization mode.

## 2. Preliminary mapping decisions

| v98 control/function | Existing Felucca candidate | Decision | Reason |
|---|---|---|---|
| Normal voice ATTACK | P_ATK | Conditional reuse | Only if trigger/retrigger and time-curve behavior match v98 |
| Normal voice DECAY | P_DEC | Conditional reuse | Must not be confused with METAL DECAY or LPG DECAY |
| Normal voice SUSTAIN | P_SUS | Conditional reuse | Verify voice-level semantics |
| Normal voice RELEASE | P_REL | Conditional reuse | Verify release and voice-lifecycle interaction |
| Final track output level | P_LEVEL | Conditional reuse | Must distinguish v98 VOLUME, LEVEL/VCA and LPG MODULE VOLUME |
| Main FREQ | none confirmed | New parameter likely | Existing glide or sequencer pitch is not a base-frequency control |
| OSC2 DETUNE | P_DETUNE | Do not reuse yet | Existing detune range/semantics may be global voice detune, not oscillator-2 cents |
| FOLLOW | none confirmed | New parameter | Independent pitch-tracking enable |
| METAL | none confirmed | New parameter | Independent METAL FM/Ring path enable |
| Complex MOD RATIO | none confirmed | New parameter | Dedicated wcMod ratio |
| METAL RATIO | none confirmed | New parameter | Dedicated wcMMod/wcRingOsc ratio |
| Complex FM INDEX | none confirmed | New parameter | Engine-specific FM depth |
| FM FEEDBACK | none confirmed | New parameter | Dedicated delayed feedback amount |
| AM DEPTH / FM-AM mode | none confirmed | New parameter(s) | Separate mode and AM depth |
| FM CHARACTER | none confirmed | New parameter | Three distinct scaling curves |
| TIMBRE | none confirmed | New parameter | First-stage wavefolder input gain |
| WAVEFOLDER FOLD / SYMMETRY / DRIVE | none confirmed | New parameters | Dedicated second-stage curve controls |
| Main CUTOFF / RESO | none confirmed | New parameters | P_FTYPE is only a filter-type selector |
| LPG RESPONSE / DECAY / VOLUME | none confirmed | New parameters | Separate transient filter/gain stage |
| METAL DECAY | none confirmed | New parameter | Alternate trigger decay while METAL is on |
| LFO controls / patch-bay | existing LFO/mod matrix | Defer mapping | V1 defers full custom patch bay; equivalence must be tested |
| RING MIX / RING RATIO | none confirmed | Deferred | Separate RING2 path is outside V1 |
| SPREAD | P_SPRD | Deferred / conditional | v98 stereo routing is not fully established |

## 3. Important non-equivalences

- P_FTYPE is not a generic cutoff/resonance pair.
- P_DETUNE must not be assumed equivalent to v98 OSC2 cents detune without checking the engine implementation and conversion.
- P_LEVEL must not automatically replace v98's several gain controls.
- P_ATK / P_DEC / P_SUS / P_REL may be reused only for the normal voice amplitude envelope; they do not replace FM ENV, METAL DECAY, or LPG DECAY.
- Existing modulation slots are not automatically equivalent to the browser patch bay.

## 4. Next source checks

1. Inspect the current Felucca engine parameter read path and the precise semantics of P_LEVEL and ADSR.
2. Inspect P_DETUNE application in the current engines and its units/curve.
3. Inspect engine EDIT E0–E7 access and page/editor behavior before assigning any new common IDs.
4. Count the minimum new independent IDs required by the approved V1 scope.
5. Reconcile the count with project serialization, presets, motion locks, editor protocol, and sidecar rules before implementation.

No parameter IDs, DSP code, UI pages, firmware image, or hardware writes were changed by this audit.
