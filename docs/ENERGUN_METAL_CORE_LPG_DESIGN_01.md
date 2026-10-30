# ENERGUN METAL CORE — LPG design 01

Status: implementation design draft. This document defines the first FM-1 LPG proposal from the browser prototype audit. It does not claim DSP implementation or hardware validation.

## 1. Source reference and compatibility

The browser prototype v104 contains a dedicated LPG stage after the existing filter/saturation chain and before the voice amplifier. Its dedicated controls are RESPONSE, DECAY and MODULE VOLUME. In that prototype:

- RESPONSE sets the LPG filter's transient brightness and opening behavior.
- DECAY controls the per-step LPG tail.
- MODULE VOLUME is an independent output level.
- The prototype's LPG filter is a low-pass stage; its resonance is not independently exposed there.
- The earlier main filter has its own CUTOFF and RESONANCE controls. Those are a separate filter stage, not LPG controls.

For FM-1, the proposed LPG page expands the dedicated LPG stage to four controls: CUTOFF, RESONANCE, DECAY and RESPONSE. This is an intentional extension, not a claim that all four controls existed in v104's LPG module.

## 2. Signal position

Approved overall voice chain remains:

OSCILLATORS → FM → FOLD → LPG → OUTPUT

The LPG is the final tone/dynamics stage inside the Metal Core voice, before the voice is added to Felucca's part/mixer path. It must not replace Felucca's global effects or output processing.

## 3. Controls and intended behavior

### CUTOFF
- Sets the LPG low-pass filter's steady-state/base cutoff.
- Higher values allow more upper harmonics; lower values darken the body and tail.
- At the neutral/open end, it must not impose an unintended dulling of the oscillator/FOLD tone.
- During a triggered note, RESPONSE may briefly raise the cutoff above this base value; the cutoff then returns smoothly to the user's setting.

### RESONANCE
- Controls the damping/Q of the LPG's low-pass filter.
- Keep the mapping bounded and musically controlled; no self-oscillation in the first implementation.
- Resonance must not create whistles, unstable peaks, crackle or abrupt gain jumps.
- Default should be restrained, not a high-Q filter setting.

### DECAY
- Sets the LPG's per-trigger tail time.
- It controls the decay of the LPG's dynamic opening/gain envelope, not the amplitude ADSR itself.
- A short value gives a tight, percussive metallic hit; a longer value allows a sustained/ringing tail.
- The gate/note-off and the existing Felucca voice lifecycle remain authoritative. LPG must not keep a released voice alive beyond the host's voice lifecycle.

### RESPONSE
- Controls the relationship between transient brightness and dynamic opening.
- At low values, opening is restrained and the transient is darker/softer.
- At high values, the initial opening is brighter and more pronounced, then returns toward the CUTOFF base setting as the LPG envelope decays.
- RESPONSE must not simply be a second volume knob. Its primary role is to shape the coupled filter-envelope response.
- Smooth parameter changes and bounded curves are required to avoid clicks and excessive brightness.

## 4. Envelope interaction

The LPG has its own internal control envelope, separate from the Felucca ADSR and separate from any FM modulation envelope.

- The host ADSR continues to control the voice's normal amplitude lifecycle.
- LPG DECAY controls the LPG's own transient/tail behavior.
- RESPONSE shapes the initial LPG opening and the transient cutoff excursion.
- The LPG control envelope is triggered by the same note/step event as the voice.
- On retrigger, its state must be reset or transitioned according to the host's normal voice retrigger contract; no stale envelope state may leak between notes.
- If the voice is released before the LPG decay completes, the host's amplitude release/final voice shutdown wins.

Do not multiply the host ADSR by an unbounded second envelope. The combined gain must remain bounded and should be calibrated so that adding the LPG does not unexpectedly reduce or boost the established Metal Core reference level.

## 5. Parameter storage and Felucca integration

The four LPG controls are part of the Metal Core engine's own parameter set and must be saved by Felucca's existing PRESET / PROJECT system.

Felucca's engine interface currently exposes eight EDIT parameters (E0–E7). The four LPG controls therefore need a deliberate mapping into the engine parameter/UI design. Do not silently overwrite OSC/FM/FOLD parameters or assume the eight-parameter interface is a permanent limitation on all internal state. Before implementation, audit the exact parameter table, per-engine memory limits and preset serialization behavior in the pinned Felucca revision.

The initial LPG implementation does not add a separate preset manager or PATCH BAY.

## 6. DSP implementation constraints

- Fixed-point arithmetic only in the real-time audio path, consistent with Felucca-Engines.
- No heap allocation, blocking calls or unbounded loops in render.
- Filter coefficients updated at the engine's block/control rate, not recalculated with expensive math per sample.
- Smooth cutoff, resonance and gain changes across blocks.
- Use a stable low-pass topology already supported by the pinned DSP code where suitable; audit coefficient ranges and state bounds before choosing it.
- Prevent denormals/overflow and clamp internal state to documented safe ranges.
- Stereo output is preserved through the existing Felucca part/mixer path; LPG itself should not introduce unintended stereo widening.

## 7. Defaults and initial test plan

Provisional neutral starting point (to be calibrated, not yet a user-approved numeric preset):
- CUTOFF: open/neutral, high enough not to mask the reference tone.
- RESONANCE: low/ restrained.
- DECAY: medium-short.
- RESPONSE: moderate.

Validation sequence:
1. LPG neutral/bypassed comparison against the established Metal Core reference at FM amount 0.
2. Low, middle and high registers; short and long notes; fast sequencer patterns.
3. Sweep each control independently, then test paired interactions.
4. Verify no clicks on note-on, retrigger, note-off or parameter movement.
5. Verify no whistles, harsh resonant spikes, crackle or unexpected gain increase.
6. Re-enable FM, FOLD, FOLLOW and METAL and confirm their established behavior is not altered.
7. Measure target CPU/headroom on FM-1; host render tests alone do not establish target timing.

## 8. Explicit non-goals

- No envelope follower.
- No separate LPG modulation PATCH BAY.
- No replacement of the existing Felucca global filter/effects.
- No change to FOLLOW or METAL semantics.
- No modification of the protected browser Metal Core v105 baseline.
- No firmware flash until reproducible build and recovery path are verified.


## Parameter-storage correction (2026-10-31)

The earlier suggestion to squeeze the four LPG controls into the eight engine EDIT values is not the preferred route. The engine ABI's E0–E7 values are already persisted, but the dedicated OSC / FM / FOLD / LPG / VOICE pages may require additional common `P_MC_*` parameter IDs. The current FUN10 project object has no complete spare per-track parameter slot, so those IDs require the proposed project sidecar described in `docs/ENERGUN_METAL_CORE_PARAMETER_STORAGE_PLAN_01.md`. The four LPG controls remain independent; do not collapse them into a macro without explicit approval. This is a storage design proposal, not implementation approval.


## Superseded

Use LPG_DESIGN_02_V98_FAITHFUL.md. The v98 LPG has RESPONSE, DECAY and MODULE VOLUME; CUTOFF and RESONANCE belong to the separate main filter.
