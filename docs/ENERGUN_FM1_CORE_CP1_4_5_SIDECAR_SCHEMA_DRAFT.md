# ENERGUN FM-1 CORE — CP1.4.5 Sidecar schema and identity draft

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **LOGICAL SCHEMA DRAFT — no binary ABI or Flash allocation approved**

## 1. Purpose and dependencies

The Sidecar carries V1 values that cannot safely be inserted into the legacy FUN10 base object:
- 16 common P_MC values for each of four project tracks/parts;
- 13 Private values for each of four parts;
- P_MC Motion Lock records removed from the legacy base motion list so old firmware cannot misinterpret new IDs as E0..E7.

For user presets, the Sidecar carries 16 P_MC + 13 Private values for each of the 16 slots in a preset bank.

Dependencies:
- CP1.4.1 must resolve E0–E7 ordering and the OSC2 LEVEL / TIMBRE overlap before stable key IDs are assigned.
- CP1.4.2 must complete curve review and legacy compatibility approval.
- CP1.4.3/4.4 access and Motion contracts are drafted in `ENERGUN_FM1_CORE_CP1_4_3_4_PRIVATE_MOTION_IMPLEMENTATION_DESIGN_DRAFT.md`.
- CP1.4.6 capacity, CP1.4.7 A/B transaction implementation, CP1.4.8 project/preset integration and CP1.4.11 physical Flash audit remain separate gates.

This is a logical envelope and capacity model only. It assigns no byte offsets, fixed-width field layout, stable parameter IDs, object IDs, sector addresses or production ABI.

## 2. Logical identities

Seven independently versioned extension identities are required:

| Identity | Count | Payload role |
|---|---:|---|
| Project slots | 4 | P_MC values for four parts, Private values for four parts, P_MC motion extension |
| Live autosave | 1 | Same project payload shape as a project slot |
| User preset bank 0 | 1 | P_MC + Private extension data for all 16 preset slots |
| User preset bank 1 | 1 | P_MC + Private extension data for all 16 preset slots |
| **Total** | **7** | Each identity has an A/B copy pair |

The user-preset extension is bank-scoped, not a single unpaired record per preset. This is necessary because the current factory storage object writes each bank as one object containing 16 slots. The Sidecar must be rebound whenever the bank is rewritten, including rename, category, overwrite, erase, migration or compaction operations.

## 3. Logical payload contents

### 3.1 Project and live-autosave identity

Per part:
- 16 P_MC raw parameter codes, one byte each (15 unipolar 0..127 candidates; FOLD SYMMETRY signed -64..63);
- 13 Private typed values, one byte each in the current capacity estimate.

Across four parts: 4 × (16 + 13) = **116 value bytes**.

P_MC Motion extension:
- only P_MC Motion records are stored here; ordinary legacy-compatible motion records remain in base FUN10;
- one logical event uses the same three data fields as existing FUN10 motion (place, 7-bit parameter ID plus lock bit, signed value): **3 bytes per event**;
- maximum extension count is bounded by the shared global 128-event pool. The base FUN10 motion count plus Sidecar P_MC motion count must never exceed 128.

| Project payload component | Maximum logical bytes |
|---|---:|
| Four parts × 16 P_MC | 64 |
| Four parts × 13 Private | 52 |
| P_MC motion count | 1 |
| P_MC motion events (up to 128 × 3) | 384 |
| **Maximum logical value/event payload** | **501** |

This 501-byte estimate excludes the Sidecar envelope and any future alignment/reserved bytes. The actual Sidecar record remains comfortably below one 3840-byte storage payload, but the physical sector map is not selected.

### 3.2 User-preset bank identity

Each used preset slot carries:
- 16 P_MC raw codes;
- 13 Private typed values.

16 slots × 29 values = **464 bytes**. A slot-presence bitmap and envelope metadata are additional. Empty slots must not accidentally inherit stale extension values from a previously occupied slot.

The extension is bank-generation-bound. Rewriting any preset bank requires carrying forward all still-used slot extension records and clearing the erased slot's extension before committing the new bank.

## 4. Envelope and versioning model

The logical envelope must identify:
- Sidecar magic and format version;
- logical schema version;
- identity kind and identity index;
- base storage-object sequence to which this Sidecar belongs;
- payload length and logical record counts;
- extension payload;
- optional reserved fields for forward-compatible evolution.

The existing `storage.c` layer already supplies an outer 32-byte A/B commit header with object type, copy slot, local sequence, payload length, payload CRC and header CRC. It programs the payload first and commits the header last. Do not duplicate or redefine that outer commit record inside the Sidecar payload.

The Sidecar's own logical schema version is independent from the storage layer's local A/B sequence and independent from the base object's sequence.

## 5. Base-object binding and pair transaction

### 5.1 Why binding is required

The current storage layer maintains an A/B sequence independently for each object. The newest Sidecar copy is not automatically the extension belonging to the currently loaded project or preset bank. A stale extension must never attach to a newly overwritten base object.

Each Sidecar payload therefore carries the exact base-object sequence it belongs to. On load, inspect both Sidecar A/B copies and select the valid copy whose bound base sequence equals the currently selected base object's sequence. If neither matches, treat the extension as missing and apply deterministic defaults.

The current `st_current()` helper chooses the newest valid copy without a base-sequence predicate. A matching-copy reader or equivalent pair-aware storage API is required; the present helper is not sufficient by itself.

### 5.2 Save ordering

When a base project or preset bank changes, even if the Private/P_MC values themselves did not change:
1. Snapshot the complete current extension and validate it; carry forward unchanged values/events as well as edits.
2. Determine the next sequence the base object will receive.
3. Write the complete extension to its inactive A/B copy, tagged with that next base sequence; verify its outer commit record and payload.
4. Commit the new base object to its inactive A/B copy, receiving the tagged sequence.
5. Read back both and verify that the current base sequence and selected Sidecar binding match.

Power-cut behavior:
- Cut before the new Sidecar commits: old base + old matching Sidecar remain active.
- Cut after Sidecar commit but before base commit: old base still selects its old matching Sidecar; the future-bound Sidecar is ignored.
- Cut after base commit: new base selects the new matching Sidecar.
- A torn base commit leaves the previous base sequence active, which continues matching the previous Sidecar copy.

For an extension-only edit with no base-object change, write the new Sidecar copy tagged with the current base sequence; no unnecessary base rewrite is required. Conversely, every base-object rewrite must rebind the complete current Sidecar snapshot to the next base sequence, or the loader will correctly treat the old extension as stale.

### 5.3 Required storage-layer additions

Before implementation, audit and design:
- a safe way to read the current base object's committed sequence;
- a safe way to calculate the next base sequence without racing another save;
- a reader that validates both Sidecar copies and chooses by bound base sequence, not simply newest Sidecar-local sequence;
- one main-loop transaction coordinator that serializes base/Sidecar saves and prevents concurrent autosave or editor writes from changing the snapshot;
- generation-aware dirty clearing so a newer runtime edit cannot be marked saved by an older snapshot.

The current storage implementation has A/B commit semantics, but it does not yet provide this cross-object pairing contract.

## 6. Capacity summary

| Identity type | Count | Maximum logical payload per identity | A/B copies |
|---|---:|---:|---:|
| Project slot | 4 | 501 bytes + envelope | 8 |
| Live autosave | 1 | 501 bytes + envelope | 2 |
| User preset bank | 2 | 464 bytes + bitmap + envelope | 4 |
| **Total** | **7** | — | **14 logical copies** |

If each copy eventually uses one 4 KiB sector, the logical reservation would be 14 sectors (56 KiB). This is a capacity estimate only, not a Flash map or permission to reserve/erase any sector. CP1.4.11 must independently audit the full 1 MiB image, bootloader/OTA/recovery ranges, all live objects and restore procedure before any physical allocation is considered.

## 7. Compatibility rules

- Base FUN10 remains at legacy `np=111` for old firmware compatibility; the Sidecar carries new P_MC and Private values.
- Do not write `np=127` to the legacy FUN10 base: factory firmware rejects `np > 111`, and the fixed 3840-byte object cannot hold the additional 64 parameter bytes across four tracks.
- P_MC Motion records must be removed from the base motion list and placed in Sidecar. Otherwise old firmware may interpret IDs 103..110 as its E0..E7 and reject or misapply IDs 111..118.
- On new firmware load, import/migrate the base project first, then validate and merge matching Sidecar P_MC values and motion events.
- Combined base + Sidecar motion events must remain <=128. Duplicate (track, step, parameter) keys, invalid IDs, invalid values or overflow reject the extension atomically.
- Old firmware ignores Sidecar objects and continues to load the legacy base. New firmware with no matching Sidecar loads the base and applies defaults for all extension values.
- User preset bank Sidecar is bound to the exact base bank sequence. Old firmware reads the base bank and ignores the extension.

## 8. Open decisions and implementation gates

1. CP1.4.1 final E0–E7 order and the OSC2 LEVEL / TIMBRE overlap.
2. CP1.4.2 final per-control curve review and explicit acceptance of the legacy base FUN10 projection.
3. Whether one-byte typed values are sufficient for every Private enum/boolean/continuous key; current estimate assumes yes.
4. Exact payload field order, widths, reserved values, checksums, schema migration and malformed-record behavior.
5. Storage-layer pair API, base sequence exposure and transaction coordinator.
6. Full capacity and RAM/stack accounting on AC79.
7. User preset bank overwrite/delete/rename and all-32-slot pairing tests.
8. Archive backup/restore inclusion and interrupted restore behavior.
9. CP1.4.11 physical Flash map and restore qualification before assigning sectors.

## 9. Acceptance boundary

This document is a logical design draft only. It does not:
- approve a binary Sidecar ABI or field offsets;
- add storage object IDs or change `OBJ_COUNT`;
- assign physical Flash sectors or reserve/erase Flash;
- change `storage.c`, `project.c`, `upreset.c` or editor protocol code;
- register METAL CORE or merge any branch;
- write to the physical FM-1.

**CP1.4.5 is DESIGN-DRAFTED, NOT FROZEN. CP1.4.6–4.11 remain OPEN.**
