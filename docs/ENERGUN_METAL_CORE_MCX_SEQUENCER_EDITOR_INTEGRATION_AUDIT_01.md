# ENERGUN METAL CORE — MCX sequencer and editor protocol integration audit 01

Date: 2026-10-31
Branch: energun-metal-core
Status: source-backed audit and design constraints; documentation only. No source changed.

## 1. Source inspected

- `firmware/src/motion.c`
- `firmware/src/seq.c`
- `firmware/src/editor.c`
- `web/EDITOR_PROTOCOL.md`
- `tests/editor_test.c`
- `tests/target_budget.py`

The firmware repository's browser editor implementation files were not located in the inspected paths. The protocol contract is documented, but frontend parser and UI call sites still require a direct source audit before assigning command IDs or implementing UI support.

## 2. Sequencer ordering — confirmed

The current `seq_play()` flow is:

1. Determine whether the step misses due to condition/chance.
2. Call normal `motion_step(t, idx, m, skip == SEQ_ROLLED)`.
3. Process live-recording skip state.
4. Call `seq_step()`, which performs note triggers and ratchet handling.

Consequences for MCX:
- MCX step processing belongs beside normal `motion_step()` in `seq_play()`, before `seq_step()`.
- The same rolled/missed result must be passed to MCX so Parameter Locks apply only when the step passes chance/condition.
- Automation events should apply at their step even if the note step misses, matching normal motion behavior.
- A Parameter Lock must be active before note-on so FM/LPG/METAL envelope times affect that trigger.
- The lock must remain through the full step, including ratchets, and restore at the next step.
- Nudge and early/late step playback call `seq_play()` at the actual step-play time; MCX should follow that same timing, not apply prematurely at the step's nominal boundary.
- Stop currently calls `motion_end()`; MCX requires a corresponding stop/rebase path.
- Loop step zero calls `motion_restore()` inside normal motion. MCX must restore/rebase at the same logical loop boundary.

Do not add MCX processing to the per-sample render loop. The sequencer callback is already in the audio ISR, so its additional work must be strictly bounded.

## 3. Existing motion path — confirmed constraints

Normal motion is a shared `MOTION_MAX=128` event pool. Each event is four bytes. The audio ISR scans the event list in `motion_step()`. Parameter Lock restoration can scan the event list again for each currently locked parameter in `motion_unlock()`.

The normal event namespace uses a 7-bit P_* ID plus `MOTION_LOCK` in bit 7. All normal validation, descriptors, UI masks and storage assume IDs are below `P_COUNT`.

Therefore:
- MCX IDs must remain in an independent namespace and store.
- Never route MCX IDs through `motion_param()`, `param_desc_of()`, `track_t.p[]`, `motion_mask()`, or the ordinary `MOTION` command.
- Do not raise P_COUNT or consume IDs 103..126 for MCX motion.
- A second linear 128-event scan plus lock-restore scans is not acceptable without a measured ISR budget.

## 4. Recommended bounded runtime design

Use a separate project-wide MCX event pool, initially capped at 64 records pending benchmark, and build a fixed RAM index outside the audio ISR.

Preferred index:
- For each track and step, a compact head/count pair or fixed bucket range identifies the MCX events for that exact step.
- The maximum number of MCX events processed at one step is bounded by the descriptor count (seven).
- Keep per-track automation base values and active/locked bitmasks for seven MCX IDs.
- At a step, process only the events indexed for that track/step.
- On lock restoration, restore from the tracked automation base/current value rather than scanning the full event pool.
- Rebuild the index after project load, sidecar validation, editor edits, clear/copy/import and archive restore, outside the audio ISR.
- Publish index/store changes atomically under the same interrupt guard discipline as normal motion; never let the ISR see a partially rebuilt index.

The precise index layout and RAM footprint must be measured against the AC791N memory budget. The 64-event cap is a proposal, not a proven timing guarantee.

## 5. Existing editor protocol — confirmed

The SysEx framing is:
`F0 7D 46 4C <cmd> <args...> F7`.

Current commands include:
- 1..32: INFO, parameter access, steps, track access and live sync;
- 64: MOTION, including ordinary automation and Parameter Lock;
- 65..67: BACKUP_LIST / BACKUP_GET / BACKUP_PUT;
- 68..71: FM6 bank;
- 72..73: MENU_DESC / MENU_SET;
- 74..77: FM6 voice-bank functions;
- 78..79: MIDI Learn;
- 80..81: palette functions.

INFO is extensible through tagged capability blocks. Existing tags include motion (0x4D), backup (0x42), and other UI/device capabilities. A new MCX capability must use a newly audited, unique tag and explicit schema version; do not reuse 0x4D or 0x42.

The current `ED_MOTION` argument validator accepts a specific set of fixed lengths and operations. Its handler edits only the normal motion store and calls the existing UI force/undo path. Unknown command handling and editor parsing must be checked before selecting new command numbers.

Do not overload commands 64..67. Do not silently change the meaning or length of existing MOTION or backup messages. Archive v2 needs explicit version negotiation and must leave archive v1 intact.

## 6. UI and undo requirements

The normal editor's selected-track parameter shadow tracks only `TSEL->p[]` and globals. MCX values live outside that array and therefore need their own:
- selected-track MCX shadow;
- MCX_CHANGED push or a clearly specified refresh strategy;
- MCX_RELOAD behavior on selected track, engine, preset, project and sidecar load changes;
- selected-track and engine availability checks;
- descriptor query with min/max/default/step/unit/enum metadata;
- automation selector entries that are visible only for the Metal Core engine;
- event-list and step-grid marker queries for MCX automation and locks.

Undo/redo must snapshot MCX base values and MCX event changes independently from normal track parameters and normal motion. Existing `load_begin/load_end` and `UNDO_PAT` behavior must be audited before reuse; do not assume the existing pattern snapshot includes MCX state.

The editor implementation and its parser were not identified among the checked paths. Before implementation, locate the actual browser editor source and tests, then audit:
- tagged INFO parsing and unknown-tag behavior;
- command dispatch and fixed-length validation;
- request/reply matching;
- selected-track/engine state and UI lifecycle;
- undo/redo and event-grid rendering;
- backup v1/v2 negotiation.

## 7. Target test plan additions

Extend host tests before target implementation:
1. MCX automation applies before note_on at the actual nudged step time.
2. MCX Parameter Lock applies only after chance and condition pass.
3. Failed steps still apply automation but never apply a lock.
4. Ratchets use the locked envelope-time value for all retriggers within that step.
5. Next-step restoration returns to latest automation value or saved MCX base.
6. Loop step zero, stop, restart, engine change, project load and preset load restore/rebase correctly.
7. Four tracks remain isolated.
8. MCX index rebuild is atomic relative to the audio ISR.
9. Maximum 64-event pool plus maximum normal 128-event pool remains bounded.
10. Worst-case same-step events, all four tracks, locks, ratchets and editor updates meet the target timing budget.
11. Old editor ignores the new capability and receives no unsolicited MCX command traffic unless it opts in.
12. New editor works with old firmware by hiding MCX controls when capability is absent.
13. Malformed MCX commands, IDs, values, lengths and counts are rejected without state changes.
14. Undo/redo, project copy/clear/import, archive v1 restore and archive v2 restore preserve extension isolation.

Use existing host-side test infrastructure and target static-budget tooling as inputs, but add a dedicated measured MCX step-cost benchmark. Static instruction counts alone do not prove sequencer timing margin.

## 8. Decisions still not frozen

- MCX automation/Parameter Lock as a V1 feature remains a proposal pending explicit user approval.
- 64 MCX events/project remains a provisional cap pending memory and timing measurements.
- Command numbers and the new INFO capability tag are deliberately not assigned.
- Browser editor source/parser audit remains incomplete.
- No Sidecar sector allocation is approved.

## 9. Safety boundary

This audit is documentation only. No firmware source, editor source, emulator, command number, parameter ID, project format, backup format, Flash address, firmware image, factory firmware, or physical device was changed.
