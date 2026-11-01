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
