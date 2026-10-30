# ENERGUN METAL CORE — OSC design 02 (v98-faithful)

Date: 2026-10-31
Status: reference-corrected design draft. This supersedes the two-oscillator/FM-source assumption in OSC design 01. It does not implement DSP.

## 1. Source of truth

The original v98 HTML has been inspected. Its oscillator graph is not a two-oscillator FM pair. It contains two main audible oscillators plus separate modulation oscillators:

- wcOsc: OSC 1, the main carrier.
- wcOsc2: OSC 2, a second direct-audio source mixed into the shared pre-fold input.
- wcMod: the independent complex FM / AM modulator.
- wcMMod: the additional METAL FM modulator, active when METAL is ON.
- wcRingOsc: the METAL ring-mod oscillator, active when METAL is ON.
- wcRing2Osc: a separate optional ring-mod oscillator controlled by RING MIX / RING RATIO.

Do not replace these roles with a single OSC2-as-FM-modulator architecture.

## 2. Main audible oscillators

### OSC 1
- Base frequency is FREQ (v98 default 160).
- Waveform selector: SINE / TRI / SAW; default SINE.
- Feeds the Complex Oscillator amplitude path and then the shared TIMBRE / FOLD input path.

### OSC 2
- Uses the same base FREQ and main WAVE selection as OSC 1.
- Has independent DETUNE (v98 range 0..30 cents, default 7).
- Has independent direct-audio level OSC2 LVL (0..1, default 0.50).
- Its direct audio is mixed into the shared pre-fold input through wc2Gain and wcFoldIn.
- OSC 2 is not the FM/AM modulator in v98.

### Shared voice controls
- FREQ: 30..500, default 160.
- WAVE: SINE / TRI / SAW, default SINE.
- DETUNE: 0..30 cents, default 7.
- OSC2 LVL: 0..1, default 0.50.
- VOLUME: final output gain, default 1.
- LEVEL: triggered VCA level, default 0.50.
- SPREAD exists in the v98 UI (0..1, default 0.50), but its actual stereo routing must be re-audited before porting: OSC2 is summed into the common fold input, and the inspected audio graph does not establish an independent stereo OSC2 path.

## 3. Pre-fold input and TIMBRE

Both main audible oscillator sources feed the common fold input:
- OSC 1 -> Complex Oscillator amplitude path -> wcFoldIn.
- OSC 2 -> wc2Gain -> wcFoldIn.

TIMBRE controls wcFoldIn gain as 1 + TIMBRE × 6 (default TIMBRE 0). It is therefore a pre-fold input-gain / first-stage drive control, not an oscillator waveform or the second-stage WAVEFOLDER DRIVE control.

## 4. FOLLOW and METAL destinations

FOLLOW ON tracks sequencer note/octave and updates:
- OSC 1 frequency;
- OSC 2 frequency;
- the independent Complex FM/AM modulator frequency;
- the separate RING2 oscillator frequency;
- and, while METAL is ON, the METAL FM and METAL ring oscillator frequencies.

FOLLOW OFF skips the per-step pitch updates; base FREQ remains.

METAL is independent from FOLLOW. It enables the METAL-specific oscillator paths described in FM design 02 and also selects the METAL trigger envelope behavior.

## 5. Explicit distinctions

- OSC 2 direct audio is separate from the Complex FM/AM modulator.
- Complex FM/AM uses wcMod.
- METAL FM uses wcMMod.
- METAL Ring Mod uses wcRingOsc.
- The separate RING MIX / RING RATIO path uses wcRing2Osc and is not the METAL ring path.
- TIMBRE drives the shared first-stage wavefolder input; WAVEFOLDER DRIVE controls the second fold stage.

## 6. Implementation gate

Before DSP coding:
1. Preserve the six oscillator/modulator roles listed above.
2. Confirm fixed-point phase increment and waveform-table conventions against Felucca.
3. Define how OSC 1 and OSC 2 are mixed before the fixed first-stage wavefolder.
4. Resolve SPREAD routing only after an exact source-path audit; do not invent a stereo route.
5. Verify FOLLOW pitch tracking for every oscillator destination listed above.
6. Test low, middle, and high registers for clicks, aliasing, unwanted whistles, crackle, and level jumps.
7. Measure target CPU cost.

No DSP, engine registration, UI changes, or firmware flashing is included in this design draft.
