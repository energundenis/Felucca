# ENERGUN METAL CORE — Sidecar storage transaction audit 01

Date: 2026-10-31
Branch: energun-metal-core
Status: source-backed design audit; documentation only. No firmware, emulator, backup code, Flash map, or device changed.

## 1. Confirmed main-project storage identities

Felucca stores five independently persisted project identities:
- four stored projects: OBJ_PROJECT0..OBJ_PROJECT3;
- one live project: OBJ_AUTOSAVE.

Each identity uses two 4 KiB A/B sectors. The stored projects occupy 0x97000..0x9EFFF; live autosave occupies 0xE5000..0xE6FFF. The live project is not one of the four stored slots: it is separately saved and restored at boot.

Therefore the Metal Core sidecar requires:
- 5 project identities × 2 copies = 10 sidecar sectors;
- one sidecar copy associated with each main-project A/B copy;
- a total of 40 KiB of sector capacity.

The former eight-sector proposal is insufficient. This audit supersedes the eight-sector capacity assumption in earlier draft notes.

## 2. Logical sidecar contents

The main FUN10 project remains unchanged and continues to hold E0..E7 and the existing project data.

The sidecar stores only the Metal Core extension state:
- four tracks × 16 new common P_MC values;
- four tracks × 7 MCX extension values;
- MCX motion events only if the separate extension-motion feature is approved;
- schema and integrity metadata.

The sidecar must not duplicate E0..E7 or existing Felucca project fields as a second source of truth.

All extension values are initialized from the Metal Core descriptor defaults when the matching sidecar is absent, invalid, or from an older schema that cannot be migrated safely.

## 3. Main/sidecar A/B pairing

A sidecar copy is valid only for one exact main-project copy. Its header must include:
- sidecar magic and schema version;
- logical project identity (AUTOSAVE or stored slot 0..3);
- A/B copy identity;
- sidecar generation;
- referenced main-project storage sequence;
- referenced main-project payload length and CRC;
- sidecar payload length and CRC;
- header CRC;
- commit marker.

On load:
1. Load and validate the main project using the existing storage layer.
2. Identify the selected main-project copy, sequence, length and payload CRC.
3. Read the corresponding sidecar A/B pair.
4. Accept only a committed sidecar whose logical identity, A/B identity, schema and referenced main-project sequence/length/CRC all match.
5. If no exact match exists, load the main project normally and initialize extension state to defaults with no MCX motion.

Never reject a valid main project because its optional sidecar is absent or corrupt.

## 4. Power-loss-safe save transaction

For a save of one project identity:
1. Snapshot the project and extension state consistently in RAM while the sequencer is stopped or at a safe save boundary.
2. Pack the new main-project image in RAM and calculate its exact next sequence, length and payload CRC using the existing storage rules.
3. Select the inactive main-project A/B copy.
4. Select the sidecar copy associated with that inactive main-project copy.
5. Erase only that sidecar sector; write payload; read back and verify payload CRC.
6. Program the sidecar header/commit record last; read back and validate the complete sidecar.
7. Commit the inactive main-project A/B copy using the existing header-last storage procedure.
8. Read back and validate the new main project. Only then report save success.

Recovery cases:
- Power loss before sidecar commit: old main and old sidecar remain valid.
- Power loss after sidecar commit but before main commit: old main and old sidecar remain valid; the new sidecar is an orphan and is ignored because it references a main sequence/CRC that was not committed.
- Power loss during main commit: existing main A/B recovery selects the last valid main copy; only its exactly matching sidecar is accepted.
- Power loss after main commit: new main and its already-committed sidecar match.
- Missing/corrupt new sidecar: main project loads with defaults; extension warning is nonfatal.

The implementation must not overwrite the currently selected valid sidecar before the matching main-project commit.

## 5. Clear, replace, slot copy and project switching

The following operations must preserve pairing:
- save live autosave;
- save stored project slot;
- load stored slot into live state;
- duplicate/copy project;
- clear project slot;
- factory/default project creation;
- project import;
- project rename (identity semantics must be explicit);
- backup restore.

A copied or imported project must receive a sidecar identity and generation matching the destination main-project object. Do not copy a source sidecar header unchanged to another project slot.

On project clear, either commit a valid empty/default sidecar linked to the new empty project generation or invalidate both sidecar copies only after the main project transition is safe. The chosen procedure must be tested against power loss.

## 6. Sector map — candidate only, not approved

Source-derived known occupied regions include:
- 0xE0000..0xE4FFF: OTA staging;
- 0xE5000..0xE6FFF: live autosave A/B;
- 0xE7000: FM6 voice bank;
- 0xE8000: stock update record at 0xE8F00; exclude this sector;
- 0xFC000 and above: global/settings storage.

A contiguous ten-sector capacity candidate would be 0xE9000..0xF2FFF (ten 4 KiB sectors; end-exclusive 0xF3000). This is only a sizing illustration derived from the current source map, not an approved Flash allocation.

The update loader scans XDATA sectors in the wider 0xE0000..0xFC000 region and checks an 80-byte update record at sector offset 0xF00. Any future sidecar sector in that scan window must keep its final 256 bytes (sector + 0xF00 .. sector + 0xFFF) erased. This reserve is mandatory but does not by itself prove a sector is free.

No sidecar sector may be whitelisted, erased, programmed, or otherwise allocated until the user's own complete 1 MiB FM-1 V1.5 dump has been obtained and compared with the loader/BSP map. A dump from another FM-1 is not a substitute. No device dump is currently approved as available for this allocation decision.

## 7. Backup archive v2 requirements

Current backup archive v1 uses a fixed object list and has no sidecar objects. Do not change the meaning, length, or count of v1 IDs.

A future versioned archive must include sidecar state for all five identities:
- live autosave extension;
- stored project 0 extension;
- stored project 1 extension;
- stored project 2 extension;
- stored project 3 extension.

For each logical project, the archive must bind the sidecar to the matching main-project sequence/CRC and validate both before any destructive restore operation.

Restore behavior:
- validate the entire archive, all project objects, all sidecars, all lengths and all CRCs before the first Flash write;
- restore each sidecar to the inactive matching sidecar copy and verify it;
- commit the matching main-project copy only after its sidecar is valid;
- restore live autosave last, preserving existing restore ordering;
- for legacy archives without sidecars, explicitly write/default an empty extension for each restored project so stale sidecar values cannot attach to unrelated restored projects;
- report partial restore clearly if power or connection is lost; every individual project identity must remain recoverable as either its old valid pair or its new valid pair.

The browser editor and emulator must negotiate archive v2 explicitly. Existing archive v1 remains readable and writable according to its existing protocol.

## 8. Required host/emulator tests

Implement a pure host-side storage model first. Do not use physical Flash.

Minimum tests:
1. Round-trip all 5 identities × both A/B copies.
2. Save each identity repeatedly through A/B rollover.
3. Power cut after each erase/program/header/commit boundary.
4. Corrupt sidecar payload, payload CRC, header CRC, schema, identity, A/B field, sequence and main-project CRC.
5. Valid main project + absent sidecar loads defaults and remains playable.
6. Orphan sidecar is ignored.
7. Project slot copy/import/clear cannot attach another slot's sidecar.
8. Live autosave extension is independent from all four stored slots.
9. Backup v1 restore clears/defaults extension state without stale data.
10. Backup v2 full round-trip for all five identities.
11. Interrupted archive restore leaves each project identity recoverable.
12. Reserved update-loader tail bytes 0xF00..0xFFF remain erased in every candidate sector.
13. Reject all addresses outside an explicit test-only whitelist.
14. Fuzz malformed lengths/counts/IDs and verify bounded memory access.
15. MCX event pool and extension values remain isolated per track.

## 9. Outstanding decisions and gates

- Confirm whether MCX0..MCX6 need per-step automation/parameter locks in V1. If yes, implement and benchmark a separate namespace/store; never put MCX IDs into the normal 7-bit parameter-lock ID.
- Finalize archive v2 object IDs and protocol versioning.
- Obtain and inspect the user's own complete V1.5 flash dump before any sector selection.
- Prove a recovery route on the exact V1.5 package before any target firmware work.
- Only after host/emulator tests pass may a target build be considered. Hardware writes remain prohibited until the full dump and recovery gate are satisfied.

## 10. Safety statement

This audit is documentation only. No firmware source, parameter IDs, UI, editor protocol, project format, backup format, emulator persistence, Flash whitelist, firmware image, factory firmware, or physical device was changed.
