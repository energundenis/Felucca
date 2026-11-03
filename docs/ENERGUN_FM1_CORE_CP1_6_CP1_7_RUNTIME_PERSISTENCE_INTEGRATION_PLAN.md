# ENERGUN FM-1 CORE — CP1.6 / CP1.7 runtime persistence integration plan

Date: 2026-11-03  
Repository: `energundenis/Felucca`  
Branch: `energun-development-orchestrator`  
Status: implementation contract based on the frozen CP1.17 Sidecar V1 scope; production callsites are not yet wired.

## 1. Current source facts

- Current Felucca parameter layout has `P_COUNT=111`, `P_E0=103`; the last eight stored values are E0..E7.
- `params_by_count()` maps older records by preserving the first `np-8` common parameters and moving the final eight values to the current E0..E7 slots.
- FUN10 is a fixed 3840-byte object. Its current source comments identify only 11 additional per-track common parameter slots before the object limit. Expanding the serialized Base record by all sixteen P_MC values would exceed the existing object layout.
- User-preset records have a 144-byte packed parameter array and a fixed 192-byte record. The current `UP_PMAX=72` provides 144 value bytes; a 127-parameter runtime map fits this capacity.
- Motion events use a 7-bit parameter ID and reserve bit 7 for the Motion Lock flag. The shared pool is limited to 128 events.
- Existing project and preset storage formats are legacy contracts. They must not be silently resized or reinterpreted.

## 2. Parameter namespace reservation for compatibility review

The logical ordering required by the accepted 8/16/13 map is:

| Logical range | Candidate numeric range | Owner |
|---|---:|---|
| Existing common Felucca parameters | 0–102 | Base host |
| P_MC common parameters | 103–118 | Metal Core / Sidecar for projects |
| Engine EDIT E0–E7 | 119–126 | Metal Core engine / Base project |
| P_COUNT | 127 | Global bound |

This is a **candidate append-before-E0 allocation**, not yet a committed firmware ABI. It is the only contiguous 16-ID slot that preserves the existing 7-bit ID limit and the established “new common parameters immediately before E0” migration rule. Before changing `core.h`, `params.c`, editor pages or stable protocol IDs, CP1.4.2 must run exact legacy project/preset migration fixtures and verify the FUN10 Base-size strategy below.

P_MC order is exactly the sixteen-control order in `ENERGUN_METAL_CORE_CP1_4_1_FROZEN_PARAMETER_MAP.md`, section 4. Private values remain named logical keys in the frozen section 5 and do not consume global P_* IDs.

## 3. CP1.6 — project runtime integration contract

### 3.1 Preserve Base FUN10 bytes

The runtime parameter array may contain 127 values, but the Base FUN10 object must continue to serialize the legacy 111-value projection:

- first 103 existing common parameters, unchanged and in their existing order;
- E0–E7, taken from the runtime indices after the inserted P_MC block;
- do not serialize P_MC values into FUN10;
- write the legacy Base parameter count (111) in the Base record;
- keep the 3840-byte FUN10 object, step records, song chain, Base Motion block, FM6 patches and CRC behavior unchanged.

On load, the existing count-based mapper restores the 103 legacy common values and E0–E7. P_MC receives deterministic descriptor defaults first, then a matching Sidecar overlays the four tracks' P_MC values. A missing, invalid or stale Sidecar never blocks a valid Base project.

### 3.2 Motion split

- Base FUN10 retains all existing Base-owned Motion events, including E0–E7 and existing supported host controls.
- Sidecar V1 stores only P_MC Motion Lock events (logical index 0–15, lock flag in bit 7).
- Private and MCX values are excluded from V1 Motion.
- Save snapshots split the in-memory Motion set into Base-owned and P_MC-owned collections without mutating the live set.
- Load merges only a fully validated, exact-Base-generation P_MC Sidecar set with Base Motion; reject duplicate (track/step, parameter) pairs and any combined count over 128.
- The same P_MC conversion descriptors must be used by editor, runtime, save/load and Motion Lock.

### 3.3 Pair transaction and callsites

For every project identity (four stored slots plus live autosave), the pair transaction must cover:

1. normal project save;
2. live autosave snapshot and boot recovery;
3. stored project load into live state;
4. project copy/duplicate/import;
5. project rename where identity/generation changes;
6. project clear/default creation;
7. restore from a versioned archive.

The Sidecar must be written and read back before the matching Base generation is committed. Never overwrite the currently selected valid Sidecar before its matching Base commit. Sidecar binding requires the exact Base storage sequence and payload CRC, not merely the project slot number.

### 3.4 Runtime safety and staged enablement

- First implement and test a host-only serialization/pair adapter against simulated NOR and fault injection.
- Then compile the runtime callsites and prove that the legacy Base projection remains byte-for-byte compatible for old projects.
- Keep candidate physical Sidecar sectors protected. Do not call hardware Sidecar init or enable erase/program paths until the complete user FM-1 V1.5 dump and recovery audit approve a physical map.
- Preserve FM6 staging arbitration. Sidecar pair saves and live autosave snapshots must defer under the same lease/quiet contract.
- No load path may rewrite storage. Missing/corrupt Sidecar fallback is read-only.
- No factory firmware change, physical Flash write, device installation or merge to `main`.

## 4. CP1.7 — user-preset runtime integration contract

### 4.1 Base record and Sidecar ownership

- Keep the existing 192-byte user-preset record and two-bank storage layout unchanged.
- The common P_* parameter record continues to hold the 127-value runtime projection; `UP_PMAX=72` leaves 17 bytes of packed capacity beyond 127 values.
- P_MC common values therefore remain in the existing preset record's parameter array and use the canonical CP1.4.2 conversions.
- Sidecar preset-bank payload carries thirteen Private values for each of sixteen slots (210 bytes total including the two-byte presence bitmap).
- Two logical preset-bank identities cover all 32 user slots. Each bank has its own A/B Sidecar copies.
- Absent/deleted slots must have zero Private payload bytes; stale Private data cannot attach to a reused slot.

### 4.2 Preset operations

The paired preset implementation must cover:

1. store/overwrite;
2. load to the selected part;
3. delete and slot reuse;
4. rename/category-only update;
5. bank load at boot;
6. FM6 patch sidecar interactions;
7. backup/archive restore.

Preset load applies common parameters from the legacy user-preset record and the thirteen Private values from the matching preset Sidecar. If the optional Private Sidecar is absent or invalid, use frozen defaults and keep the preset playable.

The current user-preset binary record remains unchanged. Private state is not placed in volatile engine memory and is not inferred from an engine ID or a previous slot occupant.

### 4.3 Transaction and failure behavior

- Update the inactive preset Sidecar copy and verify it before committing the matching preset-bank Base object.
- On failure, restore the in-RAM bank/slot snapshot and report failure; do not leave a mixed Base/Private preset.
- Delete/slot reuse must atomically clear the corresponding Private values.
- Rename/category changes that do not change sound data may preserve the current Sidecar binding only if the Base generation contract explicitly permits it; otherwise rebind through a new pair commit.
- Archive V1 remains legacy-only. Archive V2 must validate all Base and Sidecar objects before any destructive restore write and default extension data for V1 archives.

## 5. Required CP1.6 / CP1.7 acceptance tests

### Project tests
- Old FUN10 fixture loads with exact legacy common values and E0–E7; all sixteen P_MC values start at defaults.
- New Base projection remains exactly 3840 bytes and retains legacy parameter count 111.
- New P_MC/Private state round-trips across all four tracks and live autosave.
- All five project identities remain isolated.
- Base/Sidecar sequence and CRC mismatch is rejected; valid Base remains loadable with defaults.
- Copy/import/rename/clear cannot attach another project's Sidecar.
- Every Sidecar erase/program/header/readback cut point recovers either the old valid pair or the new valid pair.
- Base and Sidecar Motion split/merge preserves E0–E7 Base events and only P_MC Sidecar events; combined event count remains <=128.

### Preset tests
- Legacy preset versions 1–5 load with existing count-based mapping and default Private values.
- New 127-parameter records fit the existing 144-byte parameter area and keep the 192-byte record size.
- All 32 preset slots round-trip common values and independent Private values.
- Delete/reuse clears Private state; rename/category update does not cross-contaminate slots.
- Missing/corrupt Private Sidecar leaves the Base preset playable with defaults.
- Interrupted paired save preserves the old valid preset or the new valid preset, never a mixed pair.

### Shared regression
- CP1.4.2 quantization reference and exact project/preset legacy fixtures.
- Sidecar V1 semantic validation and A/B selection.
- Pair coordinator and simulated-NOR fault injection.
- FM6 staging lease and autosave exclusion.
- Full host regression and exact-head AC79 target compile/link evidence.

## 6. Immediate execution order

1. Complete CP1.4.2 compatibility review and approve/reserve the P_MC numeric range before changing production parameter enums.
2. Add host fixtures for the legacy 111-parameter FUN10 projection and the 192-byte preset record.
3. Implement the project Base projection/split/merge adapter in host-tested code.
4. Bind project save/load/copy/rename/clear/autosave callsites to the pair coordinator, initially behind a disabled physical-backend gate.
5. Implement the two preset-bank Private sidecar adapter and test all 32 slots.
6. Add archive-v2 pair-aware backup/restore after project and preset roundtrips pass.

**Status:** CP1.17 logical ownership is closed. CP1.6 and CP1.7 are OPEN; no runtime callsite has been wired by this plan.


## 7. Host-only implementation checkpoint — 2026-11-03

### CP1.6 Base projection / Motion adapter

Added:
- `firmware/src/energun_project_base_projection.h`
- `firmware/src/energun_project_base_projection.c`
- `tests/project_base_projection_test.c`
- `tests/run_project_base_projection_test.sh`

The helper is deliberately not yet called by `project.c`; it is a pure host-testable boundary contract.

Verified on exact SHA `6d3a738389d70c3c89a1d3285903bc1bb7c40f01`, CP1 worker evidence in GitHub Actions run [41668891585](https://github.com/energundenis/Felucca/actions/runs/41668891585):

- CP1.6 Base projection: **17 checks, 0 failures**.
- The Base projection is exactly 111 values: 103 legacy common parameters + E0–E7.
- P_MC changes do not alter the Base FUN10 projection.
- Legacy Base load restores common values and E0–E7 and leaves P_MC at deterministic defaults until Sidecar overlay.
- Base Motion and P_MC Motion split/merge preserves lock flags, rejects Private index 16 and P_COUNT sentinel 127, rejects duplicate place/parameter pairs, and enforces the shared 128-event limit.
- Pack/unpack reject invalid values atomically without modifying output/runtime buffers.

### CP1.7 preset schema coverage

Expanded `tests/energun_sidecar_v1_test.c` to exercise both preset-bank identities and all 32 user-preset slots:

- all sixteen Private records validate in bank 0 and bank 1;
- delete clears the selected slot's thirteen Private bytes;
- slot reuse does not change the other fifteen records;
- absent slots cannot retain stale Private values.

The same exact-SHA CP1 worker run reports Sidecar V1 **46 checks, 0 failures**. This verifies the V1 preset payload schema, not production `upreset.c` callsite integration.

### Current disposition

- CP1.17 logical schema/ownership: CLOSED.
- CP2/CP3 parameter ownership and persistence-boundary audit: recorded; P_MC IDs 103–118 remain reserved candidates, not yet production ABI.
- CP1.6 host Base projection/Motion adapter: PASS; production project/autosave/copy/rename/clear callsites NOT YET WIRED.
- CP1.7 preset Sidecar schema: PASS; production preset save/load/delete/rename callsites NOT YET WIRED.
- Pair commit remains blocked until runtime callsites, complete target firmware link evidence and exhaustive backend power-cut acceptance are complete.
- No physical Sidecar sector allocation, hardware Sidecar init, Flash erase/program, device write, factory firmware modification or installation occurred.
