# ENERGUN FM-1 CORE — CP1 production persistence callsite audit

Date: 2026-11-04
Branch: `work/cp1-production-callsite-audit-20261104`
Source baseline: `2efb03a06fd1d4992581d8a1bd472c13e749c601`
Status: source audit and integration plan only; no storage callsites modified.

## 1. Project slots

Audited `firmware/src/project.c`:

| Function | Source line | Current behavior | Required pair-aware boundary |
|---|---:|---|---|
| `proj_fetch()` | 842 | `st_load(OBJ_PROJECT0 + slot)`, import/normalize Base, repack RAM cache | Load Base+Sidecar pair, validate Base binding, then decode Sidecar into a temporary typed cache |
| `project_save_as()` | 887 | Capture runtime, `proj_pack()`, `st_save(OBJ_PROJECT0 + slot)`, update `proj_slot` | Capture Private cache and commit Base+Sidecar as one pair; update RAM cache only after verified pair commit |
| `project_rename()` | 981 | Load stored Base from RAM cache, change name, `st_save()` | Use `energun_lifecycle_v1_copy_stored()`: preserve stored Sidecar; do not substitute unsaved live Private state |
| `project_erase()` | 1017 | `st_save(object, ..., 0)` tombstone then clear RAM slot | Keep tombstone semantics; only update RAM slot after Base tombstone succeeds; Sidecar may remain orphaned and must not be selected without matching Base |
| `project_restore_runtime()` | 1053 | Validate and apply project fields to runtime | Apply typed Private payload only after Base import, engine validation and runtime restore are accepted; use a prepared cache and atomic commit boundary |
| `autosave_poll()` | 1352 | Quiet-time gate, FM6 reservation guard, capture, then one-object `st_save(OBJ_AUTOSAVE)` | Pair-commit live Base+Sidecar; retain `st_object_reserved(OBJ_AUTOSAVE)` guard and do not update saved signature until pair verification |
| `autosave_boot()` | 1385 | `st_load(OBJ_AUTOSAVE)`, import Base, restore runtime | Load matching live pair first; restore runtime from verified Base and apply Private cache atomically |

The existing quiet-time, transport and FM6 staging guards must remain in force. Pair integration must not move Flash erase/write work into the audio ISR.

## 2. User preset banks

Audited `firmware/src/upreset.c`:

| Function | Source line | Current behavior | Required pair-aware boundary |
|---|---:|---|---|
| `up_boot()` | 290 | Loads two Base bank objects with `st_load(OBJ_UPRESET0 + bank)`, validates native bank | Load each Base+Sidecar pair; check Sidecar bitmap against records accepted by `up_valid()` |
| `up_put()` | 307 | Mutates one record in RAM, writes the whole 16-slot Base bank with `st_save()`, rolls back RAM on failure | Prepare the complete updated Base bank and Sidecar bank, pair-commit, then publish RAM mutation |
| `up_store_cat()` | 399 | Captures engine parameters and pattern into a new record, calls `up_put()` | Capture that part's Private cache for the same slot and include it in the bank-pair transaction |
| `up_load()` | 455 | Loads record values into selected track; current format has no Private Sidecar application | Apply the matching Private slot only after Base record validity and sound/engine load acceptance; absent slot applies canonical defaults |

The native Base layout is `up_bank_t`: 8-byte header plus 16 records × 192 bytes = 3,080 bytes. The Sidecar payload is the separately defined 210-byte 16-slot bitmap + Private values codec. Do not change the Base record ABI to make Sidecar fit.

## 3. Existing adapter boundary

The existing backend-neutral adapters already expose the needed operations:

- `energun_lifecycle_v1_save_project/load_project`
- `energun_lifecycle_v1_copy_stored`
- `energun_lifecycle_v1_erase`
- `energun_lifecycle_v1_autosave`
- `energun_preset_lifecycle_v1_save_bank/load_bank_checked`
- `energun_preset_lifecycle_v1_capture_slot/erase_slot/apply_slot`

They require injected pair storage callbacks, scratch buffers and a write guard. They do not provide a physical Sidecar storage backend or a target Flash map. The production callsites above do not currently invoke these adapters.

## 4. Safe implementation sequence

1. Approve a physical Sidecar storage map and target backend contract. No addresses or storage IDs are inferred by this audit.
2. Implement and test the backend callbacks against the approved map, including A/B selection, Base-generation binding, read-back and byte-cut recovery.
3. Integrate project save/load/rename/erase and live autosave one callsite at a time. Keep the current Base-only V1 fallback and canonical defaults for legacy objects.
4. Integrate preset bank boot/store/rename/erase/load using whole-bank pair transactions and checked slot bitmap matching.
5. Add production-path host tests that compile the real `project.c` and `upreset.c` paths with the test backend, then run exact-head Fast Regression and Full Baseline.
6. Only after review and explicit approval, enable the target backend on device.

## 5. Explicit blockers and non-claims

- No approved physical Sidecar Flash allocation/map is present in the source baseline.
- No production pair backend is wired to `project.c`, `autosave_poll()` or `upreset.c`.
- The existing host lifecycle adapters and simulated-NOR tests do not prove physical Flash behavior.
- This audit does not modify Factory V15, `main`, storage object IDs, physical Flash addresses, or device firmware.
- CP1 production integration remains open; this document is not a closure claim.
