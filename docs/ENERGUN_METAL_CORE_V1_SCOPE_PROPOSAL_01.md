# ENERGUN METAL CORE — FM-1 V1 scope proposal 01

Date: 2026-10-31
Branch: energun-metal-core
Status: proposal for user decision; no DSP or parameter IDs implemented.

## 1. Preserve the v98 core sound path

Recommended V1 includes:
- OSC 1 carrier and OSC 2 direct-audio source;
- separate Complex Oscillator modulator (FM / AM, MOD RATIO, modulator waveform, FM CHARACTER, FM INDEX, AM DEPTH, feedback and FM envelope);
- separate METAL FM and METAL Ring Mod paths, controlled by METAL;
- FOLLOW pitch tracking, independent from METAL;
- two-stage WAVEFOLDER with FOLD, SYMMETRY, DRIVE, MODULE VOLUME and SOFT / CLASSIC / HARD;
- main low-pass filter CUTOFF / RESONANCE;
- LPG RESPONSE / DECAY / MODULE VOLUME;
- normal voice ADSR and output level, mapped to Felucca only if the host semantics are equivalent.

The two ratios must remain independent: Complex MOD RATIO defaults to 3.07; METAL RATIO defaults to 4.20.

## 2. Reuse Felucca where semantics match

Potential reuse candidates:
- existing sequencer note, gate, velocity, accent and step timing;
- existing track ADSR / level only if its behavior can reproduce the v98 normal voice envelope and output gain;
- existing LFO and modulation matrix for compatible patch-bay routes.

A shared Felucca parameter must not be reused merely because its label is similar. Verify its exact DSP effect and save/restore behavior first.

## 3. Optional v98 features to decide separately

These are present in the browser reference but are not required to reproduce the main METAL CORE voice path:
- separate RING2 path (RING MIX / RING RATIO), distinct from METAL's own Ring Mod;
- the full custom PATCH BAY with LFO / RANDOM / SLEW / ENV / OSC / MOD sources and all listed destinations;
- independent SPREAD behavior, whose actual stereo routing in v98 is not fully established;
- additional module-volume controls if their behavior can be represented without changing the reference envelope/gain behavior.

The existing Felucca modulation matrix may cover some PATCH BAY needs, but its source/destination semantics must be checked before claiming equivalence.

## 4. Parameter and storage constraints

- Felucca has eight engine EDIT values E0–E7.
- Up to 16 new common parameter IDs can be added before E0 while keeping P_COUNT <= 127.
- The existing FUN10 project object cannot hold additional complete common per-track parameters; use the proposed sidecar only if required and only after verifying candidate flash sectors against the user's own V1.5 dump.
- New parameter IDs must remain stable for projects, user presets, motion locks, editor protocol and page navigation.

The exact number of new IDs cannot be frozen until the optional features above are selected and the existing Felucca parameters have been checked for true semantic matches.

## 5. Recommended decision

Recommended first target: implement the complete core voice path in section 1, use Felucca's sequencer and compatible modulation infrastructure, and defer the separate RING2 path / full custom patch-bay until the core sound is validated.

This is a scope recommendation only. It does not authorize implementation or hardware flashing.
