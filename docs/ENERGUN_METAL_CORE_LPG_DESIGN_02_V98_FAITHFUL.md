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
- MODULE VOLUME writes the shared `wcLpgGain.gain` AudioParam as a base value, but the next `wcTrig()` call holds and overwrites that same AudioParam with the RESPONSE-derived envelope peak and DECAY tail. It is not an independent per-step output multiplier in v98.
- The LPG envelope is separate from the normal voice ADSR and separate from the Complex FM-index envelope.

The LPG must not extend the host voice lifetime. Felucca's voice release and voice lifecycle remain authoritative.

## 4. Relationship to other envelopes and filters

Keep these functions distinct:

- Main CUTOFF / RESONANCE: the primary low-pass filter after FOLD.
- LPG RESPONSE / DECAY: transient brightness and gate-tail behavior in the LPG stage.
- LPG MODULE VOLUME: direct base write to the shared LPG gain node; the next sequencer trigger overrides it. See `docs/ENERGUN_METAL_CORE_CP1_8_LPG_VOLUME_SOURCE_TRACE.md`.
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


## 6. CP1.8 source-trace correction

The exact v98 source trace confirms that MODULE VOLUME is stored and its knob handler writes the LPG gain node, but each sequencer trigger subsequently overwrites the same AudioParam with `0.25 + RESPONSE × 0.75` and the LPG decay envelope. It therefore does not behave as an independent output gain during normal step playback. The V1 behavior is intentionally left undecided among exact bug compatibility, an independent post-envelope gain, or scaling the envelope peak. See `docs/ENERGUN_METAL_CORE_CP1_8_LPG_VOLUME_SOURCE_TRACE.md`; do not implement an interpretation without approval.


## 7. Approved V1 correction — LPG MODULE VOLUME Option C

The user approved Option C for V1. The peak at each trigger is:

`peakLpg = (0.25 + RESPONSE × 0.75) × MODULE VOLUME`

MODULE VOLUME scales the envelope peak; it does not add a separate post-envelope stage. RESPONSE attack and DECAY tail remain independent and unchanged. This intentionally differs from the v98 shared-gain overwrite defect. The isolated Q15 helper and host test are documented in CP1.8 and CP1.7; neither is yet connected to the engine render callback.
