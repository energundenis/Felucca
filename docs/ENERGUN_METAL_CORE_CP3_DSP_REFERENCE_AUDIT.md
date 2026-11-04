# CP3 DSP reference audit — ENERGUN METAL CORE

Date: 2026-11-04

Reference reviewed: `energundenis/Felucca/energun_metal_core_v98_reference.html` on `energun-metal-core`, plus `projects/metal-core/presets/TECHNO_MAIN_SOUND_v01.md` in `energundenis/ENERGUN-SYNTH-LAB`.

## Confirmed reference structure

The reference has separate processing stages and must not be collapsed into one generic saturation block:

1. Complex Oscillator: OSC 2 modulates OSC 1; FM/AM selectable; modulator waveform and ratio; FM index, feedback and a per-note FM index envelope.
2. METAL: independent switch. It adds a second sine modulator at METAL RATIO to the carrier frequency and uses a same-frequency sine to ring-modulate the post-LPG/VCA output. The reference default METAL RATIO is 4.20. METAL OFF bypasses these two Metal paths and retains their values.
3. Wavefolder: a first fold stage follows the complex oscillator; a second folder has mode, amount, symmetry and drive controls. The second folder has soft/classic/hard transfer modes and 4x Web Audio waveshaper oversampling.
4. MAIN filter and LPG filter are serial and distinct.
5. LPG gain envelope and VCA are separate: the VCA is the final gate/level stage, while LPG includes a separate low-pass response path.

## Implications for CP3 implementation

- The newly added two-operator FM header is only an isolated phase-modulation candidate. It is not yet the complete Complex Oscillator or the METAL module.
- The current CP3 dispatcher harness proves OSC1/OSC2 + one FM modulator + VCA lifecycle. It does not yet prove the METAL ring-mod stage, FM feedback, FM/AM mode, FM-character curves, or the sequencer-driven FM envelope.
- Do not assume the current FM ratio/depth test constants are the final reference defaults.
- Keep separate state for the Complex Oscillator modulator and METAL modulator until phase-sharing is explicitly proven. Keep the Metal ring oscillator path distinct from the post-LPG/VCA gain envelope.
- Do not merge the two filter states or merge LPG gain envelope with VCA.

## Next safe sequence

1. Add and test a separate Metal ring-mod candidate with bounded Q15 multiplication and independent enable/bypass behavior.
2. Add a separate FM feedback candidate with explicit one-sample/short-delay state and numeric stability tests; do not insert a delay into the audio callback until cycle budget is measured.
3. Implement the reference wavefolder modes as a fixed-point candidate and compare transfer curves against the source reference.
4. Add separate MAIN and LPG filter candidate state and preserve both in the per-voice/part memory budget.
5. Only then build a combined real-dispatch harness for the complete chain and perform AC79 codegen/cycle analysis.

This is a design audit only. It does not change engine registration, parameter IDs, preset formats, Sidecar storage, or device firmware.
