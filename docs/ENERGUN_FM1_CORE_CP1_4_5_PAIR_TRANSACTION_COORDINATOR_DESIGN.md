# ENERGUN FM-1 CORE — CP1.4.5 Sidecar Pair Transaction Coordinator design

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **LOGICAL TRANSACTION CONTRACT — NOT PRODUCTION CODE**

Related source audit: `ENERGUN_FM1_CORE_CP1_4_5_PERSISTENCE_WRITE_PATH_AUDIT.md`  
Related integrity audit: `ENERGUN_FM1_CORE_CP1_4_7_SIDECAR_INTEGRITY_AUDIT.md`

## 1. Scope and non-goals

Define one transaction contract for a legacy Felucca base object and its future ENERGUN Sidecar extension. The contract must cover project slots, live autosave, user-preset banks and future archive V2 restore.

This document does not freeze the Sidecar binary envelope, assign object IDs, add a storage object, change `OBJ_COUNT`, select Flash sectors, implement storage code, change archive V1, or authorize a physical FM-1 operation.

## 2. Existing storage constraints confirmed in source

The current implementation in `firmware/src/storage.c` has:
- A/B object copies with 4 KiB sectors, payload offset 256 and maximum payload 3840 bytes.
- A 32-byte commit header containing object type, copy slot, sequence, length, payload CRC-32 and header CRC-32.
- Header-last writes, read-back verification and wrap-aware sequence selection.
- `st_current()` validates headers and then bodies; if the newest candidate body is corrupt it can fall back to the older valid copy.
- `st_load()` returns payload bytes/length only; it does not expose the selected copy or committed sequence.
- `st_save_to()` accepts an explicit target copy internally, but still derives the next local sequence from `st_current()`.
- A single static `st_buf` shared by storage operations.

The existing `storage_test.c` already provides a simulated NOR device, byte-level program interruption, erase/write-protection failures, CRC checks, A/B fallback and sequence-wrap tests. This is a strong base for extending tests against the real included `storage.c`, rather than creating a behavioral clone.

## 3. Logical identities

The coordinator operates on one pair identity at a time:

| Pair kind | Base identity | Sidecar logical content |
|---|---|---|
| Project slot | One of four `OBJ_PROJECT0 + slot` objects | All four parts' P_MC and Private state plus associated motion extension |
| Live autosave | `OBJ_AUTOSAVE` | Complete live P_MC/Private/motion extension |
| User-preset bank | One of two `OBJ_UPRESET0 + bank` objects | Private extension for all 16 slots in that bank, with explicit slot-presence semantics |
| Archive V2 destination | One destination identity from the above | Validated archive extension rebound to destination base generation |

A Sidecar must identify its pair kind and logical slot/bank independently of its physical storage location. Physical Sidecar object IDs and sectors are deliberately unspecified until CP1.4.11 is complete.

## 4. Required storage read primitives

The production implementation needs an internal read result that exposes, for each copy:
- header validity;
- payload CRC validity;
- header and payload metadata;
- copy slot;
- local storage sequence;
- payload bytes (or a caller-owned bounded snapshot);
- semantic Sidecar validity;
- pair identity validity;
- base-sequence binding match.

The base-object read must return the exact selected copy and sequence chosen using the existing storage fallback rules. It must preserve the current behavior where a corrupt newest payload falls back to the older valid base copy.

For Sidecar, the coordinator must inspect **both** copies. It must not call a Sidecar-local newest-only selector and then assume that result is the one associated with the current base.

## 5. Commit algorithm

All sequence values below are the existing 32-bit storage sequence domain. Sidecar payload schema/field widths remain unspecified. The Sidecar envelope must include the logical pair identity and a base-generation binding before this algorithm can be implemented.

### Phase A — capture and preflight

1. Reject a second/re-entrant pair transaction for the same shared storage context.
2. Capture an immutable Sidecar snapshot into caller-owned RAM. The snapshot must include a generation token so later runtime changes cannot be accidentally marked clean.
3. Validate snapshot size, schema version, identity and semantic constraints before any Flash erase.
4. Read the currently selected valid base copy and its exact sequence. If no valid base exists, record the defined empty-base state; do not infer a sequence from erased bytes.
5. Inspect both Sidecar copies and classify them as invalid, valid-but-stale, valid-future/orphan, or matching-current-base.

### Phase B — protect the old pair and stage the new extension

6. Among fully valid Sidecar copies whose identity and base binding match the current base sequence, select the newest matching copy using wrap-safe sequence comparison; A wins an exact tie. This is the **protected matching copy**.
7. If a protected matching copy exists, select the other physical copy as the Sidecar write target, regardless of which copy has the larger local sequence. Never erase the protected matching copy before the new base commit.
8. If no matching copy exists, both copies are stale/invalid/orphan relative to the selected base. Select a deterministic replaceable target (prefer invalid, otherwise the older fully valid local sequence; A wins a tie). The current base already has no valid Sidecar, so defaults remain the recovery state if the transaction stops.
9. Determine the next Sidecar-local sequence from the newest fully valid Sidecar storage object (header and payload CRC), regardless of whether that object's base binding matches. If neither is fully valid, start from the storage layer's first-write sequence convention.
10. Write the complete new Sidecar snapshot to the chosen target, bound to the **next base sequence**. Commit header last and read back/validate the full record. Do not touch the base object yet.

### Phase C — commit the base and verify the pair

11. While the same transaction guard remains held, commit the base object to the copy opposite the currently selected valid base copy, with the next base sequence. Reuse the current storage header-last and read-back verification behavior.
12. Re-read the selected base and both Sidecar copies. Require the selected base sequence to equal the Sidecar binding exactly, and require the selected Sidecar to pass storage CRC, semantic schema and identity validation.
13. Report success only after the complete pair is verified. If the snapshot generation still equals the live extension generation, clear dirty state; otherwise leave the extension dirty for a later save.
14. Release the transaction guard on every exit path.

### Why the old pair survives a failed base commit

If a matching Sidecar exists, it is protected while the other copy is staged with the future base sequence. If the Sidecar write tears, the old matching copy remains. If Sidecar commit succeeds but the base commit tears or fails, the old base remains selected and still has its matching Sidecar. The staged future-bound Sidecar is ignored by the reader. Only after the base header is committed does the new base sequence match the staged Sidecar.

This is the essential power-loss invariant. It is stronger than “write Sidecar first” and stronger than “write the inactive Sidecar copy.”

## 6. Load algorithm

1. Select the valid base copy using existing Felucca A/B and payload-CRC fallback behavior; obtain its exact sequence.
2. Inspect both Sidecar copies independently.
3. Accept only Sidecar records with valid storage header, payload CRC, supported semantic schema, matching logical identity and exact base-sequence binding.
4. If both copies match, select the newest matching Sidecar sequence (A on tie).
5. If neither matches, load the base normally and apply deterministic ENERGUN extension defaults. Never attach a future/orphan Sidecar or partially apply malformed extension data.
6. Do not rewrite storage during load merely to repair an absent/stale Sidecar. Any migration or repair is a separate explicit save operation.

## 7. Sequence and tie behavior

- Use the same wrap-aware comparison rule as Felucca storage: candidate B is newer than A only when `B.seq != A.seq` and unsigned `B.seq - A.seq < 0x80000000`.
- A wins exact ties.
- The Sidecar's local sequence orders valid Sidecar records but does **not** determine which record belongs to the selected base.
- The base sequence binding determines pair membership.
- A Sidecar record bound to a future base sequence is an orphan until that exact base sequence is committed.
- If no Sidecar record matches the selected base, defaults apply.
- A future ABI review must consider whether binding only a 32-bit sequence is sufficient across theoretical sequence wrap/reuse. A base payload CRC or stronger base-generation fingerprint may be needed; do not add fields until the envelope ABI is reviewed.

## 8. Concurrency and storage-buffer ownership

Current source uses one global static `st_buf` and direct storage calls from project, preset and archive code. The coordinator must therefore:
- be the sole owner of paired persistence operations;
- use a non-reentrant transaction guard around the complete pair sequence;
- keep caller snapshots outside `st_buf`, because every storage read/write may overwrite that buffer;
- avoid nested storage operations while a source pointer aliases `st_buf`;
- run only in the existing main/control context, never in the audio ISR;
- preserve existing transport-stop and flash timing rules;
- serialize autosave against user-triggered save/rename/erase and archive restore.

A simple guard is not a substitute for a real scheduler mutex if future firmware adds concurrent storage tasks. Under the current single-main-loop execution model, it is intended to reject re-entrancy and document exclusive ownership, not to claim multicore/thread safety.

## 9. Operation-specific semantics

- **Project Save/Save As:** capture live extension with the same musical snapshot boundary as `project_capture()`; commit pair before showing SAVED.
- **Project Rename:** preserve the full extension but rebind it to the new base sequence because rename rewrites the base object.
- **Project Erase:** commit an explicit empty/tombstone Sidecar bound to the newly committed empty base, or use a separately reviewed invalidation rule. An old extension must never reappear after slot reuse.
- **Live Autosave:** preserve existing quiet-time/throttling behavior; capture and commit the base and extension as one serialized generation.
- **Preset bank mutation:** bank is one base object. Any slot save/rename/erase must preserve all other 15 slots' extensions and commit a complete bank Sidecar snapshot. Slot presence must be explicit.
- **Preset load:** restore Private values for the selected part/slot only from a matching bank Sidecar. Missing slot extension uses defaults.
- **Archive V1:** remains legacy-only; restoring a V1 archive must never inherit a destination's unrelated Sidecar.
- **Archive V2:** validate the complete incoming base+Sidecar pair before destination erase; commit through this same coordinator and rebind to the destination's next base sequence.
- **FM6 patch object:** remains a separate persistence object/transaction. No cross-object atomicity is claimed.

## 10. Host acceptance matrix — use real storage implementation

Extend `tests/storage_test.c` or a narrowly scoped companion that includes the real `firmware/src/storage.c` and reuses its simulated NOR hooks. Do not test a copied coordinator model as if it were production storage.

Minimum cases:
1. Empty storage: first base+Sidecar pair commit; verify first-write sequence convention and exact binding.
2. Normal replacement: second pair commit; old copy becomes stale, new pair loads.
3. Sidecar payload tear at every byte; base remains old and old Sidecar remains selected.
4. Sidecar header tear at every byte; base remains old and old Sidecar remains selected.
5. Base erase failure after Sidecar commit; old base and old matching Sidecar remain selected.
6. Base payload tear at every byte after Sidecar commit; old pair remains selected.
7. Base header tear at every byte after Sidecar commit; old pair remains selected.
8. Orphan future Sidecar with higher local sequence plus older matching Sidecar: next transaction must protect the older matching copy; inject all base-commit failures.
9. Corrupt newest base body: base fallback chooses older base sequence and only its matching Sidecar is loaded.
10. Corrupt newest Sidecar body: older matching Sidecar remains eligible.
11. No matching Sidecar: deterministic defaults; no stale/future attachment.
12. Both Sidecar copies match: wrap-aware newest matching sequence and A tie behavior.
13. Sequence wrap at 0xFFFFFFFF -> 0 for base and Sidecar local sequences.
14. Wrong identity, unsupported schema, malformed length, invalid presence bitmap and semantically invalid payload: reject whole Sidecar.
15. Generation changes during a save: persisted snapshot is valid, but live dirty flag is not cleared.
16. Project erase and preset slot erase/reuse: stale extension never reappears.
17. Bank mutation: all untouched slots retain their extension values.
18. Archive V1 restore: base restored with extension defaults.
19. Archive V2 destination restore: extension rebound to destination next base sequence.
20. Re-entrant transaction attempt: second writer rejected without any Flash operation.

Fault injection must test erase failure, every programmed payload/header byte boundary and read-back corruption. The assertions must inspect selected base and Sidecar identities/sequences after each simulated reboot (reset in-memory state, preserve NOR bytes).

## 11. Integration gate and no-go conditions

Do not implement this coordinator until:
- Sidecar envelope ABI and identity/generation fields are frozen;
- Sidecar semantic validation rules are frozen;
- target Sidecar object mapping is approved after CP1.4.11 Flash/bootloader/OTA/recovery audit;
- exact storage ownership and all direct paired write paths are enumerated;
- host test harness can exercise the real storage implementation and simulated power loss.

No production firmware code, object IDs, Flash allocation, bootloader change, archive V2 implementation, factory image modification or physical FM-1 access is authorized by this design document.

**CP1.4.5 remains a design milestone. CP1.4.7 and CP1.4.8 remain open until production reader/writer code and real host acceptance tests pass.**
