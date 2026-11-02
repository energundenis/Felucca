# ENERGUN FM-1 CORE — CP1.4.9 backup / restore integration audit

Date: 2026-11-02  
Canonical repository: `energundenis/Felucca`  
Branch: `energun-metal-core`  
Status: **SOURCE AUDIT COMPLETE — ARCHIVE EXTENSION DESIGN REQUIRED; NO PRODUCTION CHANGES**

## 1. Scope and source pins

Audited the existing Felucca editor backup/archive path to determine how the proposed CP1.4 Sidecar must participate in backup and restore.

Pinned source files on the audited branch:
- `firmware/src/editor_backup.c` — `3c68a2ea997238007c3f74bdf20966c5173296e6`
- `firmware/src/storage.c` — `aea1162eb53defacc99ae631a9791be7877aae96`
- `firmware/src/project.c` — `c972d6e3a289b93f20b632007869b3b8e009e929`
- `firmware/src/upreset.c` — `4766bf6e7f29d6bafbdc3288e5045fe293dfece5`
- `tests/backup_test.c` — `709c3cbeb55a138061759df456891fb61594fdf0`
- `web/fm1backup.js` — `17995d56a700a3054472d1bffe22969b0ea5eaf2`
- `web/test_backup.mjs` — `7788efe1aa245e3af6ab5c8ed719834e74b56417`

The source tree was read from the canonical branch after the exact-head baseline CI closure. No production source was changed by this audit.

## 2. Existing archive manifest

Firmware `ED_BK_IDS` currently exposes 13 logical archive objects:

| Archive ID | Meaning |
|---:|---|
| 0 | Current live musical project snapshot (captured from runtime as FUN10) |
| 1 | Settings |
| 2–5 | Four stored project slots |
| 6–7 | Two user-preset banks |
| 8 | Retired legacy FM6 patch bank; listed empty in current firmware, retained for old archive migration |
| 9 | User-preset FM6 patch object |
| 32–34 | Three user sample slots |

These are archive protocol IDs, not the internal storage object IDs or approved Sidecar IDs.

The browser implementation accepts exactly three legacy manifest shapes:
- 13 objects: current format;
- 12 objects: earlier format without archive ID 9;
- 11 objects: earlier format without IDs 8 and 9.

The JSON archive is `format: "felucca-backup"`, `version: 1`. `readBackup()` requires the exact object count/order for one of those versions. It validates each object's Base64 encoding, size, CRC and sample metadata before returning a restorable archive.

## 3. Capture behavior and integrity

Firmware `ED_BACKUP_LIST` captures the current runtime project into the shared serialized scratch buffer, captures settings, and returns each object's length and CRC. `ED_BACKUP_GET` streams chunks from that snapshot.

The snapshot is deliberately invalidated by:
- USB reset;
- a project wire-generation change that reuses the staging buffer;
- transport activity / invalid transport state;
- malformed or expired transfer sessions.

The PUT path stages one small object in main-loop RAM, checks the complete byte count and CRC before committing, validates the object's format and semantic fields, then calls the existing storage commit path. A project/preset object is therefore individually validated and committed; the complete multi-object archive is not one global flash transaction.

The web capture path independently recomputes each object's CRC and rejects a device snapshot that changes during transfer.

## 4. Restore ordering and atomicity boundary

The browser validates the complete archive before the first destructive request. It then restores non-live objects in archive order, restores settings, and restores the live musical project last.

Firmware commits each ordinary storage object independently through `st_save()`. The storage layer uses an A/B pair: erase the inactive copy, write payload, write the 32-byte CRC-protected commit header last, and read back the committed object. This protects each individual object against a torn write; it does not make a project + Sidecar pair or the whole archive atomic.

A disconnect or failed chunk can leave an archive partially restored. The browser aborts the current staged object and stops; previously committed objects remain restored. Sample restore uses its own BEGIN/WRITE/END path and has a separately documented disconnect limitation.

Special case: archive ID 0 is not restored through `st_save(OBJ_AUTOSAVE,...)`. Firmware imports it and calls `project_restore_runtime()` to restore the live runtime. Therefore the future Sidecar identity for live autosave cannot be treated as an ordinary archive storage object without an explicit live-runtime/autosave binding design.

## 5. Sidecar integration gaps found

### 5.1 Archive protocol does not yet carry Sidecar

- `ED_BK_IDS` has no Sidecar objects.
- `ed_bk_object()` has no Sidecar capture path.
- `ed_bk_write()` currently rejects ordinary archive object IDs greater than 9 at PUT begin.
- Browser `BACKUP_IDS`, manifest validation and `readBackup()` accept only the 13/12/11-object V1 shapes.
- There are no archive-version fixtures for Private values, P_MC values or P_MC Motion records.

A Sidecar implementation that is not added to archive capture/restore would violate the accepted requirement that Private values for all four project slots, live autosave and all 32 user presets participate in backup/restore.

### 5.2 Sidecar cannot be restored as an opaque byte-for-byte object

The Sidecar draft binds each extension to the exact base object's committed storage sequence. During restore, `st_save()` assigns a new local sequence to the destination base object; it does not preserve the source device's sequence from the archive. Copying a Sidecar archive object unchanged would therefore leave its base-sequence binding stale and the extension correctly ignored on load.

The restore coordinator must validate the archived extension, commit/rebind it against the destination base object's *new* sequence, and verify the resulting pair. It must not trust a source-device sequence as a destination sequence.

### 5.3 Project/preset pair ordering needs a coordinator

Current browser restore loops over independent archive objects. It does not group a base project and its Sidecar into one pair operation. The firmware backup PUT handler also commits one object at a time.

Required future behavior:
- Stage and validate both base and extension before the destructive pair commit.
- Use the same pair-aware ordering and sequence reservation rules as normal project/preset saves.
- Ensure an interrupted restore leaves either the previous matching pair or the new matching pair selectable; never attach a stale extension to a different base object.
- If the design intentionally allows base restore without an extension, apply deterministic defaults and report the extension as missing; do not claim full restore success for Private/P_MC data.
- Preserve the existing legacy archive V1 reader and restore path.

### 5.4 Live runtime / autosave requires separate semantics

Archive ID 0 captures live runtime musical state, while Sidecar planning identifies live autosave as its own persistent identity. The restore path calls `project_restore_runtime()`; it does not directly commit the live autosave storage object in that call.

Before implementation, define how the live Sidecar snapshot is restored, how it becomes associated with the next autosave base sequence, and what happens if power is lost before the next autosave commit. Do not assume archive ID 0 and the persistent autosave object are interchangeable.

### 5.5 User-preset FM6 patch data remains a separate object

The existing user-preset FM6 patch data is archived as ID 9 and persisted separately from the preset bank. The existing `up_store_cat()` path commits the preset bank through `up_put()`, then writes the FM6 patch object through `upf_store()`.

A Private Sidecar pair for a preset bank does not make the separate FM6 patch object atomic with the base bank and extension. Preserve the current restore ordering and add explicit tests; do not claim whole-preset atomicity across all three objects.

## 6. Required implementation and regression work

After CP1.4.1–1.4.5 have stable logical ownership and schema:

1. Define an archive V2 representation that includes the required Sidecar identities without changing the meaning/order of V1 object IDs.
2. Preserve V1 archives (11/12/13-object variants) as readable and restorable.
3. Add firmware LIST/GET/PUT support for Sidecar logical objects through symbolic archive identities; do not expose raw Flash addresses.
4. Extend browser manifest, JSON validation, capture and restore for V2.
5. Implement a pair-aware restore coordinator that rebinds Sidecar to the destination base sequence rather than copying source sequence metadata.
6. Define live runtime ID 0 versus persistent live-autosave Sidecar semantics and power-loss behavior.
7. Include both user-preset banks and all 32 slot-presence/Private records; clear deleted/empty slots so stale extensions cannot attach to reused slots.
8. Keep the separate FM6 patch object explicitly outside the claimed base+Private Sidecar atomicity boundary unless separately redesigned.
9. Add host firmware tests and browser tests for legacy V1 archives, V2 round-trip, missing/corrupt Sidecar, wrong base generation, project/preset rename/erase/overwrite, interruption at each pair-commit stage, live restore before autosave, all 32 preset slots and FM6 patch ordering.
10. Run full baseline CI and inspect the archive-specific firmware and web test logs on the exact code HEAD.

## 7. Acceptance boundary

This is a source-backed audit only. It:
- does not define archive V2 field layout or object IDs;
- does not allocate storage object IDs or Flash sectors;
- does not change `editor_backup.c`, `storage.c`, `project.c`, `upreset.c` or browser backup code;
- does not register METAL CORE or merge any source branch;
- does not write to physical FM-1.

**CP1.4.9: SOURCE AUDIT COMPLETE; ARCHIVE V2 DESIGN AND IMPLEMENTATION OPEN.**  
**CP1.4.11 physical Flash allocation remains separately blocked.**
