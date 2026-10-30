# ENERGUN METAL CORE — OSC design 01

Status: **APPROVED BASELINE — preserve the METAL CORE sonic identity**. This document records the accepted oscillator architecture. It does not implement DSP or register an engine.

## 1. Design goal

Build a compact two-oscillator source that can cover:
- stable, solid low-frequency bass;
- harmonically rich industrial leads;
- metallic and inharmonic FM textures;
- restrained detuning and beating without forcing an acid/303-style character.

The oscillator architecture must support the established ENERGUN METAL CORE character. It is not intended to turn the engine into a generic subtractive two-oscillator synth. The defining sound remains the interaction of the oscillator source, FM, FOLD, LPG, and the independent FOLLOW and METAL functions.

Avoid adding a third oscillator, noise generator, unison stack, or sub-oscillator in this first implementation. Keep CPU use and parameter interactions predictable.

## 2. Oscillator roles

### OSC 1 — Carrier / primary audible source
- Tracks the part's played note and octave.
- Supplies the main audible tone.
- Receives frequency modulation from OSC 2.
- Has its own waveform selection and level.

### OSC 2 — Modulator / secondary audible source
- Tracks the played note with independently adjustable coarse and fine tuning.
- Supplies the FM signal to OSC 1.
- Can also be mixed directly into the audible signal.
- Has its own waveform selection and direct-audio level.
- Its FM contribution and direct-audio contribution are independently controlled.

OSC 2 is therefore not merely a second audible oscillator: it is the dedicated FM source. This avoids adding a separate hidden modulator oscillator.

## 3. Initial waveform set

Use a deliberately small, useful waveform set:
- SINE — clean fundamental; preferred starting point for controlled FM.
- TRIANGLE — restrained upper harmonics.
- SAW — dense harmonic content.
- PULSE — hollow / nasal harmonic spectrum.

This set is an initial toolkit, not a replacement for the characteristic METAL CORE sound. The final choice of defaults and any waveform-shaping behavior must be judged against the established metallic bass/lead character. Do not force an acid/303-like nasal slide or excessive high-frequency harshness.

Waveforms should be generated or selected using the project's existing table/generator facilities where practical. Do not introduce expensive per-sample waveform construction in the audio interrupt.

The final table formats, interpolation, phase behavior, and anti-aliasing must follow the audited Felucca engine conventions.

## 4. Pitch and tuning

- OSC 1 follows the part note and octave setting.
- OSC 2 coarse tune: -24 to +24 semitones, default 0.
- OSC 2 fine tune: -50 to +50 cents, default 0.
- Do not add independent OSC 1 detune in the first pass.
- FM ratio remains a separate FM-page control; it must not be silently coupled to OSC 2's audible pitch offset.
- Define whether FM ratio is frequency-ratio based or note-interval based during the FM design step. Do not assume they are interchangeable.

## 5. Level and routing

- OSC 1 has a primary level control.
- OSC 2 has a direct-audio level control.
- OSC 2's FM depth is controlled only on the FM page.
- OSC 2 direct-audio level at zero must not disable its FM role.
- FM amount at zero must not mute OSC 2's direct-audio contribution.
- The oscillator mix feeds the FM/FOLD processing path according to the final DSP graph; the exact operation order must be settled in the FM-stage design before implementation.
- Normalize / headroom behavior must be deterministic: adding OSC 2 must not unexpectedly double the output level or cause clipping. Use explicit gain staging rather than relying on the final limiter.

Conceptual roles:

```text
OSC 1 -----------------------> carrier path ----+
                                                 +--> FM / tone shaping --> FOLD
OSC 2 --> FM source -----------------------------+
   |
   +--> optional direct audio mix -------------->+
```

This diagram shows functional roles, not a final implementation graph. The exact modulation operation and placement are specified in the FM design.

## 6. Proposed OSC page controls

Four primary controls, consistent with the FMini-style four-knob page model:

| Control | Function | Initial default |
|---|---|---:|
| OSC 1 WAVE | Select OSC 1 waveform | SINE |
| OSC 2 WAVE | Select OSC 2 waveform | SINE |
| OSC 2 TUNE | Coarse semitone offset | 0 st |
| OSC 2 LEVEL | Direct audible OSC 2 mix | 0% |

OSC 1 level should be a stable engine parameter with a conservative default and can be exposed in an edit/detail control if the page layout requires it. OSC 2 fine tune and OSC 1 level must not be omitted from the parameter model merely because the four primary knobs are limited.

Detail controls:
- OSC 1 LEVEL: 0–100%, default 100%.
- OSC 2 FINE: -50…+50 cents, default 0.
- OSC 2 TUNE: -24…+24 semitones, default 0.

The UI mapping is provisional pending the full parameter/page design. Four primary knobs do not limit the number of stored engine parameters.

## 7. Initial sound-design defaults

Recommended clean starting patch for isolated oscillator validation:
- OSC 1: SINE, level 100%.
- OSC 2: SINE, coarse tune 0, fine tune 0.
- OSC 2 direct level: 0%.
- FM amount: 0 until the FM stage is designed and enabled.
- FOLD: bypassed / neutral during oscillator validation.
- LPG: neutral / open during oscillator validation.

These are diagnostic defaults only, not the required factory sound or the final METAL CORE preset. Once the oscillator is validated, the characteristic FM/FOLD/FOLLOW/METAL interaction must be restored and tuned as the sound-design target.

## 8. Validation gates

Before proceeding to FM:
1. OSC 1 produces clean notes across the intended register.
2. OSC 1 waveform changes are audible and stable.
3. OSC 2 direct audio can be blended independently without changing FM depth.
4. OSC 2 pitch changes affect its direct tone and FM source consistently, while FM ratio remains an independent parameter.
5. No clicks at note-on/note-off, waveform changes, or tuning changes.
6. No unexpected clipping or level jump when OSC 2 is blended.
7. The result can reproduce the established METAL CORE bass/lead character after the later FM/FOLD stages are enabled; it must not be judged only as a clean isolated oscillator.
8. Measure CPU cost on the target hardware; host rendering tests alone do not establish target timing.

## 9. Explicitly out of scope for design 01

- Third oscillator or dedicated sub-oscillator.
- Noise source.
- Hard sync.
- Ring modulation.
- Unison / voice stacking.
- Per-oscillator envelopes.
- Automatic randomization.
- Any change to Felucca's existing engines or audio path.

These can be considered later only if listening tests show a concrete need.

## 10. Approved decision

The following is approved as the OSC architecture baseline:
- two oscillators, with OSC 2 serving as both the FM source and optional direct-audio source;
- SINE / TRIANGLE / SAW / PULSE as the initial waveform set;
- OSC 2 coarse tune ±24 semitones and fine tune ±50 cents;
- independent OSC 2 direct level and FM amount;
- no third oscillator, noise, hard sync, ring modulation, or unison in the first version;
- preserve the established ENERGUN METAL CORE sonic identity, especially the role of FM, FOLD, FOLLOW, and METAL.

Approval freezes this design direction, not unverified DSP details. No engine registration, UI integration, or firmware flashing is authorized by this document alone.
