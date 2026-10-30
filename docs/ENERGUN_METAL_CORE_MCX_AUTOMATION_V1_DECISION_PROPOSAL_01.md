# ENERGUN METAL CORE — MCX automation V1 decision proposal 01

Date: 2026-10-31
Branch: energun-metal-core
Status: design proposal only; requires user approval. No firmware/editor/emulator source changed.

## 1. Recommendation

Make all seven persisted MCX extension parameters available to both Automation and Parameter Lock in V1, using the separate MCX namespace and storage designed in SIDECAR_EXTENSION_MOTION_DESIGN_01.

Do not route MCX events through Felucca's ordinary P_* motion functions, P_COUNT, MOTION_LOCK namespace, or track_t.p[].

Initial capacity recommendation: 64 MCX events per project, shared across four tracks. Do not enable a second 128-event linear scan on every sequencer step. Before implementation, build a bounded runtime index or step buckets so the sequencer processes only MCX events relevant to the current step. Benchmark worst-case event application, lock restoration, ratchets, and editor updates on the AC791N before accepting the capacity. If the indexed design cannot meet timing and memory budgets, reduce capacity further; do not silently raise it.

## 2. Seven MCX parameters and application timing

All seven values are persisted per track. All seven are eligible for automation and parameter locks, with these application semantics:

| MCX | Parameter | Automation / lock application |
|---|---|---|
| MCX0 | FM ENV ATTACK | Update the Complex FM modulation-envelope attack used by the next eligible note trigger; do not restart an envelope already in progress. |
| MCX1 | FM ENV DECAY | Update the Complex FM modulation-envelope decay used by the next eligible note trigger; do not reshape a decay already in progress. |
| MCX2 | METAL DECAY | Update the Metal trigger-envelope decay used by the next METAL-enabled trigger; no effect on an already-running tail. |
| MCX3 | WAVEFOLDER MODULE VOLUME | Apply to the active audio path with a short click-safe smoothing ramp; do not hard-step gain. |
| MCX4 | LPG DECAY | Update the LPG trigger decay used by the next eligible trigger; do not rewrite an active LPG decay segment. |
| MCX5 | LPG MODULE VOLUME | Apply to the active LPG path with a short click-safe smoothing ramp. Must preserve the v98 coupling between LPG module volume and trigger-envelope gain unless a separate DSP redesign is approved. |
| MCX6 | VOICE VOLUME | Apply to the active voice VCA/output control with a short click-safe smoothing ramp, preserving the reference velocity/accent envelope relationship. |

Automation values become the current extension base at the step where the event occurs and remain active until the next automation event, loop restart, stop, engine change, or project transition, matching the normal Felucca automation concept.

Parameter Lock values apply only when the step actually plays (chance passes), remain active through ratchets on that step, and restore at the next step to the latest automation value up to that point or to the saved MCX patch value if no automation exists.

For time parameters, a lock on a trigger step must be applied before note-trigger processing so the new time is used by that trigger. A lock on a step without a trigger changes the value for later triggers but does not create or restart an envelope.

## 3. Event model

- Keep MCX0..MCX6 as a separate local ID namespace 0..6.
- Preserve the existing four-byte event record shape only if the MCX store is physically separate and independently validated.
- One event per (track, step, MCX ID); duplicate tuples are rejected or deterministically replaced by the editor API.
- Event values are validated against the MCX descriptor's quantized range before storage and again before application.
- Automation and Parameter Lock share the event pool but have distinct event flags/semantics.
- Clear MCX locks, clear track MCX motion, undo/redo, project copy, project clear and archive restore operate only on the MCX extension store.
- Stop, loop restart, engine change, project load and preset load must rebase all seven runtime values to the correct automation or saved base state.

## 4. Runtime and real-time design

Do not scan all MCX events from the audio sample callback or an unbounded interrupt path.

Preferred implementation:
1. Validate and load the bounded MCX event pool outside real-time processing.
2. Build a compact per-track/per-step index or bucket table in RAM.
3. At a sequencer step boundary, inspect only the small list of MCX events attached to that track/step.
4. Maintain per-track automation base values and lock masks, so restoration does not rescan the entire event pool.
5. Publish changed DSP targets through the same safe parameter-update boundary already used by the engine; apply smoothing in the audio engine rather than performing expensive work in the sequencer handler.

The index must have fixed maximum memory and bounded work. Measure its RAM cost against AC791N per-part and global memory budgets. Do not allocate dynamically.

Initial 64-event capacity is a conservative proposal, not a proven performance limit. Benchmark with:
- 64 MCX events plus the maximum normal 128 events;
- all four tracks active;
- worst-case same-step locks and ratchets;
- loop boundary and stop restoration;
- simultaneous editor changes and sequencer playback.

If 64 events are still too expensive, use a smaller cap or a more efficient index.

## 5. UI/editor behavior

- Show the seven MCX parameters in the Metal Core Automation parameter selector only when the selected track engine is ENERGUN METAL CORE.
- Clearly distinguish MCX extension IDs from ordinary P_* parameters in protocol and UI code.
- Show automation and lock markers on the step grid using separate MCX event queries.
- Keep ordinary Felucca automation and MCX automation independent.
- Editor must negotiate the MCX capability/version before displaying or sending MCX commands.
- Unsupported firmware must continue to use the existing editor behavior without unknown command traffic.

## 6. V1 boundaries

Included in the proposal:
- persistence of all seven MCX base values per track;
- automation and Parameter Lock for all seven;
- separate MCX event namespace and editor operations;
- bounded event capacity, initially 64/project, subject to timing/memory benchmark.

Not included:
- MCX destinations in the general modulation matrix;
- arbitrary per-sample modulation of MCX values;
- changing the v98 envelope/DSP algorithms;
- changing the ordinary Felucca motion format or ID width;
- selecting or writing physical Flash sectors;
- target firmware implementation before user's own full V1.5 dump and recovery proof.

## 7. Approval gates

Before this proposal becomes a frozen V1 decision:
1. User approves all-seven MCX automation/Parameter Lock.
2. User approves 64 events/project as the initial target capacity, subject to hardware benchmark.
3. Exact descriptor ranges, units, quantization and defaults are reconciled against the v98 source audit.
4. Runtime index memory and step-time budgets are measured in the host model and target benchmark.
5. Sidecar and archive v2 transaction design includes the MCX event pool and schema migration behavior.

This document is a proposal only. No firmware source, editor source, emulator, project format, parameter ID, backup protocol, Flash address, firmware image, factory firmware, or physical device was changed.
