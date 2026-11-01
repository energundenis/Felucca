# ENERGUN FM-1 CORE — CP3 host integration workstream

Base: 77c704bd8ef58e2fa03070fd4fefe3bae59e6d3c
Status: test-only lifecycle contract matrix implemented; host allocator source trace completed; exact-head CI evidence pending; production engine remains unregistered.

## Objective
Extend the existing real-track adapter from a VCA-only proof into a lifecycle contract harness using Felucca's real track_t / voice_t / vmod_t structures.

## Work
1. Verify the adapter runs on the exact current HEAD in CI.
2. Add explicit tests for fresh slot reuse, sounding retrigger, released-but-not-idle reuse, mono/legato fallback and voice stealing.
3. Verify per-part engine memory isolation and the eng_mem_claim() ownership boundary.
4. Verify Gate-off remains block-boundary and done() retires only after the internal VCA is idle.
5. Preserve host amplitude ramp, LFO-to-AMP and engine-switch fade exactly once.
6. Test explicit Accent metadata independently of MIDI velocity, including velocity 127 without Accent.

## Implemented increment — lifecycle contract matrix
- Added `tests/metal_core_cp3_lifecycle_contract_test.c`, compiled against Felucca's real `core.h` structures and existing VCA implementation.
- The matrix checks all eight voice-slot mappings, aligned per-part state isolation across four parts, invalid pointer rejection, velocity 127 without Accent, explicit Accent metadata, released-but-not-idle retirement guard, cancel-and-hold retrigger, and fresh-slot reset before reuse.
- Added the test to `tests/run_metal_core_fast.sh` using strict C11, `-Wall -Wextra -Werror`.
- GitHub Actions has not yet produced a run for the current PR head. The new test is therefore **not yet marked passed**.

## Implemented increment — host source trace
- Added `docs/ENERGUN_FM1_CP3_HOST_LIFECYCLE_SOURCE_TRACE.md`, tracing the actual `voice_alloc()`, `voice_victim()`, `voice_start()`, `mono_play()`, `engine_block()` and `eng_mem_claim()` source paths.
- Confirmed there are 8 slots per part but one shared global budget of 8 sounding voices across 4 parts.
- Confirmed victim priority: oldest released, then extra UNISON, then (hard allocation) oldest held non-bass POLY voice; the lowest held note is protected.
- Confirmed same-part in-place restart differs from a cross-part stage-4 fade/steal.
- Confirmed MONO retriggers while LEGATO/UNISON can update pitch without retriggering the envelope; queued note metadata preserves Accent separately from velocity.
- Confirmed engine memory is per-part and zeroed on ownership change; switching to an engine with no memory leaves the previous owner/state intact.
- This is source inspection, not runtime test evidence. The trace explicitly lists untested host allocator and engine-switch acceptance cases.

## Remaining host-level work
The lifecycle matrix is an engine-side contract test, not execution of Felucca's static allocator/event dispatcher. Still required:
- host-level shared eight-voice budget and victim-priority tests;
- same-part in-place restart versus cross-part stage-4 steal;
- MONO retrigger, LEGATO pitch-only update, LOW/HIGH priority fallback and UNISON under budget pressure;
- queued Accent metadata through engine-switch fade;
- actual `eng_mem_claim()` ownership-change and no-memory-engine preservation tests;
- host amplitude ramp, LFO-to-AMP and engine-switch fade exactly once;
- exact-head CI for the new matrix and source changes.

## Acceptance gates
- Each lifecycle transition has deterministic assertions against real Felucca structs and, where claimed, the actual host allocator/event path.
- No host ABI growth and no production engine registration in this workstream.
- No sample-accurate Gate-off claim; any event-offset API is a separate design decision.
- Full host/web regression passes on the exact reviewed commit.

Do not change engine IDs, parameter IDs, persistence, main branch or device state.
