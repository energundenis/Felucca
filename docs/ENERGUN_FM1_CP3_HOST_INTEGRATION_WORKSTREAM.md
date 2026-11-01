# ENERGUN FM-1 CORE — CP3 host integration workstream

Base: 77c704bd8ef58e2fa03070fd4fefe3bae59e6d3c
Status: test-only lifecycle contract matrix implemented; exact-head CI evidence pending; production engine remains unregistered.

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
- Current PR head: `067b7c4994dfbda12bc656ff7d44d12ffde85dba`.
- GitHub Actions has not yet produced a run for this exact head. The new test is therefore **not yet marked passed**.

## Remaining host-level work
The new matrix is a test of the engine-side lifecycle contract, not Felucca's actual voice allocator or event dispatcher. It does not prove:
- host slot selection and fresh/released voice reuse policy;
- mono/legato fallback and voice-steal stage behavior;
- actual MIDI/sequencer event dispatch ordering;
- engine memory ownership transitions through `eng_mem_claim()`;
- LFO-to-AMP and engine-switch fade behavior through a registered engine.

These require further tests around the real host functions before production registration.

## Acceptance gates
- Each lifecycle transition has deterministic assertions against real Felucca structs and, where claimed, the actual host allocator/event path.
- No host ABI growth and no production engine registration in this workstream.
- No sample-accurate Gate-off claim; any event-offset API is a separate design decision.
- Full host/web regression passes on the exact reviewed commit.

Do not change engine IDs, parameter IDs, persistence, main branch or device state.
