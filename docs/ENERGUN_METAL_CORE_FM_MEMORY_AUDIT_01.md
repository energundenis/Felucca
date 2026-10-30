# ENERGUN METAL CORE — FM feedback memory audit 01

Date: 2026-10-31
Branch: energun-metal-core
Status: source-based feasibility estimate; no DSP code or memory allocation implemented.

## 1. Reference requirement

The v98 Complex Oscillator feedback path uses a 3 ms delay:
wcMod -> delay (configured to 3 ms) -> feedback gain -> wcMod frequency.

This is per voice because each voice has its own oscillator/modulation state.

## 2. Felucca memory budget

The pinned Felucca build defines:
- CTL = 32 samples per render block.
- Default sample rate = 44.1 kHz.
- ENG_MEM_PART = 12,888 bytes per part when ADPCM is disabled; 13,344 bytes when ADPCM is enabled.
- engine_t.mem is the per-part state region; a custom engine can use it for its per-voice delay buffers without changing voice_t or allocating memory in the audio render path.

At 44.1 kHz, 3 ms is 132.3 samples. A 133-sample Q31 / int32 ring buffer per voice requires:

- 133 × 4 bytes = 532 bytes per voice.
- 8 voices × 532 bytes = 4,256 bytes per part.

This fits inside the current 12,888-byte per-part engine-memory budget, leaving 8,632 bytes for oscillator/filter state and alignment. The estimate should be confirmed with a compile-time size assertion when the engine state structure is designed.

At 96 kHz, 3 ms is 288 samples and eight int32 buffers require 9,216 bytes per part, still below 12,888 bytes before other state. Do not claim 192 kHz support for this design without recalculating the complete state size; eight 576-sample int32 buffers alone would require 18,432 bytes per part.

## 3. Implementation constraints

- Allocate the delay buffers statically inside the engine's per-part memory block and index them by voice.
- No malloc/free, dynamic allocation, blocking, or unbounded loops in render.
- Keep the ring-buffer read/write cost bounded and constant per sample.
- Define behavior for note retrigger and voice reuse; clear or retain the delay state deliberately according to the reference behavior and Felucca's engine keep/retrigger contract.
- Keep feedback amount bounded and smoothed; test for runaway DC, clicks, high-frequency whistles, and clipping.
- Measure target CPU cost in addition to the memory size.

No source code, firmware build, or hardware write was performed by this audit.
