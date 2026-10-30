# ENERGUN METAL CORE — parameter storage plan 01

Date: 2026-10-31
Branch: `energun-metal-core`
Status: **PROPOSAL — not implemented or user-approved**. This resolves the project-size conflict found during the storage audit.

## 1. Why the eight EDIT values are not the whole parameter model

Felucca's engine ABI exposes eight generic EDIT values, `P_E0..P_E7`, on two pages of four. They are already stored in projects and user presets.

The existing track model also supports common parameters before `P_E0`. These are the correct place for dedicated engine-specific pages such as OSC, FM, FOLD, LPG, and VOICE. The page table is static but already has engine-specific visibility rules.

Current limits:

- Current `P_COUNT = 111`, `P_E0 = 103`.
- Parameter IDs must remain below 128 because bit 7 is reserved by motion/parameter-lock records. Therefore at most 16 additional common parameter IDs can be introduced before `P_E0` (`P_COUNT <= 127`).
- User-preset parameter storage has a 144-value ceiling (`UP_PMAX * 2`), so a total `P_COUNT` of 127 remains within its declared capacity.
- The current FUN10 project is fixed at 3840 bytes. Its 16-byte tail reserve was mostly consumed by MIDI-IN-TRACK metadata; only three bytes remain. One new common per-track parameter needs four packed bytes (one value for each of four tracks). Therefore no complete additional common parameter can be added to the existing FUN10 main object.

Conclusion: dedicated common parameter IDs remain the preferred model, but their project persistence must be externalized. Do not use undocumented macros as a substitute for independent controls unless the user explicitly approves that compromise.

## 2. Proposed parameter model

1. Add a bounded set of `P_MC_*` common parameter IDs immediately before `P_E0`, preserving `P_E0..P_E7` as the final eight IDs.
2. Keep all new IDs below 127. The absolute maximum is 16 new IDs; the actual count must come from the audited v98 control inventory.
3. Use the existing common `TP[]` descriptor table for labels, units, ranges, and defaults.
4. Add METAL CORE-specific pages to the existing page table and make them visible only for the METAL CORE engine.
5. Put FOLLOW and METAL on the OSCILLATORS / SYNTH VOICE page/block, not on FOLD. They remain independent ON/OFF parameters.
6. Keep the eight E values available to the engine ABI; do not repurpose or shift their meaning.

Old projects and user presets continue to use the count-based migration rule: the last eight stored values are mapped to the current `P_E0..P_E7`; newly added common parameters receive defaults unless a valid project sidecar supplies them.

## 3. Proposed project sidecar

The FUN10 project object remains 3840 bytes and retains its existing magic, size, and base parameter count. New `P_MC_*` values are stored in a separate sidecar object.

### Sector layout candidate

- Four project slots × two A/B copies = eight sectors.
- Candidate sector range: `0xE9000–0xF1000` (eight 4 KiB sectors).
- The final 256 bytes of every sector (`sector + 0xF00 .. sector + 0xFFF`) remain erased to protect the update loader's record scan.
- Sidecar header at the start of the sector; payload begins at offset `0x100` and ends no later than `0xF00`.
- Maximum sidecar payload per sector: 3584 bytes. The expected METAL CORE parameter payload is much smaller.

This is only a candidate. It must not be allocated until a full dump from the user's own FM-1 V1.5 confirms that the sectors contain no device-specific data that must be preserved.

### Sidecar record

The record should contain at least:

- magic and schema version;
- project slot and A/B copy number;
- sidecar generation sequence;
- the main project's storage sequence and payload CRC;
- sidecar payload length and CRC;
- header CRC;
- the four tracks' new `P_MC_*` values.

The sidecar must be accepted only when both its project slot and its referenced main-project generation (sequence + CRC) match the selected project. If no matching valid sidecar exists, the main project still loads and the new parameters receive defaults.

### Power-loss-safe save order

1. Pack the new main project in RAM and calculate the exact project payload CRC and next storage sequence.
2. Write the inactive sidecar sector: erase, payload, verify, then write its commit header last.
3. Write the inactive main project A/B sector using the existing header-last commit procedure.
4. On load, select the valid main project first, then select only a sidecar that references that exact project generation.

If power is lost before step 3 commits, the old main project still matches the old sidecar copy. If step 3 commits, the new sidecar is already present. An orphaned sidecar that does not match the selected project is ignored.

Project clear/replace must also clear or invalidate the associated extension. A corrupt/missing sidecar must never make the main project unloadable.

## 4. Backup / restore implications

A sidecar is part of the project and must be included in complete musical archives.

The existing backup list has:
- ID 0: live project snapshot;
- IDs 2–5: four stored project slots.

A versioned backup extension will need to carry the live project's extra values and one extension object for each stored project slot. Restore must preserve the project-generation link and use the same safe ordering: write/validate the sidecar first, then commit the matching project. Old archives without extension objects load with defaults.

Required coordinated changes if this plan is approved:
- `editor_backup.c`: list, capture, validate, and restore the live and stored-project extensions;
- `web/fm1backup.js`: versioned archive IDs and validation;
- editor / emulator tests: round-trip, missing extension, corrupted extension, and interrupted restore.

Do not increase the current 3840-byte project object or silently enlarge the current A/B storage object.

## 5. Flash / loader safety

The BSP's update loader scans sectors in `[0x93000, 0x97000)` and `[0xE0000, 0xFC000)`, skipping Felucca's main store `[0x97000, 0xE0000)`. It checks an 80-byte update record at offset `0xF00` in each scanned sector and erases the sector if a valid record is found.

- `0xE7000` is already used by Felucca's FM6 voice bank.
- `0xE8000` contains the stock-firmware update record at `0xE8F00`; do not allocate it.
- Candidate `0xE9000–0xF1000` sectors are within the loader scan window. Keep each sector's final 256 bytes erased.
- `storage_hw.c` currently does not expose general XDATA writes. A dedicated XDATA API must use an explicit sector whitelist, audio-safe erase/program handling, read-back verification, and host tests.
- The browser emulator already persists changed sectors from a full 1 MiB NOR image; add sidecar-specific tests rather than inventing a new emulator persistence layer.

## 6. Required gates before implementation

1. Inspect the exact METAL CORE v98 source and count the independent controls; do not infer the complete v98 control set from v99/v104.
2. Keep the new parameter count at or below 16 and verify `P_COUNT <= 127`.
3. Verify user-preset migration and project base-count migration, including old FUN10 and all older supported formats.
4. Verify the candidate XDATA sectors against the user's own full V1.5 dump.
5. Implement and test the sidecar A/B transaction, project-generation matching, backup/restore, and browser power-cycle behavior on a separate branch.
6. Only after all host tests pass, consider a target build. No hardware write until the user's own dump and a proven recovery route exist.

## Explicit non-goals

- No change to the existing FUN10 project object size or magic.
- No unbounded growth of `P_COUNT`; the hard limit remains 127.
- No macro compression of independent controls without explicit approval.
- No XDATA allocation, flash write, firmware build, or physical-device operation in this proposal.
