# ENERGUN METAL CORE — Host-side MCX motion model and test plan 01

Date: 2026-10-31
Branch: energun-metal-core
Status: implementation specification only. No host model or firmware source changed. MCX automation remains unapproved pending explicit user decision.

## 1. Purpose and boundary

Define a deterministic host-side reference model for MCX base values, automation and Parameter Locks. The model is intended to validate semantics and event ordering before any firmware implementation.

It is not a firmware emulator, audio DSP emulator, Flash simulator or editor implementation. It must not write device storage or touch the physical FM-1. It must not modify the factory V1.5 firmware.

The model should be a standalone test module under `tests/`, independent of `track_t.p[]` and the ordinary motion store. It may later be linked into the existing hostsim test build, but must not change the firmware source include graph.

## 2. Reconcile the capacity proposals

The earlier Sidecar Motion Design proposed 128 MCX events/project, while the later Automation Proposal recommended 64 as an initial target. Neither value has been approved or benchmarked.

For the first host-model tests:
- Make capacity a compile-time/test configuration, not a serialized-format constant.
- Run the complete test suite at 64 and 128 events.
- Keep the proposed serialized record at four bytes in the separate MCX namespace.
- Do not choose a production cap until target step-time and RAM measurements are available.
- Never add a second unindexed 128-record scan to the audio ISR.

This avoids prematurely freezing either proposal.

## 3. Model data

### Base state

Four tracks × seven MCX controls:
- `base[4][7]`: saved patch values.
- `automation_value[4][7]`: most recent automation value reached in the current pattern pass.
- `runtime_value[4][7]`: value currently exposed to the simulated DSP parameter sink.
- `automation_valid[4][7]`: whether a current-pass automation value exists.
- `locked[4][7]`: whether the current step has an active lock for the control.

Use integer quantized values matching the future descriptor table; do not use floats as serialized event values. The model should receive descriptors from a single table containing min, max, default, step and unit. Descriptor values must be reconciled against the v98 source before implementation.

### Event record

Keep the separate four-byte MCX record concept:
- place: `(track << 6) | step`;
- param: MCX ID in bits 0..6, lock kind in bit 7;
- value: signed 8-bit quantized value.

Validate track 0..3, step 0..63, ID 0..6, value against descriptor, count against configured capacity, and unique (track, step, ID). Automation and lock for the same ID/step are mutually exclusive under the current one-record-per-tuple proposal; if product behavior requires both, the format must be redesigned rather than silently accepting duplicates.

## 4. Step-processing contract

The test model receives explicit events from a simulated sequencer scheduler:
- track;
- step index;
- whether chance/condition passed;
- whether this is loop step zero;
- whether this is a stop/restart/rebase boundary;
- trigger/ratchet count;
- whether a note trigger occurs.

The production insertion point must mirror current `seq_play()` ordering:
1. Resolve chance/condition result.
2. Process normal motion.
3. Process MCX motion at the same actual scheduled step time (including NUDGE).
4. Run note trigger and ratchet processing.

At each MCX step:
1. If loop step zero, clear transient locks and rebase automation state according to the approved loop semantics.
2. Restore locks from the previous step before applying the new step's events.
3. Apply automation events at this step regardless of chance/condition result.
4. Apply Parameter Locks only if chance/condition passed.
5. Ensure the resulting values are visible before the first note trigger on this step.
6. Keep locks active through every ratchet on this step.
7. At the next step, restore each locked ID to its latest automation value, or saved base value if no automation has occurred.

Stop/restart, engine change, project load and preset load must each have explicit reset/rebase tests. Do not assume all boundaries have identical semantics until reconciled with normal motion and project lifecycle source.

## 5. Parameter timing model

The reference behavior proposal is:
- FM ENV ATTACK / FM ENV DECAY / METAL DECAY / LPG DECAY: set the parameter used by the next eligible envelope trigger; do not restart or reshape an envelope already in progress.
- WAVEFOLDER MODULE VOLUME / LPG MODULE VOLUME / VOICE VOLUME: update the simulated live target immediately, with the production DSP required to apply a click-safe smoothing ramp.

For time controls, the event must be processed before the trigger on that step. For volume controls, the model records a target change; smoothing itself belongs to DSP and is outside this motion model.

The LPG volume coupling observed in v98 must be preserved. The model must not invent an independent post-envelope gain stage.

## 6. Index and bounded work

The host model should compare:
A. linear event scan (reference only);
B. fixed per-track/per-step index.

The production candidate is B:
- build/rebuild index outside the ISR;
- at runtime inspect only events attached to the current track/step;
- at most seven unique IDs can exist on one track/step under the current uniqueness rule;
- restore from per-track state, never by rescanning the entire event pool;
- publish rebuilt index atomically relative to the sequencer callback.

Test index equivalence against the linear reference for randomized valid event pools and event orderings. Include malformed stores and rejected duplicate tuples.

The model should report:
- index RAM bytes for 64 and 128 event capacities;
- worst-case number of event records inspected per step;
- maximum per-step MCX operations;
- memory for base, automation, runtime and lock state.

Host timing is useful for algorithmic comparisons but is not evidence of AC791N ISR timing. Target cycle/step-time benchmark remains mandatory.

## 7. Required tests

### Validation
1. Valid automation and lock records accepted.
2. Invalid track, step, ID, value, count and duplicate tuple rejected.
3. Malformed/corrupt MCX store fails closed to base defaults and empty events.
4. 64- and 128-event configurations both enforce their own bounds.

### Ordering and behavior
5. Automation applies on a step even if chance/condition fails.
6. Lock applies only when chance/condition passes.
7. Time lock is visible before note-on on that step.
8. All ratchets on the step observe the same lock value.
9. Lock restores on the next step to latest automation value.
10. Lock restores to saved base when no automation value exists.
11. Multiple automation events update the base in step order.
12. Lock at step N and automation at N+1 restore correctly.
13. Loop step zero has deterministic and documented rebase behavior.
14. NUDGE moves processing to actual playback time without double application.
15. Four tracks remain isolated.

### Lifecycle and robustness
16. Stop and restart clear transient locks.
17. Engine change disables MCX state on non-Metal-Core engines and restores it on return according to approved policy.
18. Project load and preset load rebase base and automation state.
19. Clear track, clear locks, replace event list and undo/redo do not mutate other tracks or normal motion.
20. Index rebuild never exposes a partial table to the step callback.
21. Randomized property test: indexed output equals linear reference output for all generated valid event sets.
22. Maximum event pool plus 128 normal motion records remains within a measured target budget.

## 8. Existing test integration

The repository's `tests/hostsim.c` includes `firmware/src/seq.c`, and `tests/editor_test.c` includes the host simulator and exercises editor/USB/storage behavior. Do not add MCX implementation to `seq.c` or `hostsim.c` at the specification stage.

Recommended first code artifact, after scope approval:
- `tests/mcx_motion_model.c` — standalone deterministic model and unit tests;
- `tests/mcx_motion_model.h` — model interface;
- optional `tests/mcx_motion_test.c` — test runner, depending on existing build conventions.

Only after the standalone model passes should its semantics be mapped to the actual `seq_play()` insertion point and the editor protocol.

## 9. Open product decisions

The following remain proposals and are not approved by this document:
1. Whether all seven MCX parameters are V1 Automation/Parameter Lock destinations.
2. Whether the production MCX event capacity is 64 or 128 (target benchmark decides).
3. Whether automation values persist across loop restart exactly as normal Felucca motion or reset to saved patch base; this must be confirmed from `motion_restore()` / loop behavior and explicitly specified.
4. Whether one event per (track, step, ID) is sufficient if a user wants both automation and lock on the same parameter and step.
5. Exact descriptor ranges/defaults and quantization for all seven MCX controls.

## 10. Safety boundary

Documentation only. No source code, host test, firmware image, UI, editor protocol, parameter IDs, project or backup format, Flash allocation, factory firmware or physical device was changed.
