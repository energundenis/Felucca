# ENERGUN METAL CORE — FM design 02 (v98-faithful)

Date: 2026-10-31
Status: reference-corrected design draft. This supersedes the OSC2-as-FM-source and 4.20-only FM-ratio assumptions in FM design 01. It does not implement DSP.

## 1. Two independent modulation systems

The v98 source contains two separate FM systems. Do not merge them.

### A. COMPLEX OSCILLATOR FM / AM

- Modulator: independent oscillator wcMod.
- Carrier: OSC 1 (wcOsc).
- OSC 2 (wcOsc2) is a separate direct-audio source; it is not wcMod.
- Modulator frequency is FREQ × MOD RATIO.
- MOD RATIO range 0.25..16, default 3.07.
- Modulator waveform: SINE / TRIANGLE / SAW / SQUARE, default SINE.
- Mode selector: FM / AM, default FM.
- FM CHARACTER: SMOOTH / CLASSIC / METALLIC, default METALLIC.
- FM INDEX range 0..1, default 0.
- FM FEEDBACK range 0..0.8, default 0.
- FM ENV AMT 0..1, default 0.50; ENV ATTACK 0.001..1 s, default 0.005; ENV DECAY 0.01..3 s, default 0.25.
- COMPLEX MODULE VOLUME 0..1, default 1.

### B. METAL FM

- Modulator: separate sine oscillator wcMMod.
- Carrier: OSC 1 (wcOsc).
- Frequency: FREQ × METAL RATIO.
- METAL RATIO range 0.5..12, default 4.20.
- METAL FM is enabled only while METAL is ON.
- Its modulation amount is scaled by WEST COAST DRIVE through wcDrv(); it is not the same control as COMPLEX FM INDEX.
- METAL DECAY range 0.05..3 s, default 0.40.
- METAL ON also selects the METAL trigger attack of 0.002 s and METAL DECAY. METAL OFF returns to the normal ATTACK / DECAY values.

The two ratios are distinct:
- MOD RATIO (3.07 default) controls wcMod, the Complex Oscillator.
- METAL RATIO (4.20 default) controls wcMMod and the METAL ring oscillator.

Never alias one ratio to the other.

## 2. COMPLEX OSCILLATOR FM behavior

The v98 Complex FM path is audio-rate frequency modulation:

wcMod waveform -> FM scaling -> OSC 1 frequency

The modulation amount is scaled as:

- SMOOTH: 0.72 × sqrt(index)
- CLASSIC: index
- METALLIC: 0.55 × index + 1.15 × index²

The selected character curve is multiplied by FREQ × 2 × wcDrv(), where wcDrv() = WEST COAST DRIVE ^ 1.5.

Therefore:
- FM INDEX and WEST COAST DRIVE are separate controls.
- At WEST COAST DRIVE = 0, the Complex FM amount is zero even if FM INDEX is raised.
- FM INDEX = 0 leaves OSC 1 unaffected by this Complex FM path.
- Do not merge WEST COAST DRIVE into WAVEFOLDER DRIVE or TIMBRE.

### AM mode

When MODULATION = AM:
- the Complex FM index gain is set to zero;
- wcMod is routed to the Complex amplitude gain;
- AM DEPTH controls the wet modulation amount;
- the direct carrier path is reduced correspondingly.

AM DEPTH range is 0..1, default 0. The FM and AM paths share wcMod but are selected as separate modes.

### Feedback

The v98 feedback path is:

wcMod -> fixed delay (20 ms maximum object, set to 3 ms) -> feedback gain -> wcMod frequency

- FM FEEDBACK range 0..0.8, default 0.
- Feedback gain scales with FREQ × 3.
- Feedback is part of the Complex Oscillator modulator path only; it is not METAL feedback and does not feed the FOLD stage.

For the fixed-point port, reproduce the bounded 3 ms feedback behavior with a fixed-size per-voice delay state or another demonstrably equivalent method. Audit memory use and CPU cost before implementation; do not allocate memory in the audio render path.

### FM modulation envelope

On each sequencer trigger, the Complex FM envelope raises the base modulation amount and returns it to the panel's base amount:
- base = current FM INDEX after character and DRIVE scaling;
- peak = base × (1 + FM ENV AMT × 4);
- ENV ATTACK and ENV DECAY set the transition times.

This envelope affects Complex FM amount only. It is not the voice amplitude ADSR and not the LPG envelope.

## 3. METAL FM and METAL Ring Modulation

METAL uses two separate oscillators:
- wcMMod: sine oscillator at FREQ × METAL RATIO, routed to OSC 1 frequency through wcMIdx.
- wcRingOsc: sine oscillator at FREQ × METAL RATIO, routed to the post-VCA ring-mod gain path through wcRingDepth.

When METAL is ON:
- wcMMod's frequency is synchronized to FREQ × METAL RATIO;
- wcMIdx is enabled with gain FREQ × 2 × wcDrv();
- wcRingDepth is connected to wcRingNode.gain, enabling the post-VCA ring-mod contribution;
- the trigger uses 0.002 s attack and METAL DECAY.

When METAL is OFF:
- wcMIdx gain is set to zero;
- wcRingDepth is disconnected;
- wcRingNode gain returns to unity;
- METAL RATIO and METAL DECAY values remain stored.

The METAL ring oscillator is not the same as the optional RING MIX / RING RATIO path below.

## 4. Separate RING2 path

v98 also contains a separate ring-mod path:
- oscillator wcRing2Osc: sine, frequency FREQ × RING RATIO;
- RING RATIO range 0.25..16, default 2.01;
- RING MIX range 0..1, default 0;
- it blends a ring-modulated tap from after the first fixed wavefolder stage with the main path after the second wavefolder stage.

This RING2 path is independent of METAL ON/OFF. Do not rename it METAL or use it as a substitute for the METAL ring path.

## 5. FOLLOW interaction

When FOLLOW is ON, sequencer note/octave updates:
- OSC 1 and OSC 2 frequencies;
- wcMod frequency;
- wcRing2Osc frequency;
- and, while METAL is ON, wcMMod and wcRingOsc frequencies.

When FOLLOW is OFF, the per-step pitch updates are skipped and base FREQ remains. FOLLOW is independent of METAL and is located in the OSCILLATORS / SYNTH VOICE block.

## 6. Fixed-point implementation requirements

- Reproduce audio-rate FM using the dedicated wcMod source; do not substitute OSC 2 as the modulator.
- Preserve separate MOD RATIO and METAL RATIO.
- Preserve separate COMPLEX FM INDEX, WEST COAST DRIVE, METAL FM depth, and FM FEEDBACK.
- Implement FM / AM mode switching without discontinuities.
- Bound the 3 ms feedback state and measure per-voice memory use.
- Keep the METAL-specific ring modulation separate from RING2.
- Smooth frequency, index, ratio, and feedback changes.
- Test low, middle, and high registers for aliasing, whistles, crackle, clicks, and gain jumps.
- Verify METAL OFF retains ratio/decay values and fully bypasses only the METAL-specific paths.
- Measure CPU and headroom on the target device; host tests alone do not establish target performance.

No DSP, engine registration, UI changes, or firmware flashing is included in this design draft.
