# ENERGUN METAL CORE — Persistent Data Format 01

**Status:** design draft / review required.  
**Target:** M-VAVE FM-1, Felucca v1.7.0 base.  
**Scope:** persistent Metal Core engine parameters associated with a Felucca project or autosave image.  
**No firmware implementation or flash write is authorized by this document.**

## 1. Design decisions

1. Felucca's native project/preset system remains the owner of tracks, patterns, MIDI, effects, mixer and global state.
2. Metal Core's engine-specific controls are stored in a sidecar object because the current project format exposes only eight generic engine edit values per track, insufficient for the proposed control set.
3. Sidecar data is per project slot and per track. It is applied only when that track selects the Metal Core engine.
4. The sidecar is bound to the exact serialized project image by CRC-32. A sidecar for a different project image must never be applied.
5. Sidecar A/B copies are evaluated independently. On load, select a valid copy whose project CRC matches the loaded project; do not blindly select the highest sequence number. This preserves the old matching copy if a new sidecar was written but the corresponding project save failed.
6. FOLLOW and METAL are included as independent per-track persistent flags. They are not aliases for FOLD, STEP FOLD MOD or FOLD CV.
7. This is a format proposal, not a claim that the proposed flash addresses are safe or available.

## 2. Parameter schema — version 1

All 18 controls below are stored for each track. Parameter IDs are stable and must not be reused for a different meaning.

| ID | Section | Parameter | Encoding proposal |
|---:|---|---|---|
| 0x0001 | OSC | OSC1 waveform | enum |
| 0x0002 | OSC | OSC2 waveform | enum |
| 0x0003 | OSC | OSC2 coarse tune | signed semitones |
| 0x0004 | OSC | OSC2 fine tune | signed cents |
| 0x0005 | MIX | OSC1 level | normalized unsigned |
| 0x0006 | MIX | OSC2 direct level | normalized unsigned |
| 0x0007 | FM | FM amount | normalized unsigned |
| 0x0008 | FM | FM ratio | fixed-point ratio; exact scale TBD |
| 0x0009 | FM | FM feedback | normalized unsigned |
| 0x000A | FOLD | Fold amount | normalized unsigned |
| 0x000B | FOLD | Symmetry | normalized signed |
| 0x000C | FOLD | Drive | normalized unsigned |
| 0x000D | FOLD | Module volume | normalized unsigned |
| 0x000E | FOLD | Fold mode | enum: SOFT / CLASSIC / HARD |
| 0x000F | LPG | Cutoff | normalized unsigned; DSP mapping TBD |
| 0x0010 | LPG | Resonance | normalized unsigned |
| 0x0011 | LPG | Decay | normalized unsigned |
| 0x0012 | LPG | Response | normalized unsigned |

Track flags are separate from the parameter array:
- bit 0: FOLLOW enabled
- bit 1: METAL enabled
- bits 2–15: reserved; write zero, ignore on read

**Important:** normalized ranges, enum numeric values, fixed-point scales and conversion to/from UI values must be frozen in a separate parameter-mapping table after the DSP/UI audit. Do not serialize UI knob positions as if they were already canonical DSP values.

## 3. Logical record

One sidecar record represents one project slot and contains all four track records.

### Header (32 bytes, little-endian)

| Offset | Type | Field |
|---:|---|---|
| 0 | char[4] | magic = ENMC |
| 4 | u16 | format_version = 1 |
| 6 | u16 | header_size = 32 |
| 8 | u16 | payload_size |
| 10 | u8 | project_slot (0–3) |
| 11 | u8 | flags; zero in version 1 |
| 12 | u32 | sequence number |
| 16 | u32 | exact packed project image CRC-32 |
| 20 | u32 | payload CRC-32 |
| 24 | u32 | header CRC-32, calculated with this field zeroed |
| 28 | u32 | reserved; write zero |

### Track payload

Four fixed-size records, ordered PART 1 through PART 4. Each track record is 44 bytes:

| Offset | Type | Field |
|---:|---|---|
| 0 | u16 | engine_id |
| 2 | u16 | parameter_schema_version = 1 |
| 4 | u16 | FOLLOW/METAL flags |
| 6 | u16 | parameter_count = 18 |
| 8 | u16[18] | values in ascending parameter-ID order |

Total payload: 4 × 44 = 176 bytes. Total logical record: 208 bytes.

The engine ID must be taken from Felucca's actual engine registry; its numeric value is intentionally not guessed here. A track record is only applied when the saved engine ID matches the current Metal Core engine ID. Unknown engine IDs or schema versions are not reinterpreted.

### CRC definition

Use CRC-32/ISO-HDLC (reflected polynomial 0xEDB88320, init 0xFFFFFFFF, xor-out 0xFFFFFFFF), unless the existing Felucca CRC helper is proven to implement the identical variant. The implementation must reuse the existing helper when compatible. CRC coverage:
- payload CRC: exactly the 176 payload bytes;
- header CRC: exactly 32 header bytes with bytes 24–27 zeroed;
- project CRC: exactly the packed project image bytes and length produced by the existing project serializer.

## 4. Flash placement — candidate only

The BSP header defines XDATA ranges:
- XDATA0: 0x93000–0x97000 (16 KiB)
- XDATA1: 0xE7000–0xFC000 (84 KiB)

The sector at 0xE7000 is already used by the FM6 voice bank. A **candidate reservation** discussed for sidecars is 0xE8000–0xF2000 (10 × 4 KiB sectors = 40 KiB):
- four project A/B pairs: 8 sectors;
- autosave A/B pair: 2 sectors.

This reservation is **not verified free space**. Before any implementation or write, audit every user of XDATA0/XDATA1, the complete bootloader/firmware flash map, board flash capacity and the exact target hardware revision. Do not infer that an address is safe merely because the current storage map does not name it.

The loader uses a special update-record signature at byte offsets 0xF00–0xF07 of each 4 KiB sector. Firmware must leave those eight bytes erased/untouched in every XDATA sector. The existing A/B storage object format must not be copied unchanged unless its writer is modified to preserve these reserved bytes. Implement a dedicated XDATA sidecar backend with explicit bounds checks and read-back verification.

The current storage hardware whitelist does not generally authorize all XDATA addresses. Any extension to that whitelist is a separate, reviewed change and must not weaken existing bounds checks.

## 5. Save, load and failure behavior

### Project save

1. Serialize the future project image into the existing project wire buffer using the normal project serializer.
2. Compute its exact length and project CRC.
3. Build the new sidecar record referencing that CRC.
4. Write the inactive sidecar copy; preserve loader-reserved bytes; read it back and validate header, payload and CRCs.
5. Save the project through the existing A/B project storage path.
6. If sidecar write/verification fails, abort the project save. If project save fails after sidecar succeeds, the previous sidecar copy remains available and the loader must search both copies for one matching the still-current project CRC.

Do not update the in-memory current-saved state until both operations report success.

### Project load

1. Load and validate the project through the existing path.
2. Compute CRC over the exact loaded packed image.
3. Read both sidecar copies for that slot.
4. Validate magic, versions, sizes, slot, header CRC and payload CRC.
5. Select a valid record whose project CRC matches. If both match, select the newer sequence using wrap-safe sequence comparison.
6. Apply only records whose engine ID matches Metal Core and whose schema is supported.
7. If no record matches, load documented Metal Core defaults. Never apply a stale or mismatched sidecar.

### Other lifecycle operations

- **Save As / duplicate:** create a sidecar for the destination slot using the destination project's exact serialized bytes; copy source Metal Core values only when the source sidecar validates against the source image.
- **Rename:** if serialized project bytes change, rewrite the sidecar against the new project CRC. If rename only changes external metadata, verify that assumption in code before skipping the rewrite.
- **Erase:** erase/invalidate the project first; clear sidecar copies only after successful project erase. A stale sidecar must not resurrect data for an empty slot.
- **Autosave:** use a separate A/B sidecar pair bound to the exact autosave image. Audit the existing autosave capture/restore call chain before wiring it in.
- **Backup/restore:** include the sidecar object in the editor backup protocol. Restore must validate the project and sidecar pair before committing; a missing or mismatched sidecar means safe defaults, not partial application.
- **Engine switch:** retain stored Metal Core values in the sidecar when another engine is selected; apply them only when Metal Core is selected again.

## 6. Power-loss and compatibility rules

- Each sidecar copy is independently validatable and updated using inactive-copy / verify / commit semantics.
- Never invalidate the previous copy before the new copy is verified.
- A project/sidecar mismatch is a safe-default condition, not a fatal project-load error.
- Unknown future format/schema versions must not be overwritten by an older firmware build unless the user explicitly initializes that data.
- Missing parameters in a future migration receive explicit defaults; unknown parameter IDs are ignored only when the record structure and CRC are valid.
- Existing FUN10 project layout and stock Felucca objects remain unchanged in version 1.
- Do not allocate new voice-state meanings or change the shared voice ABI as part of persistence work.

## 7. Required source and hardware audit before implementation

- [ ] Confirm exact target hardware revision and flash JEDEC ID/capacity.
- [ ] Enumerate every read/write user of XDATA0 and XDATA1 across Felucca, BSP, bootloader, rescue/OTA and FM6 voice-bank code.
- [ ] Confirm all candidate sectors and loader-reserved bytes on the target.
- [ ] Confirm existing CRC helper variant.
- [ ] Confirm engine registry ID and parameter/UI/DSP conversions.
- [ ] Audit project save, Save As, rename, erase and load call paths.
- [ ] Audit autosave capture, write, restore and power-loss behavior.
- [ ] Extend editor backup/restore format without dropping existing object types.
- [ ] Add host tests for CRC, malformed records, version mismatch, slot mismatch, stale CRC, sequence wrap and A/B fallback.
- [ ] Add simulated power-loss tests at every write boundary.
- [ ] Measure target flash operation behavior and verify no realtime audio path performs flash I/O.
- [ ] Build reproducibly from the pinned commits and verify recovery before any device flashing.

## 8. Explicit non-goals for this draft

- No firmware code changes.
- No flash writes or firmware flashing.
- No changes to stock project format or existing Felucca preset format.
- No new preset manager or browser Patch Bay port.
- No change to the four-part / eight-voice architecture.
- No claims of verified free flash capacity.

## 9. Review items

This draft is ready for design review, not implementation. The following must be settled before coding:
1. Confirm the 18-parameter list and the inclusion of FOLLOW and METAL as persistent per-track flags.
2. Freeze canonical parameter ranges, enum values and fixed-point scales.
3. Complete the XDATA ownership and hardware audit; only then choose final sector addresses.
4. Confirm whether the project CRC should bind the full packed image or a narrower canonical project identity. Full-image CRC is safest against stale association but requires rewriting the sidecar whenever any serialized project content changes.
5. Define exact backup file compatibility/versioning for the sidecar object.
