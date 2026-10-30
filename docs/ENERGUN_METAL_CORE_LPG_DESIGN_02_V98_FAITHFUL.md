# ENERGUN METAL CORE — LPG design 02 (v98-faithful)

Date: 2026-10-31
Status: reference-corrected design draft. This supersedes the proposed four-control LPG page in LPG design 01. It does not implement DSP.

## 1. Exact v98 LPG controls

The original v98 LOW-PASS GATE module contains exactly three controls:

| Control | Range | v98 default | Role |
|---|---:|---:|---|
| RESPONSE | 0..1 | 0.50 | Transient brightness and opening response |
| DECAY | 0.02..2 s | 0.25 | Per-trigger LPG tail |
| MODULE VOLUME | 0..1 | 1 | LPG output level |

CUTOFF and RESONANCE are not LPG controls in v98. They belong to the separate main low-pass filter before the LPG.

## 2. Signal position

The relevant v98 audio path is:

OSCILLATORS / COMPLEX FM / FOLD → MAIN FILTER → LPG FILTER → LPG GAIN → VOICE VCA → METAL RING PATH → OUTPUT

The main filter has its own CUTOFF and RESONANCE controls. The LPG has a separate low-pass filter and a separate per-trigger gain envelope.

## 3. v98 LPG behavior

- The LPG filter is a low-pass biquad with an initial frequency of 1800 Hz and Q of 0.7.
- RESPONSE sets the transient gain peak to 0.25 + RESPONSE × 0.75.
- RESPONSE also sets a short LPG opening attack: 0.0015 + RESPONSE × 0.006 seconds.
- During each sequencer trigger, the LPG filter cutoff is briefly raised above the main-filter cutoff, then returns toward the base CUTOFF value.
- DECAY sets the LPG tail, limited by the current step gate.
- MODULE VOLUME controls the LPG gain.
- The LPG envelope is separate from the normal voice ADSR and separate from the Complex FM-index envelope.

The LPG must not extend the host voice lifetime. Felucca's voice release and voice lifecycle remain authoritative.

## 4. Relationship to other envelopes and filters

Keep these functions distinct:

- Main CUTOFF / RESONANCE: the primary low-pass filter after FOLD.
- LPG RESPONSE / DECAY: transient brightness and gate-tail behavior in the LPG stage.
- LPG MODULE VOLUME: LPG output gain.
- ATTACK / DECAY / SUSTAIN / RELEASE: normal voice amplitude envelope.
- FM ENV ATTACK / DECAY / AMT: envelope that moves Complex FM INDEX only.
- METAL DECAY: alternate trigger decay selected while METAL is ON.

Do not move CUTOFF or RESONANCE into the LPG module and do not collapse LPG DECAY into the normal ADSR DECAY.

## 5. Felucca mapping and implementation gates

- Reuse the existing normal ADSR parameters only for the normal voice ADSR.
- Keep the three LPG controls as independent Metal Core parameters.
- Use the project-sidecar proposal if the final count of independent Metal Core parameters exceeds the existing project persistence capacity.
- Fixed-point filter and envelope state must be bounded and updated at the engine's control/block rate where practical.
- Validate short and long gates, retriggers, low/high registers, and the METAL ON/OFF envelope selection.
- Verify no clicks, whistles, crackle, resonant spikes, or unexpected gain changes.
- Measure target CPU and per-voice state memory before hardware validation.

No DSP, parameter IDs, UI changes, or firmware flashing is included in this design draft.
