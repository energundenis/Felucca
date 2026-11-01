# ENERGUN METAL CORE — CP3 integration readiness plan

Date: 2026-11-01
Branch: `energun-metal-core`
Status: planning gate only; no production engine registration or DSP graph change.

## Entry evidence

CP2.23 VCA-only integration harness passed on commit `ed8a35421aa185306659c35636bcd7200ca8228e`, GitHub Actions run [41242563432](https://github.com/energundenis/Felucca/actions/runs/41242563432).

- Host VCA harness: PASS, 0 failures.
- Eight independent VCA states: 36 bytes each, 288 bytes total per part.
- Per-part memory fit: 288 / 12,888 bytes.
- Gate-off, Release, Retrigger, idle and `done()`: PASS.
- Additive render and host Q15 amplitude ramp: PASS.
- AC79 pi32v2 compile/disassembly probe: PASS.
- Existing complete host/web suite: PASS; 98 golden renders unchanged, 0 health failures, 0 voice/routing failures, 0 host CPU entries over budget.
- Existing target render-loop static budget script: PASS with the repository's existing thresholds; no thresholds were raised.

The target-budget script evaluates existing Felucca render loops. It does not yet include the METAL CORE VCA or a complete METAL CORE render path. The combined VCA target probe is compile/disassembly evidence only, not a cycle-accurate measurement.

## CP3 scope

CP3 means a real, registered Felucca engine with a bounded audio render callback. It does not include a hardware flash, factory firmware modification, or sequencer V0.3 integration. Sequencer work remains its own CP5 stream.

Approved signal order remains:

```text
OSCILLATORS → FM / METAL → FOLD → MAIN FILTER → LPG → VOICE VCA → OUTPUT
```

FOLLOW and METAL remain independent controls in the OSCILLATORS / SYNTH VOICE block. The detailed reference tap points must be preserved; this compact diagram must not remove the separate METAL ring contribution or module-level gain controls documented by the v98 reconciliation.

## Required staged implementation

### CP3.0 — integration contract freeze

1. Reconcile the earlier ADSR compatibility audit with the later CP2.19 decision: FM-1 live/manual and sequencer notes use the dedicated full VCA contour; host ADSR must not be applied a second time. The VCA-only harness currently exercises the own-envelope (`ownenv=1`) callback contract.
2. Preserve the Felucca shared voice allocator, MIDI routing, host track mixer, stereo and global FX path.
3. Keep host amplitude modulation / engine-switch fades multiplied into the engine output exactly once.
4. Gate-off is currently observed at render-block entry. Do not claim sample-accurate sub-block note-off. A future host API extension is a separate decision; CP3 must explicitly test and document the block-boundary limitation.
5. Accent comes only from explicit `NOTE_META_ACCENT`; never infer Accent from MIDI velocity 127.
6. Freeze fresh-note, retrigger, mono/legato, voice-steal and voice-retirement semantics before the real callback is registered.

### CP3.1 — complete engine state accounting

- Keep the eight 36-byte VCA objects in bounded per-part engine memory unless a source audit proves a safer per-voice placement.
- Account for oscillator phases, fold/filter integrators, FM feedback, LPG state and all other voice data against the unchanged `voice_t` ABI (`ph[3]`, `s[8]`).
- Do not allocate dynamically or retain transient host pointers.
- Add compile-time assertions for the complete `engine_t.mem` size and active `ENG_MEM_PART` bound. Do not assume a single build configuration's memory pool size.
- Define retrigger keep/reset behavior for every used state slot.

### CP3.2 — connect the real fixed-point DSP chain

Integrate in small reviewable increments, each with standalone host tests:
1. OSC source and pitch/FOLLOW adapter.
2. Complex FM and METAL contribution, with independent ratios and bounded feedback.
3. FOLD transfer function and module gain.
4. Main filter and LPG filter/gain/envelope.
5. VCA as the final per-voice envelope stage, followed by additive output into Felucca's mix.

No floating point, unbounded loops, heap allocation, blocking work, or per-sample expensive coefficient calculation in the real-time render path. Coefficients and parameter smoothing should be prepared at control/block rate.

### CP3.3 — temporary integration controls and registration

- Before assigning production parameter IDs, complete the approved V1 include/defer map for the 37-control reference inventory and resolve storage/editor/preset/backup implications.
- Do not consume parameter IDs or change persistence formats speculatively.
- A temporary test-only engine descriptor may be used before production registration, but it must not be exposed as a user-selectable engine or written into saved projects.
- Production engine ID allocation is append-only and must preserve optional SLICE/ADPCM builds.

### CP3.4 — acceptance evidence

- Host tests: fresh notes, all MIDI velocities, explicit Accent, short/long Gate, retrigger, mono/legato, eight-voice isolation, voice stealing, engine switch, release completion and no hanging notes.
- DSP tests: bounded output, no DC, no unexpected clipping, stable FM feedback, parameter corner cases, smooth parameter changes and deterministic silence after release.
- Audio regression: reference renders and health tests for all supported source combinations.
- Resource checks: full-engine per-part memory, target render-loop static budget with no threshold increase, and a documented target cycle-measurement plan.
- Full Felucca host/web regression and AC79 target compile on the exact implementation HEAD.

## Explicit non-goals / safety

- No physical FM-1 flashing, erase, or device write.
- No factory firmware replacement.
- No changes to `main`.
- No sequencer V0.3 integration in CP3.
- No claim of hardware sound quality or sample-accurate Gate timing without direct evidence.

## Current blockers before production registration

1. Finalize the 37-control V1 include/defer and parameter/storage map.
2. Confirm the full engine state map and retrigger ownership.
3. Decide whether block-boundary Gate timing is acceptable for the first firmware prototype or whether a host event-offset extension is required.

CP3 implementation may proceed in isolated test code while these are resolved, but production registration and user-facing parameter/storage changes remain gated.


## CI checkpoint — 2026-11-01

- Full baseline workflow: https://github.com/energundenis/Felucca/actions/runs/41322047870 — SUCCESS; all 16 principal steps completed.
- Exact tested source: 542126c0498f177f38f6f14525ac3042806cfdb7 on DSP work branch energun/cp3-1-biquad-prototype-01 (PR #1 head), not the coordination branch.
- Unmodified Felucca firmware build: PASS.
- Isolated ENERGUN VCA compile with pinned AC79 pi32v2 target compiler: PASS.
- Felucca Host Regression and Web Regression: PASS.
- Build output/size recording and experimental evidence upload: PASS.
- Companion fast regression: https://github.com/energundenis/Felucca/actions/runs/41322047771 — SUCCESS on the same SHA.
- Biquad candidate host regression + AC79 compile/disassembly probe: https://github.com/energundenis/Felucca/actions/runs/41322047859 — SUCCESS on the same SHA.
- AC79 biquad inspection is compile/code-generation evidence only, not runtime cycle timing or full-engine CPU proof.
- This verifies the tested DSP candidate commit only. It does not mean PR #1 is merged, METAL CORE is production-registered/playable, or the coordination branch has the same tested tree.
- Coordination branch HEAD at checkpoint start: e6877d47f6b5684efab1e2cf759ed37b2ea76cd0. Main remains untouched.
- No device write or firmware flash occurred; factory firmware remains unchanged.

This is a positive CI qualification checkpoint for the DSP candidate SHA. It does not close integration readiness gates: review/merge approval, production DSP integration, actual host allocator/lifecycle matrix, full-engine RAM/CPU accounting and runtime timing remain required.
