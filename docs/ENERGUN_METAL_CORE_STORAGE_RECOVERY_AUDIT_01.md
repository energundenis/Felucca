# ENERGUN METAL CORE — storage and recovery audit 01

Date: 2026-10-31
Branch: `energun-metal-core`
Status: source audit only. No firmware source, storage format, flash contents, or hardware state changed.

## 1. Project parameter capacity

Pinned Felucca source findings:

- Current project format is FUN10 / `FUNA`, fixed at 3840 bytes.
- `project.c` documents `P_COUNT = 111`, `P_E0 = 103`.
- The project format has only four remaining common parameter bytes before its reserved tail is exhausted.
- Common parameters must be inserted immediately before `P_E0` to preserve the established migration rule that the last eight stored values are `P_E0..P_E7`.
- `storage.c` uses a 4096-byte sector with a 256-byte header area, so `ST_PAYLOAD_MAX = 3840`; the current A/B object store cannot hold a larger project object without a wider storage design.
- `editor_backup.c` accepts only the current `sizeof(project_store_t)` and known older project lengths. The web backup implementation `web/fm1backup.js` independently limits non-sample objects to 3840 bytes.
- User preset records reserve 144 bytes for parameter values (`UP_PMAX * 2`), so user presets have more parameter headroom than projects. Project capacity is the immediate bottleneck.

Consequence: adding every independent OSC / FM / FOLD / LPG control as a common parameter is not possible within the current FUN10 project layout. Do not increase `P_COUNT` beyond the four remaining slots without a versioned persistence design. Do not silently compress independent controls into macros or volatile state.

## 2. Existing storage and backup path

- Project and user-preset objects use the existing A/B sector commit format in `storage.c`.
- The project backup path captures and validates the serialized project before restore. The web backup format has a 3840-byte maximum for ordinary project/settings objects.
- A project extension therefore requires coordinated changes to firmware storage, project serialization/load, backup protocol, and the web editor's archive validation. Updating only `P_COUNT` or only the project structure would be unsafe and incompatible.

## 3. XDATA findings and restrictions

Pinned BSP source `chip/fm1_flash.h` defines:

- XDATA0: `0x93000–0x97000`.
- XDATA1: `0xE7000–0xFC000`.

However:

- Felucca `storage_hw.c` currently permits `FL_STORE_OK` plus only the FM6 voice-bank sector `0xE7000–0xE8000`. It does not permit general XDATA writes.
- The candidate range `0xE8000–0xF2000` is not approved. No complete ownership proof has been established.
- The update loader scans update records in `[0x93000, 0xFC000)`. At each 4 KiB sector boundary it checks the record at offset `0xF00`; a valid record can cause that sector to be erased. Any future XDATA format must preserve bytes `0xF00–0xF07` in every affected sector so they cannot be mistaken for an update record.
- The browser emulator's current flash persistence allowlist does not include XDATA above `0xE8000`; an XDATA extension would not survive emulator reloads until that behavior is separately changed and tested.

Consequence: no XDATA address may be allocated and no XDATA write path may be added until all Felucca, BSP, and loader ownership is audited and the loader-record exclusion is tested.

## 4. Candidate read-only backup / recovery utility

The pinned BSP contains `tools/fm1_rescue.py`. Its documented behavior is relevant to the safety gate:

- It accepts only the exact M-VAVE FM-1 V15 package, checked against a pinned SHA-256.
- It checks chip key `0x980F`, flash type 3, and JEDEC ID `0x856014`; otherwise it stops before writing.
- It reads and saves the full 1 MiB flash before any optional restore.
- Without `--write`, it performs the check and backup only.
- If explicitly run with `--write`, it writes only sectors in `[0x4000, 0x93000)`, leaves the boot head and device-data area above `0x93000` untouched, then performs a full read-back comparison.
- The source comments say the protocol and write policy were verified on hardware. This is not yet a validation on the user's physical FM-1.

This is a promising candidate for obtaining the user's own full dump and a possible V15 recovery route, but it is not yet approved for execution on the user's device. The exact stock package, local script revision, UBOOT entry behavior, chip key, JEDEC ID, and resulting backup must be verified first. Do not use `--write` during the initial validation.

## 5. Required next steps

1. Finish searching the pinned Felucca, BSP, and loader trees for XDATA ownership, direct flash offsets, erase/program helpers, and loader record handling.
2. Verify the exact local `FM-1_015.fwsc` package against the rescue script's pinned SHA-256; do not substitute the V14 development kit or another device's dump.
3. Review the read-only rescue procedure and its Mac USB dependencies. If later approved and executed, preserve the resulting full 1 MiB dump as the user's own original backup and verify its hash before considering any firmware write.
4. Design the project-parameter extension and backup migration as a separate versioned feature. Keep the existing FUN10 reader and current A/B storage behavior intact.
5. Only after these gates are closed should the DSP implementation and target build proceed.

## Explicit non-goals

- No firmware flash or physical-device write.
- No XDATA allocation or XDATA write implementation.
- No changes to `P_COUNT`, `project.c`, `storage.c`, `storage_hw.c`, `editor_backup.c`, or `web/fm1backup.js`.
- No change to the approved OSCILLATORS → FM → FOLD → LPG → OUTPUT signal order.
- No claim that the rescue utility has been validated on the user's individual FM-1.
