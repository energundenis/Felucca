# ENERGUN METAL CORE — CP3.1 state and memory accounting

Date: 2026-11-01
Branch: `energun-metal-core`
Status: partial source-backed accounting; production state layout and engine registration remain blocked.

## 1. Scope and evidence

This increment accounts only state whose representation is already defined by the isolated primitives and the v98 source audits. It does not claim a complete engine memory total.

Source-backed host limits:
- Eight voice slots per part (`NVOICE=8`).
- Four parts (`NPART=4`), each with its own engine memory region.
- Felucca engine render quantum: `CTL=32`.
- Per-part engine memory: 12,888 bytes with ADPCM disabled; 13,344 bytes with ADPCM enabled.
- The host `voice_t` ABI remains unchanged: three phase words and eight signed state words.

## 2. Exact primitive sizes

The current C layouts are checked by `tests/metal_core_cp3_memory_accounting_test.c`:
- `eng_metal_vca_t`: 36 bytes per voice.
- `eng_metal_lpg_env_t`: 24 bytes per voice.
- Combined VCA + LPG envelope: 60 bytes per voice, 480 bytes for eight voices per part.

These are exact sizes for the current headers, not estimates. The VCA and LPG envelopes remain independent state machines.

## 3. Additional known per-voice state

The v98 oscillator audit identifies five distinct phase sources: OSC1, OSC2, Complex modulator, METAL FM modulator and METAL ring oscillator.

The host already supplies three phase words in `voice_t.ph[3]`. The provisional per-part engine record therefore reserves only the two additional METAL phase words, plus:
- one feedback ring write cursor (32-bit);
- one 36-byte VCA state;
- one 24-byte LPG envelope state.

The compile-time probe's provisional record is 72 bytes per voice:
- 8 bytes: two additional phase accumulators;
- 4 bytes: feedback write cursor;
- 36 bytes: VCA;
- 24 bytes: LPG envelope.

Eight records = 576 bytes per part. The three host-owned phase words are not counted against `engine_t.mem`.

This is a lower-bound state record, not the final production `metal_voice_state_t`. Main-filter integrators, LPG biquad state, Complex FM envelope, block coefficients/smoothing and any other algorithmic memory remain unaccounted until their fixed-point representations are frozen.

## 4. Complex FM feedback delay

The v98 Complex Oscillator feedback path uses a 3 ms delay. A fixed int32 ring per voice has the following conservative sample counts (ceil of 3 ms × sample rate):

| Sample rate | Samples per voice | Bytes per voice | Eight-voice delay bytes |
|---:|---:|---:|---:|
| 44.1 kHz | 133 | 532 | 4,256 |
| 48 kHz | 144 | 576 | 4,608 |
| 96 kHz | 288 | 1,152 | 9,216 |
| 192 kHz | 576 | 2,304 | 18,432 |

The ring memory is part-scoped and indexed by host voice slot. It is statically allocated inside the engine memory request; no render-time allocation is permitted.

## 5. Known subtotal and headroom

Combining only the 72-byte provisional record and the 3 ms int32 delay rings:

| Sample rate | Voice records | Delay rings | Known subtotal | Remaining in 12,888-byte pool |
|---:|---:|---:|---:|---:|
| 44.1 kHz | 576 | 4,256 | 4,832 | 8,056 |
| 48 kHz | 576 | 4,608 | 5,184 | 7,704 |
| 96 kHz | 576 | 9,216 | 9,792 | 3,096 |
| 192 kHz | 576 | 18,432 | 19,008 | exceeds pool by 6,120 |

The 13,344-byte ADPCM-enabled pool adds 456 bytes of headroom, but does not change the 192 kHz failure. The 192 kHz configuration is therefore excluded for this straightforward eight-voice 3 ms int32 feedback design.

At 96 kHz only 3,096 bytes remain before filter states, FM envelope state, alignment, and any other engine state. 96 kHz remains conditional on completing the full layout and target build assertions; it is not yet approved.

## 6. Retrigger and lifecycle ownership

- Host `voice_t.ph[3]` phase continuity is separately preserved by Felucca during retrigger.
- The two extra METAL oscillator phases, feedback cursor/ring contents, VCA and LPG state live in `engine_t.mem`; the host does not automatically preserve them.
- Fresh-note, retrigger, mono/legato and voice-reuse policies must explicitly decide which of these fields are reset, held or re-triggered.
- VCA retrigger uses cancel-and-hold; LPG retrigger also holds its current level. The feedback delay ring policy is not yet source-verified and must not be assumed to clear or persist.
- `ownenv=1` remains the approved VCA ownership contract. Host amplitude ramps / engine-switch fades must still be multiplied exactly once.

## 7. Remaining CP3.1 blockers

1. Freeze the main filter state representation and its fixed-point limits.
2. Freeze the LPG biquad state representation; the 24-byte LPG envelope is not the LPG filter.
3. Freeze the Complex FM-index envelope state representation.
4. Confirm the feedback delay format, interpolation/read convention and exact retrigger clearing policy.
5. Define a complete typed part-state structure and assert its exact size against both host pool configurations.
6. Add compile-time and target-build evidence for the final structure before production engine registration.
7. Preserve the current `voice_t` ABI and do not allocate dynamically.

## 8. Disposition

CP3.1 is **partially advanced**, not closed. The exact primitive sizes, five-phase requirement, feedback-ring budget and sample-rate limits are now independently testable. The complete engine memory total and final state ownership remain open.

No production engine registration, parameter IDs, persistence changes, audio graph connection, factory firmware modification, firmware package, physical device write or flash was performed.
