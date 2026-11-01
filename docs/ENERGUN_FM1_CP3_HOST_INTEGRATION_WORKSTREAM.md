# ENERGUN FM-1 CORE — CP3 host integration workstream

Base: 77c704bd8ef58e2fa03070fd4fefe3bae59e6d3c
Status: launched; test-only adapter; production engine remains unregistered.

## Objective
Extend the existing real-track adapter from a VCA-only proof into a lifecycle contract harness using Felucca's real track_t / voice_t / vmod_t structures.

## Work
1. Verify the adapter runs on the exact current HEAD in CI.
2. Add explicit tests for fresh slot reuse, sounding retrigger, released-but-not-idle reuse, mono/legato fallback and voice stealing.
3. Verify per-part engine memory isolation and the eng_mem_claim() ownership boundary.
4. Verify Gate-off remains block-boundary and done() retires only after the internal VCA is idle.
5. Preserve host amplitude ramp, LFO-to-AMP and engine-switch fade exactly once.
6. Test explicit Accent metadata independently of MIDI velocity, including velocity 127 without Accent.

## Acceptance gates
- Each lifecycle transition has deterministic assertions against real Felucca structs.
- No host ABI growth and no production engine registration in this workstream.
- No sample-accurate Gate-off claim; any event-offset API is a separate design decision.
- Full host/web regression passes on the exact reviewed commit.

Do not change engine IDs, parameter IDs, persistence, main branch or device state.
