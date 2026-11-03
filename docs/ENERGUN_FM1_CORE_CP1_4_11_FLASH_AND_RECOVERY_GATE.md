# ENERGUN FM-1 CORE — CP1.4.11 Flash map, bootloader and recovery gate audit

Date: 2026-11-03  
Branch: `energun-metal-core`  
Status: **SOURCE AUDIT COMPLETE / PHYSICAL ALLOCATION BLOCKED — no Flash map or firmware source changed**

## 1. Scope

Reconcile the current Felucca storage and update-loader source against the frozen Sidecar V1 logical requirement. This audit is read-only. It does not select or authorize a Sidecar sector, change a Flash whitelist, write physical Flash, or validate a recovery operation on an individual FM-1.

Primary source evidence:
- `firmware/src/storage.c` — object identities and A/B sector mapping.
- `firmware/src/storage_hw.c` — current firmware-side erase/program allowlist.
- `firmware/loader/ldr_core.c` — update-loader scan and reserved record region.
- `firmware/src/project.c`, `upreset.c`, `fm6_vbank.c` — project, preset and FM6 staging ownership.
- `docs/ENERGUN_METAL_CORE_STORAGE_RECOVERY_AUDIT_01.md`.
- `docs/ENERGUN_METAL_CORE_SIDECAR_STORAGE_TRANSACTION_AUDIT_01.md`.
- `docs/ENERGUN_FM1_CORE_CP1_4_6_SIDECAR_CAPACITY_AUDIT.md`.

## 2. Source-derived address map

All ranges below are half-open and sector-aligned unless a tail is explicitly stated.

| Range | Current owner / behavior | Sidecar decision |
|---|---|---|
| `0x00000–0x92FFF` | Firmware/application area; loader writes app only in `[0x4000,0x93000)` | Never allocate |
| `0x93000–0x96FFF` | XDATA0 / update-loader record scan window | Not an allocation candidate |
| `0x97000–0x9EFFF` | Four project objects, 2 × 4 KiB A/B each | Occupied |
| `0x9F000–0x9FFFF` | Shared FM6 user-preset patch / retired FM6 bank A copy | Occupied |
| `0xDC000–0xDFFFF` | Two user-preset banks, 2 × 4 KiB A/B each | Occupied |
| `0xE0000–0xE4FFF` | OTA staging, 5 sectors | Occupied |
| `0xE5000–0xE6FFF` | Live autosave A/B; also temporarily used by FM6 voice-bank transfer staging | Occupied / shared staging |
| `0xE7000–0xE7FFF` | FM6 voice-bank home sector | Occupied |
| `0xE8000–0xE8FFF` | Stock update record is documented at `0xE8F00` | Exclude entire sector |
| `0xE9000–0xFBFFF` | XDATA1 range scanned by update-loader record handling; not currently exposed by Felucca's storage allowlist | Candidate region only; not approved |
| `0xFC000–0xFFFFF` | Global/settings objects, including shared FM6 patch B copy | Occupied |

The loader's update-record candidate is 80 bytes at each scanned 4 KiB sector's `+0xF00` offset. The conservative rule is to reserve the entire final 256 bytes (`+0xF00.. +0xFFF)) of every sector in the scanned XDATA window. This reserve does not make the rest of a sector safe or free.

The loader scans `[0x93000,0x97000)` and `[0xE0000,0xFC000)`; it explicitly skips Felucca's main store `[0x97000,0xE0000)`. The stock update record at `0xE8F00` confirms why sector `0xE8000` must remain excluded.

## 3. Reconciled Sidecar capacity

The frozen Sidecar V1 schema has seven logical identities:
- four stored project slots;
- one live-autosave identity;
- two user-preset banks.

Each identity needs an A/B pair. Therefore the complete V1 requirement is **14 logical 4 KiB copies = 57,344 bytes (56 KiB)**, not merely the ten copies required by project/live identities alone.

| Identity class | Count | A/B sectors each | Total sectors |
|---|---:|---:|---:|
| Stored projects | 4 | 2 | 8 |
| Live autosave | 1 | 2 | 2 |
| Preset banks | 2 | 2 | 4 |
| **Total** | **7** | | **14** |

Payload requirements:
- Project/live: 501 bytes maximum, including 4 × 16 P_MC bytes, 4 × 13 Private bytes and up to 128 shared-pool Motion events.
- Preset bank: 210 bytes maximum, including 16 × 13 Private bytes and a 2-byte presence bitmap.
- The frozen V1 envelope is 20 bytes; the full largest project/live record is 521 bytes. The enclosing Felucca A/B storage header is separate.

A contiguous 14-sector sizing illustration would be `0xE9000–0xF6FFF` (end-exclusive `0xF7000`). **This is not an allocation proposal or approval.** It is only arithmetic showing that the candidate region is large enough by count; every sector remains inside the update-loader scan window and outside Felucca's current `ST_OK` whitelist.

## 4. Current storage allowlist and ownership blockers

`firmware/src/storage_hw.c` currently permits:
1. the existing `FL_STORE_OK` main/settings/user-preset regions; and
2. the single FM6 voice-bank home sector `0xE7000–0xE7FFF`.

It does not expose a general XDATA1 Sidecar storage API. The low-level flash driver range check is not an authorization to use a new sector. A future implementation would require an explicit narrow allowlist, audio-safe erase/program wrapper and proof that update-loader reserved tails remain erased.

FM6 voice-bank transfer staging temporarily occupies one autosave A/B copy. `autosave_hold()` is a delay policy, not a hard lock. The live-autosave pair writer must share an explicit reservation/ownership contract with FM6B_BEGIN/WRITE/END and must never erase a staging-owned sector.

## 5. Runtime integration source audit

Current production paths still write only the base objects:

- Project Save / Save As and Rename write `OBJ_PROJECT0 + slot` through `st_save()`.
- Project Erase commits an empty base object through `st_save()`; it has no Sidecar tombstone.
- Project Load and live Autosave boot restore only the base project.
- Live Autosave writes `OBJ_AUTOSAVE` directly.
- Preset Save / Overwrite / Rename / Delete mutate the in-memory bank and write `OBJ_UPRESET0 + bank` through `st_save()`.
- Preset Load applies the base record only; it does not restore a bank Sidecar.
- FM6 preset patch persistence remains a separate object/transaction.

Consequently CP1.6 and CP1.7 runtime integration are not implemented. No production Sidecar object ID, `OBJ_COUNT` extension or storage pair coordinator is present.

## 6. Candidate XDATA region — mandatory per-device gate

The source-derived candidate for capacity is the XDATA1 interval `0xE9000–0xFBFFF`, subject to:
- excluding the full `0xE8000` stock update-record sector;
- reserving `+0xF00.. +0xFFF` in every sector that remains inside the loader's scan window;
- confirming no device-specific data, samples, calibration, recovery metadata or other user content is present;
- confirming exact SPL/loader behavior and the full 1 MiB address map on the user's own FM-1 V1.5 unit.

**The user's own full 1 MiB V1.5 dump is not present in the repository evidence inspected for this audit.** A dump from another FM-1 is not a substitute. Until the user's dump is obtained, hashed, backed up and compared, CP1.4.11 cannot approve any physical Sidecar sector.

The existing read-only rescue utility is only a candidate route. Its pinned stock-package hash, local tool revision, Mac USB dependencies, chip key, JEDEC ID and read-back behavior must be checked before any invocation. The initial validation must remain read-only; no `--write` operation is part of this work.

## 7. Recovery and archive boundary

- No bootloader, SPL, OTA or factory image changes are needed or authorized for the logical schema.
- Archive V1 remains unchanged and does not contain Sidecar identities.
- Archive V1 restore must explicitly default/invalidate destination extension state once Sidecar storage exists; otherwise stale Sidecar data could attach to a restored base.
- Archive V2 must validate every base+Sidecar pair before the first destructive write, restore Sidecar before its matching base generation, and restore live autosave last.
- No recovery operation has been run on a physical device. No individual-device recovery route is claimed as verified.

## 8. CP1.4.11 gate result

- [x] Reconcile current Felucca project, autosave, preset, FM6 and settings sector ownership.
- [x] Reconcile update-loader scan window, stock update-record sector and reserved 256-byte tails.
- [x] Correct Sidecar capacity to seven identities / fourteen A/B sectors.
- [x] Confirm current firmware storage allowlist does not yet permit a Sidecar XDATA1 region.
- [x] Confirm project and preset runtime call paths are not Sidecar-integrated.
- [ ] Obtain the user's own full 1 MiB FM-1 V1.5 dump; verify SHA-256 and retain an immutable backup.
- [ ] Compare every candidate sector and loader/SPL record location against that exact dump.
- [ ] Verify the exact V1.5 stock package and read-only rescue route on the intended Mac/USB setup.
- [ ] Approve an exact Sidecar sector map and reserved-tail policy.
- [ ] Only after approval, implement the target storage whitelist and hardware backend.

**Result: source audit completed; physical allocation remains BLOCKED pending the user's own full-device dump and recovery verification.** This is not a physical Flash safety sign-off.

## 9. Next safe engineering step

Continue CP1.6/CP1.7 work in a host-only transaction harness that includes the real `firmware/src/storage.c`, reuses its simulated NOR and fault-injection hooks, and supplies a separate test-only Sidecar A/B backend. Exercise all seven logical identities, protected-old-pair behavior, torn Sidecar/Base writes, fallback, erase/reuse, bank preservation and FM6 staging reservation. Do not connect that test backend to target Flash or add production object IDs until the per-device gate above is closed.

## 10. Explicit non-goals

No firmware source, storage allowlist, object IDs, `OBJ_COUNT`, Flash address, archive V1, bootloader, OTA, factory image, physical dump, physical device or hardware state changed by this audit.
