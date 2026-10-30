# ENERGUN METAL CORE — storage and recovery audit 01

Date: 2026-10-31
Branch: `energun-metal-core`
Status: source audit only. No firmware source, storage format, flash contents, or hardware state changed.

## 1. What is already saved

Pinned Felucca source findings:

- Current project format is FUN10 / `FUNA`, fixed at 3840 bytes.
- Current common parameter count is `P_COUNT = 111`; engine parameters start at `P_E0 = 103`.
- The engine interface `engine_t` provides exactly eight editable values, `P_E0..P_E7`, on two pages of four. These values are already part of each track and are serialized in every project. They do not need a new storage mechanism.
- The same eight engine values are included in the engine's patch/preset data path. FOLLOW and METAL can therefore be persistent controls if they are assigned to two of the engine's eight parameter slots.
- The FUN10 format has only three unused bytes left in its reserved tail after the MIDI-IN-TRACK data. One additional per-track parameter needs four serialized bytes (one per track), so there is no complete spare common-parameter slot. Adding common parameters requires a versioned project-format change.
- `storage.c` stores a project in a 4096-byte sector with a 256-byte header area; `ST_PAYLOAD_MAX = 3840`. The current A/B project objects cannot simply be enlarged.
- `editor_backup.c` validates known project lengths; `web/fm1backup.js` validates ordinary project/settings objects up to 3840 bytes.
- User-preset records have more parameter headroom than projects, but that does not remove the project-format limit.

### Consequence

First, map the complete METAL CORE control set into the existing eight engine parameters if practical. FOLLOW and METAL use two slots, leaving six for other directly stored controls. Do not assume that the complete v98 control set fits until the exact v98 HTML parameter list is available and audited.

If the reference requires more than eight independently saved controls, design a separate per-project extension object rather than silently compressing values, reusing unrelated parameters, or keeping settings only in volatile RAM.

## 2. Existing project and backup path

- There are four project slots. Each uses the existing A/B sector commit format in `storage.c`.
- The current project backup path captures and validates the serialized project before restore.
- Any sidecar extension must be included in project save/load, clear, backup, and restore. It must be tied to the exact serialized project generation (for example, by storing the project payload CRC in the sidecar header), so an interrupted save cannot pair one project's extension with another project's data.
- A safe save order for a future sidecar is: commit the new sidecar to the inactive sidecar copy first, then commit the new main project to its inactive A/B copy. On load, accept only the sidecar whose stored project CRC matches the selected project. This leaves the old project/sidecar pair valid if power is lost before the main project commit.
- Backup protocol and web archive validation would need coordinated versioned changes if sidecars are introduced. Existing FUN10 projects and old archives must continue to load with extension defaults.

## 3. Flash map and update-loader constraints

Pinned BSP source `chip/fm1_flash.h`, `loader/ldr_core.c`, and Felucca's storage modules establish:

- XDATA0: `0x93000–0x97000` (four 4 KiB sectors).
- Felucca's main store: `0x97000–0xE0000`; the update loader explicitly skips this range when scanning for update records.
- OTA staging: `0xE0000–0xE5000`.
- Autosave A/B: `0xE5000–0xE7000`.
- FM6 voice bank: `0xE7000–0xE8000`; this sector is already occupied and its tail from offset `0xF00` is deliberately erased.
- XDATA1: `0xE7000–0xFC000`.
- Felucca global/settings storage begins at `0xFC000`.

Important update-loader behavior:

- The loader scans sectors in `[0x93000, 0x97000)` and `[0xE0000, 0xFC000)`; it skips Felucca's main store `[0x97000, 0xE0000)`.
- For each scanned 4 KiB sector it reads an 80-byte update-record candidate at sector offset `0xF00). If the record has the expected `AT` marker and a valid CRC, the loader erases that sector.
- The BSP comments identify a stock-firmware update record at absolute address `0xE8F00`. Therefore sector `0xE8000` is not an approved allocation target for METAL CORE data.
- Any future sidecar sector in the scanned range must keep the update-record area `sector + 0xF00 .. sector + 0xFFF` erased. Reserving the full final 256 bytes is the conservative design; do not write into that tail.
- `storage_hw.c` currently permits the main store plus only the FM6 voice-bank sector. It does not expose a general XDATA storage API. The low-level flash driver has a wider range check, but a dedicated, explicit whitelist and audio-safe erase/program wrapper would still be required.

### Candidate sidecar region — not yet approved for use

After excluding the FM6 bank sector `0xE7000` and the stock update-record sector `0xE8000`, the next candidate range is `0xE9000–0xFC000` (19 sectors, 76 KiB). This is only a source-derived candidate. Before allocating any sector, compare it against the user's own full 1 MiB V1.5 dump and verify that no device-specific data is present.

A possible later layout is four project slots × two A/B sectors = eight sectors, with the final 256 bytes of each sector reserved for the update loader. The sidecar payload would then be at most 3584 bytes per sector (header at the beginning, payload ending at `0xF00`). This is a design option, not an implemented or approved allocation.

## 4. Browser emulator persistence

The current emulator uses a full 1 MiB NOR image. `web/emu/felucca_web.c` accepts reads/erases/programs throughout that image, and `web/emu/index.html` saves all returned changed sectors in browser storage. There is no current XDATA address allowlist in this path. If a sidecar is implemented, the emulator still needs dedicated tests for its new storage API, power-cycle behavior, and A/B recovery; the general sector persistence itself is already present.

## 5. Candidate read-only backup / recovery utility

The pinned BSP contains `tools/fm1_rescue.py`. Its documented behavior is relevant to the safety gate:

- It accepts only the exact M-VAVE FM-1 V15 package, checked against a pinned SHA-256.
- It checks chip key `0x980F`, flash type 3, and JEDEC ID `0x856014`; otherwise it stops before writing.
- It reads and saves the full 1 MiB flash before any optional restore.
- Without `--write`, it performs the check and backup only.
- If explicitly run with `--write`, it writes only sectors in `[0x4000, 0x93000)`, leaves the boot head and device-data area above `0x93000` untouched, then performs a full read-back comparison.
- The source comments say the protocol and write policy were verified on hardware. This is not yet a validation on the user's individual physical FM-1.

This is a promising candidate for obtaining the user's own full dump and a possible V15 recovery route, but it is not yet approved for execution on the user's device. The exact stock package, local script revision, UBOOT entry behavior, chip key, JEDEC ID, and resulting backup must be verified first. Do not use `--write` during the initial validation.

## 6. Required next steps

1. Obtain and inspect the exact METAL CORE v98 HTML reference; enumerate every independently adjustable control and identify which six controls remain after FOLLOW and METAL occupy two of the eight engine slots.
2. Decide whether eight persistent engine controls are sufficient. Only if not, complete the sidecar design and its project-CRC linkage, backup format, restore ordering, and emulator power-cycle tests.
3. Finish checking the candidate XDATA sectors against a full dump from the user's own V1.5 device. Do not use another FM-1's dump as a substitute.
4. Verify the exact local `FM-1_015.fwsc` package against the rescue script's pinned SHA-256; do not substitute the V14 development kit or another device's dump.
5. Review the read-only rescue procedure and its Mac USB dependencies. If later approved and executed, preserve the resulting full 1 MiB dump as the user's own original backup and verify its hash before considering any firmware write.
6. Only after these gates are closed should the DSP implementation and target build proceed.

## Explicit non-goals

- No firmware flash or physical-device write.
- No XDATA allocation or XDATA write implementation.
- No changes to `P_COUNT`, `project.c`, `storage.c`, `storage_hw.c`, `editor_backup.c`, or `web/fm1backup.js`.
- No change to the approved OSCILLATORS → FM → FOLD → LPG → OUTPUT signal order.
- No claim that the rescue utility has been validated on the user's individual FM-1.
