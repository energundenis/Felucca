# ENERGUN FM-1 CORE — CP1.4.2 parameter encoding and compatibility audit

Date: 2026-11-02
Branch: `energun-metal-core`
Status: source-backed compatibility audit; no parameter IDs or firmware/storage formats changed.

## 1. Source pins

- `firmware/src/core.h` — `c66f259d48f970aaa206e4e77818a15fb634db4f`
- `firmware/src/params.c` — `b6d2ab64419be763fcbe4c1be70ef7188af693`
- `firmware/src/project.c` — `c972d6e3a289b93f20b632007869b3b8e009e929`
- `firmware/src/upreset.c` — `4766bf6e7f29d6bafbdc3288e5045fe293dfece5`
- `firmware/src/editor.c` — `3ab73de336e4afa0b33c6c20647b9225dc0db573`
- `firmware/src/motion.c` — `43fbec779d94b95c8cae587aec98b269c13557f9`

## 2. Parameter ID namespace and limits

Verified current state:
- `P_COUNT=111`, `P_E0=103`; the final eight values are E0..E7.
- Project/preset migration relies on the final-eight rule: the first `np - 8` values are common parameters and the last eight are engine EDIT values.
- `P_COUNT < 128` is mandatory because bit 7 of the motion event parameter byte is `MOTION_LOCK`; the lower seven bits hold the parameter ID.
- The 8/16/13 proposal's 16 P_MC values inserted immediately before E0 would yield `P_COUNT=127`, `P_E0=119`, E0..E7 = 119..126. This fits, but consumes all available common-ID headroom. No MCX IDs may be placed in this namespace.
- This numeric allocation is a capacity calculation only. Do not assign final IDs while the E0..E7 conflict remains unresolved.

## 3. Project compatibility

### Existing migration behavior

- `params_by_count()` maps old project values by count and preserves the final eight engine values at today's E0..E7 positions.
- `proj_motion_ids()` applies the corresponding move to motion-event IDs: for a stored project with `np < P_COUNT`, IDs at or above `np - 8` are shifted to the new E0 base while preserving the MOTION_LOCK bit.
- `proj_unpack()` rejects `np > P_COUNT`; older firmware therefore rejects a project whose stored parameter count is 127 when its own `P_COUNT` is 111.
- FUN10 is fixed at 3840 bytes. The current layout has only three spare bytes. Sixteen P_MC values across four tracks require 64 packed bytes, so they cannot be added to the existing FUN10 payload.

### Compatibility consequence

A naive implementation that writes all 127 runtime parameters into FUN10 would both overflow the fixed project object and make the project unreadable by the factory firmware.

**Recommended compatibility architecture for review:** keep the base FUN10 project payload at the legacy-compatible parameter count (111 for the current factory baseline), preserving all existing common values and the final eight E0..E7 values. Store the 16 P_MC values and 13 Private values in a separate sidecar linked to the exact main-project identity/generation. On load, the new firmware loads the base FUN10 object first (P_MC defaults are supplied by normal count migration), then applies a matching sidecar. Older factory firmware ignores the unknown sidecar and can still load the base project. This requires an explicit project pack/unpack adapter and tests; it is not implemented or approved as code yet.

This approach preserves old-firmware readability of the base musical project while making the Metal Core extension unavailable to old firmware. The extension values remain preserved in the sidecar/archive and become active again under the new firmware.

## 4. User-preset compatibility and missing extension scope

Verified:
- There are 32 user preset slots in two persisted banks (`UP_PER_BANK=16`, `UP_SLOTS=32`), stored as `OBJ_UPRESET0` and `OBJ_UPRESET1`.
- Each preset record reserves 144 packed parameter bytes; the compile-time guard allows `P_COUNT <= 144` and `P_COUNT < 128`. A 127-value P array therefore fits the parameter area.
- User-preset migration also preserves the last eight E values by count. An older firmware can ignore newly inserted common values while retaining the final eight engine values if the record version/encoding remains compatible.
- The 13 Private controls are not represented by the normal P_COUNT array and are not currently stored by the user-preset parameter serializer.

Required consequence:
- The sidecar/archive design must include Private values for all 32 user presets, not only the four stored projects and live autosave.
- There are two logical user-preset bank identities, each requiring an A/B extension pair: four additional logical sidecar copies.
- Therefore the current logical sidecar scope is potentially **seven identities × A/B = 14 logical sectors**: five project identities (four slots + live autosave) and two user-preset banks. This is a capacity model, not a physical Flash allocation.
- Each preset's Private bank must remain paired with its preset slot and bank generation. A preset overwrite, rename, delete, bank restore or partial bank update must not attach stale Private data to another preset.
- New firmware loading old preset banks must default missing Private values deterministically. Old firmware must continue to read the base preset bank; it ignores the extension sidecar.

## 5. Editor protocol

- `ED_INFO` reports `P_COUNT` and `P_E0` dynamically.
- `ED_DUMP` iterates all `P_COUNT` values.
- `ED_GET/ED_SET` and `ED_TRACK_PARAM` are generic over IDs below `P_COUNT`; P_MC values can use the existing path once descriptors and IDs are deliberately added.
- The current descriptor path resolves common IDs through `TP[]` and E0..E7 through engine descriptors.
- No current command exposes the 13 Private sidecar values. They require a versioned extension protocol or explicit new commands; do not overload ordinary parameter scopes or IDs.
- Editor clients must be checked for dynamic P_COUNT handling and 7-bit ID assumptions before increasing the announced count to 127.

## 6. Motion and sidecar interaction

- P_MC IDs inserted before E0 require the explicit whitelist described in CP1.4.3/4.4 audit.
- The normal motion pool remains 128 records total across all four tracks. P_MC records stored in a sidecar must still count against that same global runtime limit.
- Recommended save/load behavior: split P_MC motion records from the base FUN10 motion block into the sidecar, but enforce a combined count no greater than 128. At runtime, merge them into one validated motion store. Reject/default the extension safely if the combined count exceeds 128, IDs are invalid, or duplicate (track, step, ID) records occur.
- The existing project migration of legacy E0..E7 motion IDs must run on the base project before matching P_MC sidecar records are merged.
- Do not create a second independent 128-event pool for P_MC; that would silently double the supported project automation capacity.

## 7. Required tests before CP1.4.2 closure

1. Old factory FUN10 (np=111) loads in new firmware with common values and E0..E7 unchanged; P_MC/private values default when sidecar is absent.
2. New firmware saves a base-compatible FUN10 (np=111) plus matching sidecar; old firmware can load the base project and ignores the sidecar.
3. New firmware reloads the same project and restores all P_MC/private values from the exact matching sidecar.
4. Old user-preset banks load with P_MC defaults and Private defaults; all existing common/E values remain unchanged.
5. New user-preset save/load round-trips 16 P_MC + 13 Private values per preset across both banks.
6. Older firmware reads the base user-preset bank without misplacing E0..E7; sidecar-only Private values are ignored.
7. Project/preset migration tests cover all supported historical counts and preserve E0..E7 values and motion IDs.
8. P_COUNT=127 boundary tests; reject 128 and all IDs that collide with MOTION_LOCK bit 7.
9. Editor ED_INFO/ED_DUMP/ED_TRACK_PARAM compatibility tests at P_COUNT=127 and with older clients.
10. Motion merge/split tests preserve the global 128-record limit, lock bit, values, duplicate rejection and round-trip.
11. Backup archive v2 round-trips all five project identities and both preset banks; legacy archive v1 restores defaults and cannot attach stale extension data.
12. Interrupted restore leaves each main project/preset bank paired with either its previous valid extension or its new valid extension.

## 8. Status

- Namespace and count limits: source-verified.
- Project and user-preset count migration: source-verified.
- Editor transport for common P_MC: source path verified; client compatibility tests not run.
- Old-firmware project downgrade behavior: source-verified; base-compatible FUN10 strategy recommended but not implemented.
- User-preset Private persistence: missing from current five-project sidecar model; scope expanded to include two preset-bank identities.
- Stable IDs, final E0..E7 allocation, binary encodings and migration code: **not assigned / not implemented**.
- CP1.4.2 remains OPEN pending explicit acceptance of the compatibility strategy and regression evidence.

## 9. Safety

This audit changes documentation only. No firmware source, parameter ID, project/preset binary format, Sidecar, archive protocol, Flash map, factory image or physical device was changed. No PR was merged and no device write was performed.
