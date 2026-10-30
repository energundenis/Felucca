# ENERGUN METAL CORE — OSC design proposal 01

Status: **design proposal, not yet frozen**. This document proposes the first oscillator architecture for review. It does not implement DSP or register an engine.

## 1. Design goal

Build a compact two-oscillator source that can cover:
- stable, solid low-frequency bass;
- harmonically rich industrial leads;
- metallic and inharmonic FM textures;
- restrained detuning and beating without forcing an acid/303-style character.

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

Waveforms should be generated or selected using the project's existing table/generator facilities where practical. Do not introduce expensive per-sample waveform construction in the audio interrupt.

The waveform set is a proposal. Final table formats, interpolation, phase behavior, and anti-aliasing must follow the audited Felucca engine conventions.

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
- The mixed oscillator source then feeds FOLD after the FM operation.
- Normalize / headroom behavior must be deterministic: adding OSC 2 must not unexpectedly double the output level or cause clipping. Use explicit gain staging rather than relying on the final limiter.

Conceptual routing:

```text
OSC 1 ---------------------> (+) -----> FM result -----> FOLD
                              ^
OSC 2 direct audio ----------|
                              |
OSC 2 FM source ---> FM ------+
```

The diagram shows functional roles, not a final implementation graph. The exact modulation operation and placement are specified in the FM design.

## 6. Proposed OSC page controls

Four primary controls, consistent with the FMini-style four-knob page model:

| Control | Function | Initial default |
|---|---|---:|
| OSC 1 WAVE | Select OSC 1 waveform | SINE |
| OSC 2 WAVE | Select OSC 2 waveform | SINE |
| OSC 2 TUNE | Coarse semitone offset | 0 st |
| OSC 2 LEVEL | Direct audible OSC 2 mix | 0% |

OSC 1 level should be a stable engine parameter with a conservative default and can be exposed in an edit/detail control if the page layout requires it. OSC 2 fine tune and OSC 1 level should not be omitted from the parameter model merely because the four primary knobs are limited.

Possible detail controls:
- OSC 1 LEVEL: 0–100%, default 100%.
- OSC 2 FINE: -50…+50 cents, default 0.
- OSC 2 TUNE: -24…+24 semitones, default 0.

The UI mapping is provisional pending the full parameter/page design. Do not assume the four-knob page means the engine can store only four parameters.

## 7. Initial sound-design defaults

Recommended clean starting patch:
- OSC 1: SINE, level 100%.
- OSC 2: SINE, coarse tune 0, fine tune 0.
- OSC 2 direct level: 0%.
- FM amount: 0 until the FM stage is designed and enabled.
- FOLD: bypassed / neutral during oscillator validation.
- LPG: neutral / open during oscillator validation.

This makes the first oscillator test easy to diagnose: OSC 1 alone should produce a stable, clean note; OSC 2 direct mix should add a controllable second tone; OSC 2 FM should be tested separately later.

## 8. Validation gates

Before proceeding to FM:
1. OSC 1 produces clean notes across the intended register.
2. OSC 1 waveform changes are audible and stable.
3. OSC 2 direct audio can be blended independently without changing FM depth.
4. OSC 2 pitch changes affect its direct tone and FM source consistently, while FM ratio remains an independent parameter.
5. No clicks at note-on/note-off, waveform changes, or tuning changes.
6. No unexpected clipping or level jump when OSC 2 is blended.
7. Measure CPU cost on the target hardware; host rendering tests alone do not establish target timing.

## 9. Explicitly out of scope for proposal 01

- Third oscillator or dedicated sub-oscillator.
- Noise source.
- Hard sync.
- Ring modulation.
- Unison / voice stacking.
- Per-oscillator envelopes.
- Automatic randomization.
- Any change to Felucca's existing engines or audio path.

These can be considered later only if listening tests show a concrete need.

## 10. Decision required before implementation

Confirm or revise:
- two oscillators with OSC 2 serving as both FM source and optional direct-audio source;
- SINE / TRIANGLE / SAW / PULSE waveform set;
- OSC 2 coarse tune ±24 semitones and fine tune ±50 cents;
- independent OSC 2 direct level and FM amount;
- no third oscillator, noise, hard sync, ring modulation, or unison in the first version.
