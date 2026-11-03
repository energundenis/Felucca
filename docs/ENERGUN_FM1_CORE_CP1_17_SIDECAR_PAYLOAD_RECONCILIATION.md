# ENERGUN FM-1 CORE — CP1.17 sidecar payload reconciliation

Date: 2026-11-01
Branch: `energun-metal-core`
Status: source-document audit; no firmware or storage implementation changed.

## 1. Finding

The current sidecar documents do not describe one consistent payload.

Three existing documents define overlapping but different contents:

1. `ENERGUN_METAL_CORE_CP1_4_PARAMETER_ACCESS_STORAGE_PROPOSAL.md` and CP1.12 define 13 private V1 sound controls per track, in addition to 16 proposed common P_MC controls.
2. `ENERGUN_METAL_CORE_PARAMETER_STORAGE_PLAN_01.md` says the sidecar contains the four tracks' P_MC values, but does not enumerate the 13 private controls.
3. `ENERGUN_METAL_CORE_SIDECAR_STORAGE_TRANSACTION_AUDIT_01.md` lists four tracks × 16 P_MC values and four tracks × 7 MCX extension values, but again omits the 13 private controls.

The sidecar transaction audit correctly supersedes the earlier eight-sector estimate with five logical project identities × A/B = ten sectors. However, its logical payload list is incomplete relative to the approved 37-control V1 inventory and the 8/16/13 access proposal.

## 2. V1 payload that must be represented

If the 8/16/13 proposal is eventually approved, each project identity needs storage for:

- 16 common P_MC values × 4 tracks;
- 13 private V1 values × 4 tracks;
- P_MC motion/parameter-lock events, subject to the existing motion count and validity rules.

The 13 private controls are:
WAVE, FOLLOW, METAL, MODULATION mode, MOD OSC WAVE, FM CHARACTER, COMPLEX MODULE VOLUME, FOLD MODE, LPG MODULE VOLUME, ATTACK, DECAY, SUSTAIN and RELEASE.

CP1.14 has since resolved VCA ownership as an engine-owned V98-compatible envelope with no host-ADSR cascade. Therefore the four normal VCA ADSR controls cannot be silently removed from the private V1 list by assuming host ADSR reuse. Any change to this mapping requires an explicit new decision.

## 3. Keep MCX separate

The seven MCX0..MCX6 extension values mentioned by the transaction audit are not part of the 37-control V1 inventory or the 13 private controls listed above.

They must remain a separately identified extension namespace until their purpose, ownership, user-visible editing, persistence need and motion-lock behavior are documented. In particular:
- do not count MCX values as substitutes for the 13 private V1 controls;
- do not silently include MCX in V1 payload sizing;
- do not place MCX IDs into the normal 7-bit parameter-lock ID space;
- retain the separate decision on whether MCX motion events are required.

## 4. Correct logical sidecar contents — proposed reconciliation

For each of the five logical project identities (four stored slots plus live autosave), the V1 sidecar schema should be specified to include:

1. Schema / identity / A-B / generation / commit metadata.
2. Four tracks × 16 P_MC common values, if the 8/16/13 access map is approved.
3. Four tracks × 13 private V1 controls, if the 8/16/13 access map is approved.
4. Motion events whose parameter IDs refer to P_MC, split from the base FUN10 motion block.
5. MCX0..MCX6 values and MCX motion events only in a separately versioned optional extension section, after MCX scope is approved.

This is a logical schema proposal, not a byte layout. Do not infer field widths, packing, payload length, sector fit, or archive object IDs until parameter encoding and MCX scope are frozen.

## 5. Capacity and implementation consequences

- The ten-sector / 40 KiB capacity calculation is a sector-count result, not proof that the current payload schema is complete or that Flash sectors are available.
- The earlier storage plan's eight-sector candidate range is obsolete; the transaction audit's ten-sector sizing supersedes it.
- The user's complete FM-1 V1.5 dump and a proven recovery route remain mandatory before any physical sector selection or write.
- The host/emulator storage model must include all five identities, A/B rollover, project-generation pairing, P_MC values/events, private V1 values, optional MCX isolation, backup-v1 defaults, backup-v2 round-trip, interrupted restore, corruption and fuzz cases.
- Archive v2 must validate every main project and its complete extension before any destructive restore write.

## 6. Required closure decisions

Before the sidecar format can be frozen:
1. Approve or revise the 8/16/13 access split.
2. Confirm that all 13 private controls are stored per track and define the editing path for them.
3. Decide whether MCX0..MCX6 are in V1 at all; if not, explicitly exclude them from the V1 sidecar schema.
4. Decide whether MCX motion is required, independently of P_MC motion.
5. Define value encoding, schema versioning and archive-v2 object identity only after the above.

## 7. Safety and status

- This is a documentation-only reconciliation.
- No parameter IDs, engine IDs, editor pages, sidecar bytes, archive IDs, Flash addresses or sector whitelists were assigned.
- No firmware source, emulator, factory firmware or physical device was changed.
- `main` remains untouched.
- CP1 remains OPEN.


## 8. Accepted V1 ownership and payload closure — 2026-11-03

This addendum reconciles CP1.17 against the adopted CP1.4.1 logical map and the existing Sidecar V1 encoder/validator. It closes the logical ownership question for V1. It does not assign stable firmware parameter IDs, approve physical Flash sectors, or authorize device writes.

### 8.1 Control ownership

The controlling V1 allocation is **8 Engine EDIT + 16 common P_MC + 13 Private per part**:

- E0–E7 remain the existing eight Engine EDIT values and retain their existing Base-project Motion representation.
- P_MC is the exact sixteen-control order in `ENERGUN_METAL_CORE_CP1_4_1_FROZEN_PARAMETER_MAP.md`, section 4. Each of four parts has sixteen values.
- Private is the exact thirteen-control order in that map, section 5. Each of four parts has an independent bank (52 values total).
- Private values are not stored in volatile engine DSP memory.
- FOLLOW and METAL remain separate persistent booleans. Disabling either never clears other patch values.
- MCX0–MCX6 are excluded from Sidecar V1. They remain a separately versioned future extension; MCX values and MCX Motion events are both deferred.

### 8.2 Motion ownership — resolved for V1

Motion ownership is now explicit:

1. Existing E0–E7 events remain in the unchanged Base-project Motion block.
2. Sidecar V1 owns Motion Lock events for the sixteen P_MC controls only.
3. Each Sidecar Motion event stores a logical P_MC index in the low seven bits of its parameter byte (0–15); bit 7 retains the existing Motion Lock flag. This is a schema-local index, not a stable global parameter ID.
4. Event values use the canonical Felucca signed value domain -64..127. The selected P_MC descriptor performs the same conversion used by edit and persistence paths.
5. Duplicate (track/step, P_MC index) events are invalid. The combined Base + Sidecar event count must not exceed the existing 128-event pool.
6. The thirteen Private values, including booleans/enums and Private envelope controls, are not Motion Lock or modulation-matrix destinations in V1.
7. MCX0–MCX6 and MCX Motion are not represented in V1.

The Sidecar validator now enforces the P_MC namespace boundary: low-seven-bit logical indices 0–15 are accepted; index 16 or above is rejected, with or without the Motion Lock flag. The new regression checks protect this boundary.

### 8.3 Project payload layout

The existing Sidecar V1 project payload is reconciled to the frozen map:

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 64 bytes | Four parts × sixteen P_MC values, part-major and map-order |
| 64 | 52 bytes | Four parts × thirteen Private values, part-major and map-order |
| 116 | 1 byte | Sidecar Motion event count |
| 117 | 3 × count | Motion events: place, P_MC logical index + flag, signed value |

Therefore the fixed project prefix is 117 bytes and the maximum payload is 501 bytes (117 + 128 × 3). The 20-byte Sidecar envelope binds the logical identity and the exact Base sequence/CRC. The outer A/B storage object supplies its own storage sequence, CRC and header-last commit semantics. A Sidecar is optional: a missing, corrupt, or mismatched Sidecar must never prevent a valid Base project from loading; Private/P_MC state falls back to deterministic V1 defaults.

The Base FUN10 serialized object layout is not expanded to carry these 16 P_MC values. Project runtime integration must keep the legacy Base byte layout and serialization contract intact, and obtain P_MC/Private values from the matching Sidecar. Existing E0–E7 and Base Motion remain Base-owned.

### 8.4 User-preset payload ownership

The existing Sidecar V1 preset-bank payload is 210 bytes:

- two-byte presence bitmap for sixteen records in the selected bank;
- thirteen Private values for each of the sixteen preset slots (208 bytes).

The common P_MC values remain part of the existing user-preset parameter record, subject to the CP1.6/CP1.7 compatibility adapter and the approved common-parameter conversion path. The Sidecar preset bank carries only the thirteen Private values per preset. An absent preset slot must have all thirteen Private bytes zeroed; stale Private state cannot survive slot deletion/reuse. Preset Sidecar data is optional and must default safely if absent or invalid.

### 8.5 Encoding and identity boundaries

- P_MC order, types, ranges, defaults and semantic distinctions are taken from the frozen CP1.4.1 map.
- P_MC values use the accepted standard Felucca 7-bit descriptor representation; FOLD SYMMETRY uses the signed -64..63 descriptor range. The current Sidecar project validator enforces those bounds for all four parts.
- Private enum ordinals and 7-bit values follow the frozen map and current V1 semantic validator.
- The envelope's project identities remain four stored projects plus live autosave. Preset identities remain two banks. A/B copy selection and Base generation pairing remain separate from the logical payload.
- No MCX values, MCX events, high-resolution values, engine IDs, stable global P_MC IDs, archive-v2 object IDs, or physical Flash addresses are introduced by this closure.

### 8.6 Closure and remaining implementation gates

**CP1.17 logical ownership and V1 payload scope: CLOSED.**

The following are deliberately not claimed complete:

- stable P_MC numeric IDs and runtime descriptor registration;
- exact UI/editor read/write routing for Private values;
- project runtime binding, safe snapshot and generation pairing;
- preset runtime binding and archive-v2 backup/restore;
- full Base + Sidecar power-cut and migration qualification;
- full firmware target link/runtime qualification;
- full FM-1 V1.5 dump and physical recovery audit.

No physical Flash allocation/write, factory firmware modification, firmware installation, device connection, or PR merge is authorized.
