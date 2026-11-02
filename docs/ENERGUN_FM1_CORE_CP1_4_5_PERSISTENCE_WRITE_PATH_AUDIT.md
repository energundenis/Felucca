# ENERGUN FM-1 CORE — CP1.4.5 Sidecar persistence write-path audit

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **SOURCE PATH AUDIT COMPLETE — NO PRODUCTION PAIR COORDINATOR, NO ABI, NO FLASH ALLOCATION**

## 1. Purpose and boundary

This audit maps the proposed Sidecar pair-transaction contract onto the current Felucca persistence call sites. It is intended to prevent a future implementation from wrapping only UI entry points while leaving lower-level save, autosave, preset, or archive paths able to bypass pairing.

This is a source audit and implementation-boundary document only. It does not change firmware source, add storage object IDs, assign sectors, freeze a binary Sidecar ABI, or authorize a device operation.

Audited source at branch `energun-metal-core`:
- `firmware/src/storage.c` — A/B storage primitives.
- `firmware/src/project.c` — project slots, project rename/erase, runtime restore and live autosave.
- `firmware/src/upreset.c` — user-preset banks and preset save/rename/erase/load.
- `firmware/src/editor_backup.c` — archive V1 capture and restore.
- `firmware/src/editor.c` — editor command dispatch to the above paths.

## 2. Current storage-layer facts

The current storage API is implemented as static functions in `storage.c`:
- `st_head(obj, copy, h)` validates the 32-byte object header.
- `st_body(obj, copy, h)` reads the payload and validates its CRC-32.
- `st_current(obj, h)` selects the newest locally valid A/B copy, with A winning a sequence tie.
- `st_load(obj, dst, max)` returns only the payload length and bytes; it does not expose the selected copy or committed sequence to the caller.
- `st_save_to(obj, src, len, to)` derives the next local sequence from `st_current()`, commits payload before header, then reads back and verifies.
- `st_save(obj, src, len)` writes the inactive copy.

Consequences for Sidecar:
1. The future pair coordinator needs a read result that includes the selected base object's committed sequence and copy identity, not only its payload bytes.
2. The future writer needs a serialized way to determine the exact next base sequence and hold that decision until the base commit completes.
3. Sidecar selection must inspect both Sidecar copies against the selected base sequence. Calling `st_current(sidecar)` first is insufficient.
4. Since the current functions are static and expose no sequence-aware pair API, this is a storage-layer design/integration task, not something that can safely be implemented solely in the project UI.

## 3. Project and live-autosave write paths

| Operation | Current source path | Current persistence call | Required future behavior |
|---|---|---|---|
| Load project slot | `proj_fetch()` in `project.c` | `st_load(OBJ_PROJECT0 + slot, ...)` | Load base and selected sequence; then load only matching Sidecar; defaults on missing/invalid extension |
| Save / Save As | `project_save_as()` → `project_save()` | `st_save(OBJ_PROJECT0 + slot, ...)` | Snapshot complete extension; Sidecar-first commit tagged with next base sequence; commit base; verify pair |
| Rename stored project | `project_rename()` | `st_save(OBJ_PROJECT0 + slot, ...)` | Rebind unchanged complete extension to next base sequence |
| Erase project slot | `project_erase()` | `st_save(OBJ_PROJECT0 + slot, ..., 0)` | Commit explicit empty/tombstone extension bound to the new empty base sequence, or a reviewed invalidation protocol |
| Live autosave | `autosave_poll()` | `st_save(OBJ_AUTOSAVE, ...)` | Snapshot/rebind the complete live extension as part of the same serialized pair transaction |
| Restore live autosave | `autosave_boot()` | `st_load(OBJ_AUTOSAVE, ...)` | Load only the Sidecar matching the selected autosave base sequence before applying runtime state |

Important source details:
- `project_capture()` captures the legacy project payload and motion under `motion_guard()`; the future extension snapshot must be captured consistently with the same musical state boundary.
- `project_save_as()` and `project_rename()` both rewrite the base object, even when extension values have not changed. Both therefore require Sidecar rebinding.
- `project_erase()` commits a zero-length base payload. The matching extension must become explicitly absent for that new base generation; an older Sidecar must not reappear if the slot is reused.
- `autosave_poll()` is a separate base-object writer and cannot be omitted from pair coordination. Its quiet-time and write-throttling policy should remain intact.

## 4. User-preset bank write and load paths

| Operation | Current source path | Current persistence call | Required future behavior |
|---|---|---|---|
| Boot bank load | `up_boot()` | `st_load(OBJ_UPRESET0 + bank, ...)` | Load bank sequence and matching bank Sidecar; absent slots must resolve to defaults |
| Save slot | `up_store_cat()` → `up_put()` | `st_save(OBJ_UPRESET0 + bank, ...)` | Snapshot all 16 slot extensions, update selected slot, rebind complete bank extension |
| Rename/category update | `up_rename_cat()` → `up_put()` | Same bank `st_save()` | Preserve all other slot extensions and rebind whole bank |
| Erase slot | editor path → `up_put(k, NULL)` | Same bank `st_save()` | Clear presence and values for that slot; preserve other slots; rebind whole bank |
| Load preset to selected part | `up_load()` | Runtime load from bank mirror | Restore only the selected part's Private values from the matching bank/slot record; missing slot extension uses defaults |

The bank is one storage object containing 16 records. The Sidecar identity is therefore bank-scoped, not slot-scoped. Any slot mutation changes the bank base sequence and requires a complete bank-extension snapshot/rebind.

Separate FM6 patch persistence remains outside this Private Sidecar transaction. `up_store_cat()` commits the preset bank through `up_put()` and then calls `upf_store()` for the FM6 patch. Do not claim atomicity across the preset bank, Private Sidecar and FM6 patch object without a separate transaction design.

## 5. Archive V1 bypasses and restore requirements

The current `editor_backup.c` archive is a fixed object list:
- `ed_bk_object()` exposes project slots, two user-preset banks, settings and FM6 patch objects under fixed IDs.
- `ED_BACKUP_LIST/GET` exports only those enumerated objects and their transport CRC.
- `ed_bk_commit()` validates and restores a project slot or preset bank by calling `st_save(obj, raw, len)` directly.
- The runtime project restore ID is applied through `project_restore_runtime()`; it does not represent a stored project-slot commit.
- Archive V1 has no Sidecar identity entries and no pair-aware import protocol.

Consequences:
1. A future pair coordinator cannot be considered complete while archive V2 still calls `st_save()` directly for paired base objects.
2. Archive restore must validate the incoming base and Sidecar as one logical pair, then rebind the Sidecar to the destination base sequence before the destination base becomes active.
3. Restore interruption must leave either the prior destination pair or the fully restored new pair selectable. A base-only restore followed by a later Sidecar restore is not acceptable.
4. Archive V1 behavior must remain compatible. Old archives that lack Sidecar data must restore the base with deterministic Private/P_MC extension defaults; they must not attach whatever Sidecar happens to be left in the destination slot.
5. The archive transport CRC is not a replacement for the storage-layer CRC, Sidecar semantic validation or base-sequence binding.

## 6. Cross-path coordinator contract

The eventual production coordinator must own the lowest common persistence boundary for each paired identity. A UI-only wrapper is insufficient.

Required responsibilities:
- Serialize every writer for a given base identity, including normal save, rename, erase, autosave and archive restore.
- Capture a generation-stamped, complete Sidecar snapshot before starting Flash writes.
- Prevent another writer from changing the base sequence between next-sequence calculation, Sidecar commit and base commit.
- Preserve the existing storage A/B commit behavior and verify both committed objects after write.
- On load, inspect both Sidecar copies and select the newest valid candidate whose base-sequence binding exactly matches the selected base object.
- Reject malformed or unsupported Sidecars as a whole; never partially apply their values or motion records.
- Clear dirty state only when the persisted snapshot generation still matches the current runtime generation.
- Keep Flash operations out of the audio ISR and preserve existing transport/quiet-time constraints.
- Return explicit outcomes for base failure, Sidecar failure, stale snapshot, invalid extension and successful pair commit; callers must not display SAVED after only one half commits.

Recommended logical transaction outcomes (symbolic, not numeric ABI):
- `PAIR_OK`
- `PAIR_BASE_INVALID`
- `PAIR_SIDECAR_INVALID`
- `PAIR_STALE_SNAPSHOT`
- `PAIR_SIDECAR_WRITE_FAILED`
- `PAIR_BASE_WRITE_FAILED`
- `PAIR_VERIFY_FAILED`

## 7. Required host acceptance matrix

The production implementation must exercise the real storage and coordinator code, not a copied behavioral model.

### Project slots and autosave
- Save As into an empty slot, load, and round-trip all four parts' 16 P_MC and 13 Private values.
- Save over an existing slot with changed extension values; verify the new pair is selected.
- Rename a project; verify the complete extension survives and is rebound.
- Erase a project; verify the old extension cannot reappear after slot reuse.
- Interrupt every Sidecar payload/header and base payload/header write boundary.
- Interrupt between Sidecar commit and base commit; old base must still select old Sidecar.
- Commit the new base; it must select only the new matching Sidecar.
- Exercise autosave while ordinary project saves are pending; prove writes are serialized and snapshots do not cross generations.
- Restore autosave with matching, missing, stale and corrupt Sidecar copies.

### User-preset banks
- Save, load, rename and erase each of the 32 slots across both banks.
- Verify one slot mutation preserves all other 15 slot extensions in that bank.
- Erase and reuse a slot; prove stale Private values do not reappear.
- Exercise empty-bank and final-slot tombstone behavior.
- Verify the separate FM6 patch save ordering remains unchanged and is not misreported as part of the Sidecar pair.

### Archive V2
- Export and restore all four projects, live autosave and both preset banks with their Sidecars.
- Restore to a different destination base sequence; verify destination-sequence rebinding.
- Interrupt restore before/after Sidecar and base commits; destination remains old-valid or new-valid, never mixed.
- Restore V1 archive with no Sidecars; base data loads and extension defaults are applied.
- Reject duplicate, missing-required, wrong-identity, stale-generation, malformed and CRC-invalid archive records before committing a partial pair.

## 8. Scope and progress boundary

This audit closes only the source-path inventory for CP1.4.5/CP1.4.7 planning.

It does not:
- implement the sequence-aware storage reader/writer;
- implement a transaction lock or coordinator;
- freeze Sidecar field widths, stable IDs or binary ABI;
- add storage object IDs or change `OBJ_COUNT`;
- select or erase physical Flash sectors;
- change archive V1 or implement archive V2;
- change factory firmware or access the physical FM-1.

**CP1.4.5 remains a logical design draft. CP1.4.7 remains open. CP1.4.8 implementation and CP1.4.11 physical Flash audit remain separate gates.**

## 9. Critical A/B correction — preserve the current matching copy

A deeper failure-mode review found that the phrase “write the inactive Sidecar copy” is ambiguous and unsafe if it means “the copy opposite the newest Sidecar-local sequence.”

Example:
- Current base sequence is 5.
- Sidecar A has local sequence 7 but is bound to future base sequence 6 (an orphan left by an earlier interrupted pair save).
- Sidecar B has local sequence 6 and is the valid extension bound to current base sequence 5.
- A subsequent save that chooses the copy opposite the newest local Sidecar sequence would erase B before the new base commit.
- If that base commit then fails, base sequence 5 remains active but its matching Sidecar has been destroyed.

Therefore the transaction writer must choose the target copy using the current base binding:

1. Read and validate the currently selected base object and its exact committed sequence.
2. Inspect both Sidecar copies independently: validate storage header, payload CRC, Sidecar semantic schema, identity and base-sequence binding.
3. Among Sidecar copies matching the current base sequence, select the newest matching copy using wrap-safe local sequence ordering (A wins an exact tie). This is the **protected copy**.
4. Choose the other Sidecar copy as the write target, even if that target currently has a newer local sequence but is stale/future-bound.
5. Determine the next Sidecar-local sequence from the newest fully valid Sidecar storage object (header plus payload CRC), independent of its base binding; write the complete new Sidecar snapshot to the selected target with the next base sequence.
6. Read back and fully validate the newly written Sidecar.
7. Commit the base object to its inactive A/B copy while holding the same per-identity transaction lock.
8. Read back the selected base and Sidecar and verify exact sequence binding.

The protected matching copy is never erased before the new base commit. If power is lost or the base commit fails after the new Sidecar commit, the old base still has its old matching Sidecar. The future-bound/new Sidecar is ignored. After the base commit succeeds, the new Sidecar becomes the matching copy and the old one becomes stale.

If no Sidecar copy matches the current base sequence, the current extension is already logically missing and defaults apply. The writer may replace an invalid/stale copy, but must still ensure that a base commit failure cannot cause an older matching copy to be overwritten—there is none to preserve.

Implementation implications:
- The reader must report both per-copy validation/binding results, not only one local-sequence winner.
- The writer must accept an explicit Sidecar target slot selected by the base-binding predicate. Existing `st_save_to(obj, src, len, to)` already has an explicit `to` argument internally, but its public static boundary and sequence selection are not a complete pair API.
- The Sidecar-local sequence source and Sidecar target-slot choice are separate decisions: sequence comes from the newest fully valid local object; target slot is the copy not protecting the current base match.
- The per-identity transaction lock must cover base-sequence observation, Sidecar target selection, Sidecar commit, base commit and final verification.
- Tests must seed an orphan newer Sidecar plus an older matching Sidecar, inject a failure at every subsequent base-commit boundary, and prove the older matching Sidecar remains selected.

This correction supersedes any interpretation of “inactive Sidecar copy” based solely on Sidecar-local sequence order. It does not change the logical payload schema or authorize any Flash allocation.

## 10. Shared autosave-sector staging dependency — FM6 voice-bank transfer

A second source pass found an important storage ownership dependency outside ordinary `st_save()` call sites:

- `firmware/src/fm6_vbank.c` stages an incoming FM6 voice bank in the *older copy of the autosave A/B sectors* (`0xE5000` or `0xE6000`), using a distinct commit-record type `OBJ_FM6VB`.
- `fvb_begin()` chooses the copy that the next ordinary autosave write would overwrite, then erases it for transfer staging.
- `fvb_write()` writes voice records there; `fvb_end()` commits the staged bank and copies it to the separate home sector at `0xE7000`.
- At boot, `fvb_boot()` checks both autosave sectors for a staged bank and finishes the copy to `0xE7000` before autosave restoration.
- `firmware/src/editor.c` calls `autosave_hold()` for FM6B_BEGIN/WRITE/END commands. The existing design intentionally allows a long transfer to be aborted if autosave eventually takes the staging sector: `fvb_tx_ok()` detects the staged commit record was overwritten and returns “no transfer”.

This means the autosave A/B sectors are not exclusively owned by ordinary autosave objects at all times. A future live-autosave pair writer must not erase or rewrite either autosave sector while an FM6 voice-bank transfer is active. It must preserve the existing idle/quiet gating and coordinate with the FM6 bank staging reservation. The current 10-second autosave hold is a delay policy, not a hard mutual-exclusion lock: if a transfer remains open long enough, autosave may intentionally take the sector and invalidate that transfer.

Required future integration:
1. Add an explicit FM6 voice-bank staging-active query/reservation contract (do not infer it only from elapsed time).
2. Make live-autosave pair commit defer or return a retryable busy result while the FM6 transfer owns its staging sector.
3. Keep the current transfer-abort behavior only if explicitly retained as a product decision; do not let the Sidecar coordinator silently change it.
4. Serialize staging begin/write/end and autosave pair commit through the same main-context Flash ownership contract.
5. Add tests for FM6 staging active, autosave deferral, explicit timeout/abort behavior, boot-time staged-bank recovery, and no accidental Sidecar writes into either staging sector.

The Sidecar physical allocation must remain separate from these reserved/reused autosave sectors unless CP1.4.11 explicitly audits and approves a different layout. This finding does not allocate or authorize any sector.
