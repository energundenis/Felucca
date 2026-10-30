# ENERGUN METAL CORE — signal-flow decision

Status: **accepted baseline architecture**. This document records the agreed signal-flow skeleton only; it does not claim that DSP has been implemented.

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

## FOLLOW and METAL switches

- Both controls remain on the **FOLD** page.
- Each control has its own independent ON/OFF state.
- OFF bypasses only its corresponding processing contribution.
- OFF must not clear, overwrite, or reset the associated parameter value.
- ON restores that processing contribution using the retained value.
- The switches do not create new synthesis stages and do not change the main signal-flow order.

The exact internal tap points and implementation details for FOLLOW and METAL are intentionally left to the DSP design step. They must be specified before code is written; do not guess or bind them to unrelated Felucca features.

## Scope and compatibility

- This is the first accepted METAL CORE architecture baseline, not a claim of a completed engine.
- Initial voice mode remains monophonic.
- Output remains connected to Felucca's existing audio / mixer path.
- Existing Felucca engines, engine IDs, UI pages, allocator, MIDI, sync, effects, presets, and project behavior remain unchanged.
- The engine must use Felucca's existing voice allocator and bounded audio-block rendering.
- DSP remains fixed-point and must pass host regression and target CPU-budget checks before hardware validation.

## Next implementation gate

Before implementing DSP, define:
1. oscillator count, waveforms, tuning, and mix behavior;
2. FM source, ratio, amount, and feedback behavior;
3. FOLD transfer function and the precise role of METAL;
4. the precise signal/control role of FOLLOW and its bypass point;
5. LPG cutoff, decay, response, and envelope interaction;
6. parameter ranges, defaults, scaling, and persistent storage.

No engine registration, engine-ID allocation, UI integration, or firmware flashing is authorized by this architecture record alone.
