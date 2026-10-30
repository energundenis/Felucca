# ENERGUN METAL CORE — signal-flow decision

Status: **accepted baseline architecture**. This document records the agreed signal-flow skeleton and the current UI placement decision; it does not claim that DSP has been implemented.

## Main audio path

```text
OSCILLATORS
    |
    v
FM
    |
    v
FOLD
    |
    v
LPG
    |
    v
OUTPUT
```

The order is fixed for the first implementation baseline:

1. **OSC** — generates the source waveform(s).
2. **FM** — applies frequency modulation to the oscillator source.
3. **FOLD** — applies wavefolding / metallic harmonic shaping.
4. **LPG** — applies the low-pass gate / amplitude response.
5. **OUTPUT** — sends the resulting mono voice signal into Felucca's existing part/mixer path; the host path remains responsible for stereo placement and global effects.

## FOLLOW and METAL controls — current UI decision

- Both controls are located in the **OSCILLATORS / SYNTH VOICE** block, adjacent to the oscillator controls.
- They are independent ON/OFF switches.
- The **FOLD** block remains a dedicated wave-shaping stage and does not host these global voice controls.
- This supersedes the earlier draft decision that placed both switches on the FOLD page.

### FOLLOW

- ON: sequencer note/octave tracking sets the pitch of the voice's oscillator-related sources.
- OFF: sequencer pitch tracking is bypassed; the base FREQ remains in effect.
- Turning FOLLOW OFF does not reset base FREQ or any other parameter.

### METAL

- ON: enables the additional METAL FM contribution and Ring Modulation contribution.
- OFF: bypasses those additional METAL contributions.
- METAL RATIO and METAL DECAY values are retained while OFF and take effect again when ON.
- The reference implementation also selects its METAL-specific trigger attack/decay behavior while METAL is ON; preserve this behavior rather than inventing a new envelope algorithm.

The saved v99 industrial-metal HTML is a working reference copy explicitly based on METAL CORE v98; it states that the original v98 remains unchanged. Its UI already places FOLLOW and METAL in the WEST COAST / SYNTH VOICE module header. Use it as behavioral evidence, but do not claim that v99 is an exact byte-for-byte copy of v98.

The switches do not create new synthesis stages and do not change the main signal-flow order. Their UI location is in the synth block even though their processing affects more than one internal stage.

## Scope and compatibility

- This is the first accepted METAL CORE architecture baseline, not a claim of a completed engine.
- Initial voice mode remains monophonic.
- Output remains connected to Felucca's existing audio / mixer path.
- Existing Felucca engines, engine IDs, allocator, MIDI, sync, effects, presets, and project behavior remain unchanged.
- The engine must use Felucca's existing voice allocator and bounded audio-block rendering.
- DSP remains fixed-point and must pass host regression and target CPU-budget checks before hardware validation.

## Next implementation gate

Before implementing DSP, define:
1. oscillator count, waveforms, tuning, and mix behavior;
2. FM source, ratio, amount, and feedback behavior;
3. FOLD transfer function and the exact interaction with METAL processing;
4. FOLLOW's pitch-tracking inputs and all oscillator-related destinations;
5. LPG cutoff, decay, response, and envelope interaction;
6. parameter ranges, defaults, scaling, persistent storage, and retained OFF-state values.

No engine registration, engine-ID allocation, firmware flashing, or claim of hardware validation is authorized by this architecture record alone.
