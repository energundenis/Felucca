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

The compile-time probe's initial provisional record is 72 bytes per voice:
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

## 5. Filter and FM-envelope state: provisional representation

The protected v98 reference instantiates two independent low-pass biquads in series:
- the main low-pass filter (`wcFilt`), with user CUTOFF / RESONANCE;
- the LPG low-pass filter (`wcLpgFilter`), with a per-trigger cutoff transient and return toward main CUTOFF.

They cannot share integrator state. For a bounded fixed-point Direct Form II Transposed implementation, the minimum audio-history state is two 32-bit delay values per biquad. This yields four int32 values = 16 bytes per voice for the two filters. Coefficients are part-scoped / block-updated data, not duplicated per voice in this subtotal. This is a proposed implementation format, not an implemented filter or a target-verified numeric format.

The Complex FM-index envelope is independent from both filters, the LPG gain envelope and the VCA. The v98 source specifies base, peak = base × (1 + FM ENV AMT × 4), attack and decay. A provisional 24-byte record (level, base, peak, attack/decay coefficient or timing fields, remaining counter and phase) is reserved per voice for accounting only. Its exact field semantics and time-constant mapping remain to be frozen before DSP implementation.

Adding these provisional states to the prior 72-byte lower-bound voice record gives:
- 72 bytes: two extra oscillator phases, feedback cursor, VCA and LPG gain envelope;
- 16 bytes: two independent biquad history pairs;
- 24 bytes: Complex FM-index envelope candidate;
- **112 bytes per voice / 896 bytes per part for eight voice slots.**

Combined with the 3 ms int32 feedback rings, the expanded *candidate subtotal* is:

| Sample rate | Provisional voice records | Feedback rings | Expanded subtotal | Remaining in 12,888-byte pool |
|---:|---:|---:|---:|---:|
| 44.1 kHz | 896 | 4,256 | 5,152 | 7,736 |
| 48 kHz | 896 | 4,608 | 5,504 | 7,384 |
| 96 kHz | 896 | 9,216 | 10,112 | 2,776 |
| 192 kHz | 896 | 18,432 | 19,328 | exceeds pool by 6,440 |

The 96 kHz configuration now has only 2,776 bytes remaining before per-part coefficient tables, parameter smoothing, alignment, oscillator/wavefolder scratch, and any additional state. Treat 96 kHz as high risk and not approved for V1 until the full part structure and target compiler assertions pass. 44.1 kHz remains the only initial target in the CP1.9 recommendation.

The host reference's BiquadFilter is a Web Audio implementation; the 2-state fixed-point proposal is not asserted bit-equivalent. The fixed-point filter algorithm, coefficient quantization, resonance stability and cutoff modulation must pass LPG-01/LPG-02 before integration.

## 6. Explicitly unbudgeted state

The following cannot yet be honestly assigned exact bytes because the target algorithm/layout is not frozen:
- per-part filter coefficient caches and cutoff/resonance smoothing;
- oscillator increment/smoothing and FM/AM mode-transition state;
- wavefolder oversampling scratch and interpolation history;
- block-level control snapshots and any per-part modulation values;
- alignment padding for the final production part-state structure.

Do not hide these in the remaining headroom or count them as zero. The current expanded subtotal is a conservative state inventory candidate, not the complete engine memory requirement.

## 7. Earlier lower-bound subtotal and headroom

Combining only the 72-byte provisional record and the 3 ms int32 delay rings (excluding the candidate filter and FM-envelope records):

| Sample rate | Voice records | Delay rings | Lower-bound subtotal | Remaining in 12,888-byte pool |
|---:|---:|---:|---:|---:|
| 44.1 kHz | 576 | 4,256 | 4,832 | 8,056 |
| 48 kHz | 576 | 4,608 | 5,184 | 7,704 |
| 96 kHz | 576 | 9,216 | 9,792 | 3,096 |
| 192 kHz | 576 | 18,432 | 19,008 | exceeds pool by 6,120 |

The 13,344-byte ADPCM-enabled pool adds 456 bytes of headroom, but does not change the 192 kHz failure. The 192 kHz configuration is therefore excluded for this straightforward eight-voice 3 ms int32 feedback design.

At 96 kHz only 3,096 bytes remain before filter states, FM envelope state, alignment, and any other engine state. 96 kHz remains conditional on completing the full layout and target build assertions; it is not yet approved.

## 8. Retrigger and lifecycle ownership

- Host `voice_t.ph[3]` phase continuity is separately preserved by Felucca during retrigger.
- The two extra METAL oscillator phases, feedback cursor/ring contents, VCA and LPG state live in `engine_t.mem`; the host does not automatically preserve them.
- Fresh-note, retrigger, mono/legato and voice-reuse policies must explicitly decide which of these fields are reset, held or re-triggered.
- VCA retrigger uses cancel-and-hold; LPG retrigger also holds its current level. The feedback delay ring policy is not yet source-verified and must not be assumed to clear or persist.
- `ownenv=1` remains the approved VCA ownership contract. Host amplitude ramps / engine-switch fades must still be multiplied exactly once.

## 9. Remaining CP3.1 blockers

1. Freeze the main filter state representation and its fixed-point limits.
2. Freeze the LPG biquad state representation; the 24-byte LPG envelope is not the LPG filter.
3. Freeze the Complex FM-index envelope state representation.
4. Confirm the feedback delay format, interpolation/read convention and exact retrigger clearing policy.
5. Define a complete typed part-state structure and assert its exact size against both host pool configurations.
6. Add compile-time and target-build evidence for the final structure before production engine registration.
7. Preserve the current `voice_t` ABI and do not allocate dynamically.


## 11. v98 filter source trace and state-layout decision (2026-11-01)

Exact reference source inspection confirms:
- `wcFilt` is a Web Audio `BiquadFilterNode`, type `lowpass`; its frequency is initialized from CUTOFF and Q from RESONANCE.
- CUTOFF changes schedule `wcFilt.frequency.setTargetAtTime(..., 20 ms)`; RESONANCE changes schedule `wcFilt.Q.setTargetAtTime(..., 20 ms)`.
- Sequencer Accent temporarily raises the main-filter cutoff to at most 1.15 × base cutoff (capped at 12 kHz), then schedules a return at 65% of Gate. Without Accent, the trigger schedules the base cutoff with a 10 ms time constant.
- `wcLpgFilter` is a second independent Web Audio `BiquadFilterNode`, type `lowpass`, initialized at 1,800 Hz and Q=0.7.
- The LPG RESPONSE control handler also directly schedules the LPG filter frequency to 500 + RESPONSE × 7,500 Hz (25 ms). On trigger, the LPG automation cancels its pending frequency events, sets the transient target to min(12 kHz, CUTOFF × (1.25 + RESPONSE × 3.75)), then returns toward max(100 Hz, CUTOFF). LPG filter return event and time constant are separate from LPG gain-envelope timing.
- The two filters are serial and independent; one filter's history cannot be reused for the other.

**Important correction to the budget interpretation:** Web Audio specifies behavior, not the target C structure or state topology. The reference does not establish that a fixed-point port must use Direct Form II Transposed, nor does it establish coefficient precision, state width, saturation, or the exact number of persistent words. The 16-byte figure in Section 5 is therefore a *candidate* (two int32 history words per mono biquad × two filters), not a source-proven or implementation-exact memory requirement. Keep it in the candidate subtotal only; do not use it as a final compile-time engine-state contract.

The 24-byte FM-index envelope entry is likewise a candidate accounting record, not an implemented state type. The reference scheduling semantics are confirmed: base = current FM INDEX after character/drive scaling and modulation offsets; peak = base × (1 + FM ENV AMT × 4); attack begins after the sequencer trigger offset `ea`, with tau max(2 ms, ea/3); return to base begins at `ea + ed`, with tau max(5 ms, ed/3). The FM-index envelope is independent of voice VCA and LPG gain envelope. Exact fixed-point interpolation and storage layout remain a target design decision.

### CP3.1 filter implementation gate

Before the 16-byte candidate can be accepted, the port must select and test:
1. mono per-voice topology and the exact biquad realization (e.g. DF2T);
2. coefficient format/range and stable low-pass coefficient generation across supported CUTOFF / RESONANCE values;
3. state format, saturation and numeric headroom;
4. block-rate coefficient update / parameter smoothing, preserving the source's distinct 10/20/25 ms automation behaviors where applicable;
5. retrigger cancellation/hold behavior for both filter cutoff trajectories;
6. target compiler structure size and worst-case render cost.

No filter DSP was added in this increment. No final engine-state size is approved.

## 12. Disposition

CP3.1 is **partially advanced**, not closed. The exact primitive sizes, five-phase requirement, feedback-ring budget and sample-rate limits are now independently testable. The complete engine memory total and final state ownership remain open.

No production engine registration, parameter IDs, persistence changes, audio graph connection, factory firmware modification, firmware package, physical device write or flash was performed.


## 13. Sample-rate anchor and fractional feedback-delay requirement (2026-11-01)

A direct host-source check of `firmware/src/core.h` finds:

```c
#define HALF_FRAMES 128 /* I2S half buffer: 2.9 ms at 44.1 kHz */
```

This is source evidence that the FM-1/Felucca audio path is designed around a nominal 44.1 kHz rate. It is stronger than treating 44.1 kHz as an undocumented guess, but the comment is not a runtime sample-rate query and does not prove every possible board/build configuration. Keep the initial V1 budget at 44.1 kHz; before freezing the production engine, confirm the actual I2S clock / sample-rate configuration in the board and audio-driver sources. Do not silently generalize the 44.1 kHz budget to 48 kHz or higher.

At 44.1 kHz, the v98 3 ms Complex FM delay corresponds to:

```text
0.003 s × 44,100 samples/s = 132.3 samples
```

Consequences:
- A 133-sample int32 ring is the minimum straightforward storage capacity for the 3 ms delay at 44.1 kHz (532 bytes per voice; 4,256 bytes for eight voices).
- A plain integer read offset of 133 samples would be 3.016 ms, not exactly 3 ms.
- The reference uses a time-based Web Audio DelayNode, so the target implementation must account for the fractional 0.3-sample component if faithful 3 ms timing is required.
- Linear interpolation is a possible low-cost candidate, not a source-proven requirement. The read convention, interpolation quality, fixed-point fraction and CPU cost remain open until measured.
- The existing 133-sample memory subtotal remains a capacity estimate. Any interpolation state must be included in the final typed layout (it may be representable in a fractional cursor and need not require another sample buffer).

### Feedback state lifecycle — evidence boundary

The v98 design audit confirms that the 3 ms delay belongs to the Complex Oscillator modulator feedback loop, not METAL FM and not the wavefolder. It does not establish the exact target voice-allocation lifecycle or whether the reference clears a delay line on every note trigger.

Therefore no reset/persist policy is frozen yet:
- active-voice retrigger must be distinguished from fresh allocation / reuse of an idle voice slot;
- the host preserves `voice_t.ph[3]` on retrigger, but does not preserve engine-memory ring contents for the engine;
- the target engine must explicitly implement whichever policy the reference behavior and host lifecycle require;
- a test must cover stale feedback energy after voice reuse, continuity on active retrigger, and release/retirement.

Do not clear the ring or preserve it across all note-ons by assumption. First inspect the protected v98 note/voice lifecycle and the Felucca allocator path together, then encode the result as a testable contract.

### Updated CP3.1 sub-gate

1. Confirm actual board I2S sample rate from source / build configuration.
2. Choose a 132.3-sample 3 ms delay implementation (or document a measured equivalent).
3. Freeze active-retrigger versus fresh-slot ring lifecycle.
4. Add host tests for fractional delay response, bounded feedback gain (0..0.8), stale-state isolation and deterministic reset.
5. Recalculate exact memory and target render cost using the selected delay representation.

This section adds no DSP implementation and does not close CP3.1.


## 14. Felucca voice-reuse source trace — feedback lifecycle candidate (2026-11-01)

Inspected `firmware/src/voice.c` on this branch, especially `voice_alloc()`, `voice_start()` and `env_tick()`.

Confirmed host behavior:
- `voice_start()` snapshots `sounding = v->active && v->stage != 0` before it changes the voice to the new note's active/attack state.
- It calls the engine's `note_on()`, then, for a sounding non-sampled voice, restores all three host phase words and the `voice_t.s[]` slots selected by `engine_t.keep`.
- The source comment explicitly describes this as “retrigger / steal: keep phases and filter” because resetting those states clicks.
- A voice whose envelope reaches its completed state is set to `active=0`, `stage=0`; a later allocation is therefore a fresh note rather than a sounding retrigger.
- Engine-owned `ENG_MEM` state is not part of the host snapshot/restore operation.

This makes the feedback-ring lifecycle more specific than the earlier open question. Candidate contract for a per-voice Complex FM feedback ring:
1. **Sounding retrigger / in-place steal:** preserve ring samples and cursor, just as the host preserves oscillator phases and selected filter state. Resetting the ring here would violate the host's explicit continuity intent and may click.
2. **Fresh note in an inactive voice slot:** clear the ring and reset its cursor before starting the new contour, preventing feedback energy from a previous occupant leaking into the new note.
3. **Completed voice:** once the engine reports VCA idle through `done()`, the ring may remain allocated but is logically stale; the next fresh `note_on()` must clear it before audio processing.
4. **Engine switch:** `eng_mem_claim()` clears the part's shared memory region when ownership changes, so no old engine feedback state is carried into METAL CORE.

Implementation detail candidate: the VCA's explicit `phase == ENG_METAL_VCA_IDLE` can distinguish a completed/fresh slot from a still-sounding envelope at `note_on()`, because the host overwrites `voice_t.active/stage` before invoking the callback. This must be proven in a test-only note-on lifecycle adapter; do not infer the old host stage from the already-updated `voice_t`.

Remaining caveat: this host trace establishes the continuity expectation for a sounding voice, but does not inspect the protected v98 reference's exact node allocation / oscillator start lifecycle. Before freezing the candidate as final v98 equivalence, compare the original reference's per-voice node construction and trigger handling. Keep the behavior marked as a host-contract candidate until that check and its tests pass.

### Required lifecycle regression

- Start a voice, write nonzero feedback samples, then retrigger while its VCA is non-idle: ring cursor and samples remain unchanged.
- Reuse the same slot after VCA reaches IDLE: ring samples and cursor are cleared before first new render.
- Steal/retrigger a released-but-not-yet-idle voice: preserve state and avoid a discontinuity.
- Run two voice slots concurrently: clearing one slot must not affect the other.
- Engine switch / part memory claim: all ring state starts zeroed.


## 15. Host-only fractional-delay prototype and regression (2026-11-01)

Added a deliberately isolated candidate under `tests/`:
- `tests/metal_core_feedback_delay_candidate.h`
- `tests/metal_core_feedback_delay_candidate_test.c`

Candidate behavior:
- fixed 133-sample int32 ring for nominal 44.1 kHz;
- read-before-write convention;
- Q15 linear interpolation with fractional component 0.3 (stored as 9830 / 32768);
- impulse response split across samples 132 and 133, approximately 0.7 / 0.3;
- explicit bounded cursor wrap and reset;
- independent ring instances per voice.

The test compiled and passed locally with `cc -std=c11 -Wall -Wextra -Werror -O2`. It checks the two fractional impulse taps, reset-to-zero behavior, ring capacity and voice-slot isolation. It is included in both the full host test runner and the fast 11-test lane.

This is **host-only candidate evidence**, not production DSP approval:
- no feedback gain / closed-loop stability test has been added yet;
- no audio-rate modulation output is generated by this test;
- target pi32v2 64-bit multiply cost and worst-case callback budget are not measured;
- interpolation response has not been compared against the protected v98 browser reference;
- the voice retrigger/reset policy remains a host-contract candidate pending lifecycle adapter tests.

Do not include this test helper in the firmware image or claim the AC79 target compiler has verified it.


## 16. Test-only lifecycle discriminator probe (2026-11-01)

Extended `metal_core_feedback_delay_candidate_test.c` to include the actual `eng_metal_core_vca.h` phase constants and a small test-only `note_on` policy adapter:

- prior VCA ATTACK: preserve ring cursor and contents;
- prior VCA RELEASE: preserve ring cursor and contents (released voice is still sounding);
- prior VCA IDLE: clear ring before the fresh note.

The test compiled and passed locally; it is now part of both the full host suite and the fast 11-test lane. This supports the candidate use of the previous VCA phase as an engine-owned lifecycle discriminator, since `voice_start()` overwrites host `active/stage` before calling `note_on()`.

This is still an adapter test, not a production callback. It does not prove every mono/legato/voice-steal transition or the protected v98 graph's node lifetime. Those remain acceptance tests for the real integration stage.


## 17. Host-only closed-loop feedback stability regression (2026-11-01)

Extended `tests/metal_core_feedback_delay_candidate_test.c` with a test-only closed-loop harness around the provisional 132.3-sample delay:

- feedback gains: 0, 0.25, 0.5 and approximately 0.8 (Q15);
- positive and negative full-scale impulses;
- 8,192 sample iterations per case;
- checks for int32 bounds, polarity preservation, no growth above the initial impulse, echo decay, and deterministic repeated output;
- zero gain must produce no echoes; nonzero gains must produce echoes whose late tail is below the first echo.

The loop harness uses an int64 intermediate and integer division to isolate the delay/gain stability property. It deliberately does **not** define the production feedback multiplier's rounding or saturation policy. The test remains host-only and is part of the existing CP3.1 feedback candidate test; it does not add a separate CI job or change the fast-lane test count (11).

This is bounded-loop evidence for the candidate ring at nominal 44.1 kHz, not proof of audible stability, equivalence to the protected v98 Web Audio DelayNode, AC79 callback timing, or production DSP safety. The ring/interpolation implementation remains unapproved for production until:
1. the candidate is compared with the v98 reference impulse and repeated-feedback response;
2. the actual AC79 target compiler code generation and worst-case render cost are measured;
3. the full engine state and fixed-point gain path pass target-side overflow and clipping tests.

No production DSP, engine registration, parameter map, persistence, audio graph, hardware access, or firmware flashing was changed by this regression increment.
