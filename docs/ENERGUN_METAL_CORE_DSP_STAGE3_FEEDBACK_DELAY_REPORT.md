# ENERGUN METAL CORE — DSP stage 3: bounded feedback-delay primitive

Date: 2026-10-31
Branch: `metal-core-dsp-stage1-osc`
Status: fixed-point delay storage and ring-buffer primitive implemented; tested at 44.1 and 96 kHz. Not yet connected to the Complex FM feedback route or engine memory allocator.

## 1. Reference requirement

The approved source audit records a 3 ms Complex Oscillator feedback delay, per voice:

`wcMod -> 3 ms delay -> feedback gain -> wcMod frequency`

The audit estimates:
- 44.1 kHz: 133 int32 samples per voice, 532 bytes per voice, 4,256 bytes for 8 voices.
- 96 kHz: 288 int32 samples per voice, 9,216 bytes for 8 voices.
- 192 kHz: eight 576-sample buffers alone exceed the current 12,888-byte per-part memory budget; this stage deliberately does not claim 192 kHz support.

## 2. Implemented primitive

File: `firmware/src/eng_metal_core.c`

Added:
- `metal_core_feedback_state_t`: fixed-size per-voice ring storage.
- `metal_core_feedback_delay_reset()`: clears one voice's history.
- `metal_core_feedback_delay_tick()`: returns the oldest sample, writes the new sample, and advances a bounded ring index.

Delay length is computed at build time as the ceiling of 3 ms at the selected sample rate. The memory-size compile-time assertion rejects a layout larger than 12,888 bytes.

The primitive deliberately does not implement feedback gain or route delayed audio into wcMod. That routing, gain smoothing, and note/legato reset policy need to be resolved against the reference before connection.

## 3. Tests

File: `tests/metal_feedback_delay_test.c`

Checks:
- build-rate delay length;
- impulse return after exactly the configured sample count;
- independent state for each voice;
- ring wrap;
- per-voice reset;
- invalid voice bounds.

GitHub Actions run:
- Run ID: `41156380647`
- Result: **success**
- OSC, wcMod, FM curves, audio-rate FM, and feedback delay tests passed.
- Feedback delay was compiled and tested at both 44.1 kHz and 96 kHz.
- Run: https://github.com/energundenis/Felucca/actions/runs/41156380647

## 4. Current implementation boundary

The delay helper is not yet placed into `engine_t.mem`, and there is no engine registration. It is standalone DSP support code. The current source file is still excluded from `engines.c`.

No parameter IDs, UI, persistence, or sidecar storage were changed. No firmware image was built or flashed; no physical FM-1 was queried or written. Factory firmware V1.5 remains untouched.

## 5. Next implementation gate

Before wiring feedback, confirm the exact v98 feedback signal and scaling (whether the delayed wcMod waveform is added to phase increment or another frequency-control term), the feedback parameter curve and smoothing, and the reference note-retrigger/legato behavior. Then connect the ring state through the existing `engine_t.mem` contract and measure CPU and memory in the actual engine build.
