# ENERGUN FM-1 CORE — CP1 reconciliation checkpoint

Date: 2026-11-01
Coordination branch: `energun-metal-core`
Status: documentation-only reconciliation; CP1 remains OPEN.

## 1. Purpose

This checkpoint reconciles the approved V1 scope, later CP1 decisions, the 37-control inventory, parameter-access proposals, VCA source specification, storage audits, and the current CP3 readiness documents. It does not assign parameter IDs, change firmware, allocate Flash, or authorize device access.

## 2. Decisions that are already approved / resolved

- V1 objective: port the main ENERGUN METAL CORE v98 sound-generation path into Felucca while preserving its signal architecture.
- V1 signal scope: OSC1, independent direct-audio OSC2, Complex FM/AM modulator, separate METAL FM and METAL Ring Mod, independent FOLLOW and METAL switches, two-stage wavefolder, main low-pass filter, LPG, and voice amplitude behavior.
- FOLLOW and METAL remain independent controls in OSCILLATORS / SYNTH VOICE.
- MOD RATIO (3.07 default) and METAL RATIO (4.20 default) remain distinct.
- FM CHARACTER default is METALLIC (value 2), established by source inspection.
- LPG MODULE VOLUME follows the user-approved Option C decision recorded in CP1.12.
- WEST COAST VOLUME reference steady-state scale is ×5 with one deterministic writer.
- VCA ownership is engine-owned and V98-compatible: `ownenv=1`; host ADSR is not cascaded as a second amplitude multiplier.
- Short Gate / long Attack follows Option B: Release starts at Gate-off from the instantaneous level and suppresses any later Decay event.
- The pinned Felucca-Engines API supports the selected own-envelope / `done()` lifecycle contract.

These decisions resolve specific questions only; they do not close CP1 as a whole.

## 3. Source-backed inventory and unresolved parameter access

- CP1.3 contains a source-backed inventory of 37 independent V1 sound controls.
- CP1.4 and CP1.12 propose an access split of 8 engine edit values + 16 common parameters + 13 private sidecar values.
- That 8/16/13 split is explicitly a proposal, not an approved or implemented parameter map.
- The proposal consumes all 16 available common parameter IDs below `P_E0`; it leaves no spare IDs in the current 7-bit parameter namespace.
- No stable new parameter IDs have been assigned. The current parameter/preset/motion/editor migration behavior is not implemented.
- Host ADSR reuse is not permitted by the later VCA ownership decision; the VCA source controls remain engine-owned sound state.

Required before parameter-map freeze:
1. Approve or revise the 8/16/13 access allocation.
2. Decide the exact user-facing placement/edit path for private enum/switch values.
3. Define which continuous values are eligible for motion locks and which are deliberately excluded.
4. Verify project, preset, editor, web backup and restore migration as one compatibility unit.

## 4. Sidecar and storage reconciliation

- FUN10 remains fixed at 3840 bytes; its existing layout cannot accept the full new parameter set.
- The later sidecar transaction audit supersedes the earlier eight-sector estimate: five persisted project identities (four stored projects plus live autosave), each with A/B copies, require ten sidecar sectors.
- The current candidate capacity illustration is `0xE9000–0xF2FFF` (end-exclusive `0xF3000`), with the final 256 bytes of each sector reserved/erased for update-loader safety.
- This address range is NOT approved for allocation. The user's complete FM-1 V1.5 dump is required before any sector is selected.
- The sidecar must be optional for loading the base project, paired to the exact main-project generation, power-loss safe, and included in backup/restore.
- The addendum requires P_MC motion events to travel with the sidecar if those new IDs are used. The transaction audit separately leaves the MCX0..MCX6 extension-motion feature as an explicit scope decision. These namespaces must not be conflated.

## 5. VCA implementation evidence

- CP1.15 records the source equations and pinned host API semantics.
- CP1.16 resolves the short-Gate event-order rule.
- The isolated VCA module and VCA-only integration harness exist.
- The VCA-only harness passed on its recorded commit and CI run (see CP3 integration readiness plan); this does not prove full engine lifecycle integration or target runtime cost.
- Remaining VCA work includes quantified error against the high-precision reference, explicit manual/live versus sequencer trigger mapping, and full host lifecycle coverage.

## 6. Current integration / CI boundaries

- DSP PR #1 remains Draft and unmerged. Its exact tested candidate SHA passed focused host regression and an AC79 compile-only probe. The target probe wrapper still requires source/disassembly review before claiming production-path code generation or runtime performance.
- Host Integration PR #2 remains Draft and unmerged. Its test-only lifecycle contract matrix and source trace exist, but the matrix does not execute the real static allocator/event dispatcher. Exact-head CI evidence is still pending.
- Platform QA remains a separate workstream and is not complete.
- No production METAL CORE engine is registered/playable.
- No installable ENERGUN FM-1 package is established.

## 7. CP1 exit criteria

CP1 can be closed only when all of the following are recorded:
1. The V1 37-control access map is approved and stable.
2. The VCA note-source mapping for manual/live MIDI versus sequencer triggers is explicit.
3. Signal taps, gain staging and all module-level volume semantics are frozen against the reference.
4. Parameter, motion, project, preset, web archive and backup/restore migration are specified consistently.
5. Sidecar record format and host/emulator transaction tests are complete without selecting physical sectors.
6. Append-only engine-ID compatibility is verified across optional build configurations.
7. All unresolved V1/deferred feature boundaries are explicit.

## 8. Next execution order

1. Complete the parameter-access decision package from the existing 37-control inventory; do not assign IDs yet.
2. Finish the VCA trigger-mode mapping and reference-accuracy tests.
3. Reconcile P_MC motion events with the sidecar and archive-v2 design; keep MCX extension motion separate unless explicitly approved.
4. Complete host/emulator storage transaction tests without touching device Flash.
5. Close the actual host allocator/lifecycle test gap in CP3 in parallel, but keep production engine registration gated.
6. Only after these gates: freeze CP1, then integrate qualified DSP incrementally and rerun exact-HEAD CI.

## 9. Safety

- `main` is not modified.
- No PR is merged by this checkpoint.
- No engine ID or parameter ID is allocated.
- No Flash sector is allocated or written.
- No physical FM-1 access, firmware install or factory-firmware change is performed.
