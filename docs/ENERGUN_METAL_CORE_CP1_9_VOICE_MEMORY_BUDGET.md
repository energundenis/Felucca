# ENERGUN METAL CORE — CP1.9 voice-state and per-part memory budget

Date: 2026-10-31
Branch: energun-metal-core
Pinned engine API: Felucca-Engines `d0260074ce100a47675814b7f38d61f11176c926`.
Status: source-backed capacity proposal; no state layout or allocation implemented.

## 1. Confirmed Felucca capacities

From the pinned `felucca_engine.h` and `firmware/src/core.h`:
- Each track has `voice_t v[NVOICE]`; `NVOICE=8` voices per part.
- The global sounding-voice budget is also eight across all parts, but each part still owns eight voice records.
- Each `voice_t` provides `ph[3]` (three 32-bit phase accumulators) and `s[8]` (eight 32-bit engine state words).
- The engine API exposes `engine_t.mem`, a zeroed per-part memory region accessed by the host. It is reset when another engine claims that part's region.
- `ENG_MEM_PART=12,888` bytes in the default build and 13,344 bytes with ADPCM enabled.
- `CTL=32`; sample rate defaults to 44.1 kHz. Engine sources support 32–96 kHz normally and can build up to 192 kHz subject to fixed-point limits.

The per-part memory region is not a shared global pool among tracks: `eng_mem[NPART]` allocates one region for each of the four parts. Each part's region is sized to the largest active engine memory request.

## 2. Oscillator phase requirement

The v98 reference contains five distinct oscillator phase sources in the Metal Core voice:
1. OSC1 carrier;
2. OSC2 direct-audio oscillator;
3. Complex FM/AM modulator;
4. METAL FM modulator;
5. METAL ring oscillator.

The three host `ph[]` words alone cannot hold all five phases. Two additional 32-bit phase accumulators are needed. They can use two `voice_t.s[]` words or a custom per-voice state record in `engine_t.mem`; do not merge oscillators or reuse the Complex modulator phase for either METAL oscillator.

## 3. Proposed 32-byte voice-state allocation

Candidate mapping if the implementation uses `voice_t.ph[3]` and `voice_t.s[8]` directly:

| Storage | Proposed role |
|---|---|
| `ph[0]` | OSC1 phase |
| `ph[1]` | OSC2 phase |
| `ph[2]` | Complex modulator phase |
| `s[0]` | METAL FM modulator phase |
| `s[1]` | METAL ring oscillator phase |
| `s[2]`, `s[3]` | Main/LPG filter state pair, subject to exact filter topology |
| `s[4]` | LPG envelope state |
| `s[5]` | Complex FM envelope state |
| `s[6]` | Feedback delay write index |
| `s[7]` | Reserved for one required bounded state; do not assign until the algorithm is frozen |

This allocation is tight. It does not include an independent internal VCA ADSR state. The host ADSR ownership decision must therefore be resolved before this map is frozen. If additional state is needed, move the complete Metal Core per-voice state into a typed record in `engine_t.mem` rather than changing the shared `voice_t` ABI.

The phase/state keep mask must be explicit. Fresh note and retrigger paths must initialize or retain each state deliberately; do not blindly preserve filter/envelope/feedback states.

## 4. Complex feedback delay memory

The v98 Complex Oscillator feedback path uses a 3 ms delay. At 44.1 kHz:
- 3 ms = 132.3 samples;
- use 133 samples for a complete ring;
- int32 Q-format sample storage = 133 × 4 = 532 bytes per voice;
- eight voice slots per part = 4,256 bytes per part.

At 48 kHz: 144 samples × 4 × 8 = 4,608 bytes.
At 96 kHz: 288 samples × 4 × 8 = 9,216 bytes.
At 192 kHz: 576 samples × 4 × 8 = 18,432 bytes, exceeding the current 12,888-byte default part pool before any other state. Therefore a 192 kHz Metal Core build is not supported by this straightforward per-voice 3 ms delay design.

At 44.1 kHz, 4,256 bytes leaves 8,632 bytes in the default part region for any custom state/tables. At 96 kHz, it leaves 3,672 bytes. Add a compile-time fit assertion for each supported build configuration; do not assume a memory size based on only the default build.

## 5. Part-memory estimate and placement

Candidate fixed per-part layout at 44.1 kHz:
- 8 × 133-sample int32 feedback delay rings: 4,256 bytes.
- 8 × 32-byte supplemental per-voice state records, if the direct `voice_t` slots prove insufficient: 256 bytes.
- Optional aligned lookup/control data: TBD only after algorithm selection.

A typed `metal_part_state_t` can be placed in `ENG_MEM(t - trk)`; the voice slot index can be derived from `v - t->v`. This gives stable per-part/per-voice addressing without heap allocation. Confirm pointer arithmetic and ownership against the final host source before implementation.

The current API's `engine_t.mem` is zeroed when a different engine claims the part's region. It is suitable for delay/state memory but not persistent patch values. Persistent controls still belong in project/preset storage or the approved sidecar.

## 6. Envelope ownership is a separate blocking decision

The reference v98 has a dedicated per-trigger VCA envelope with attack/decay/sustain/release behavior, velocity and accent scaling. Felucca host ADSR owns the voice lifetime and has different curves and retrigger/legato behavior.

Three distinct implementation models must not be conflated:
- host ADSR directly supplies VCA amplitude (`ownenv=0`): simplest lifecycle, but only sound-faithful after measured envelope calibration;
- host ADSR plus a second internal VCA envelope: multiplies two envelopes and is not a faithful replacement unless explicitly designed;
- engine-owned envelope (`ownenv=1` plus `done()`): enables full custom contour but moves voice-ending responsibility into the engine and requires additional lifecycle proof.

Current recommendation: do not implement a second unexamined amplitude envelope. First compare host ADSR against v98 trigger curves and decide whether its shape is acceptable; if not, design a deliberate custom envelope/lifecycle contract. Keep LPG and FM-index envelopes separate regardless.

## 7. CPU and sample-rate implications

- Feedback delay ring: one bounded read, one write and index update per sample per active voice; no division or dynamic allocation in render.
- Five oscillator phases: bounded phase increments; no expensive transcendental functions in render.
- Filter coefficients should be computed at block/control rate, not per sample.
- Wavefolder 4× oversampling is a separate potential CPU hotspot and must be measured after implementation.
- The baseline target-cost suite has no ENERGUN render symbol; it cannot establish the future engine's cost.
- Initial implementation should target 44.1 kHz and 32-sample CTL. Expand to 48/96 kHz only after memory, pitch, filter and target-cost checks pass. Do not claim 192 kHz support for V1 with the 3 ms per-voice feedback rings.

## 8. Gate and status

Before coding:
1. Freeze whether five oscillator phases live in `voice_t` or a supplemental per-voice record.
2. Freeze feedback delay format and supported sample rates.
3. Decide host versus engine VCA envelope ownership.
4. Finalize per-voice state size and compile-time fit assertions.
5. Measure target CPU after a real render function exists.

No shared ABI, voice structure, engine memory request, DSP source, parameter ID, storage format, firmware package, Flash address or device state changed.
