# ENERGUN FM-1 CORE — CP1.6 / CP1.7 persistence lifecycle call-site map

Date: 2026-11-03  
Branch: `energun-metal-core`  
Status: source audit and host-test scope; no production lifecycle hooks changed by this document.

## 1. Project identities (CP1.6)

The pair coordinator must sit at the existing persistence boundary, not in UI handlers. The current source still uses direct Felucca Base-only calls.

| Lifecycle operation | Current source location | Existing Base operation | Required pair operation |
|---|---|---|---|
| Save / Save As | `project_save_as()` in `firmware/src/project.c` | `st_save(OBJ_PROJECT0 + slot, ...)` | Capture FUN10-compatible Base + four-part P_MC/Private/Motion Sidecar; commit to project identity index `slot` |
| Rename stored slot | `project_rename()` | `st_save(OBJ_PROJECT0 + slot, ...)` | Read stored pair first, change only Base name, re-commit the unchanged Sidecar payload against the new Base generation |
| Erase stored slot | `project_erase()` | zero-length `st_save()` | Pair coordinator Base tombstone; stale Sidecar becomes orphan and cannot reappear |
| Load stored slot | `project_load()` / `proj_fetch()` | `st_load()` through fetch path | Load exact Base + matching identity/generation/CRC Sidecar; if no Sidecar match, preserve legacy Base load and apply V1 defaults |
| Boot project scan | `persist_boot()` | `proj_fetch()` for empty RAM slots | Populate each slot from Base; restore Sidecar only when it matches the selected Base fingerprint |
| Live autosave write | `autosave_poll()` | `st_save(OBJ_AUTOSAVE,...)` | Commit Live Autosave identity 0 as a Base + Sidecar pair; preserve FM6 inactive-copy reservation |
| Live autosave restore | `autosave_boot()` | `st_load(OBJ_AUTOSAVE,...)` | Load pair; missing/invalid Sidecar must not block Base restore and must apply deterministic defaults |

### Project-specific invariants

- FUN10 remains the legacy-compatible Base representation. The Base adapter must keep the final eight E0–E7 values in the legacy final-eight positions; P_MC values, Private controls and P_MC Motion belong to Sidecar.
- Legacy project objects with no matching Sidecar remain loadable. Apply deterministic V1 defaults atomically; never attach a stale Sidecar from another generation or slot.
- Rename is a stored-object operation, not an implicit save of current unsaved performance state. It must load the stored extension, change the stored Base name, and preserve the extension exactly.
- Save As creates a new destination identity. Its Sidecar must be independently committed and must never alias the source slot's A/B history.
- Erase uses the Base tombstone. Do not physically erase Sidecar in place.
- Pair persistence must preserve existing transport-stopped rules and avoid clearing dirty state for edits made after the snapshot.

## 2. User-preset bank identities (CP1.7)

Current source is `firmware/src/upreset.c`:

| Lifecycle operation | Current source | Existing Base operation | Required pair operation |
|---|---|---|---|
| Boot bank 0/1 | `up_boot()` | `st_load(OBJ_UPRESET0 + b,...)` | Load bank Base + matching Sidecar identity kind PRESET_BANK, index `b`; missing Sidecar gives defaults |
| Save / overwrite slot | `up_put(k,r)` | Mutate RAM bank then `st_save(OBJ_UPRESET0 + k/UP_PER_BANK,...)` | Update the selected slot's 13 Private values and bitmap in the bank Sidecar; pair commit the full bank |
| Rename slot | `up_rename_cat()` → `up_put()` | Rewrites bank Base | Preserve that slot's Private data while updating Base name/category and rebind generation |
| Delete slot | `up_put(k,NULL)` | Clear record then rewrite bank Base | Clear corresponding Private data and bitmap, then pair commit |
| Load slot sound | `up_load(k)` | Reads RAM bank record | Apply selected slot's validated Private cache together with Base sound parameters |

- Each preset-bank Sidecar payload is 210 bytes: 2-byte slot bitmap + 16 × 13 Private bytes.
- P_MC common values are already represented by the preset Base record under its native parameter count; preset-bank Sidecar is for per-slot Private values only.
- Motion is not stored in preset-bank Sidecar.
- A bank mutation is atomic at bank granularity: never commit a new Base bank with stale/missing Private data for unrelated slots.

## 3. Required adapter boundaries before runtime integration

1. **Base codec adapter:** exact FUN10 pack/import, legacy E0–E7 compaction, Base fingerprint metadata, and Base-only legacy fallback.
2. **Project extension codec:** four tracks × 16 P_MC values, four tracks × 13 Private values, and P_MC Motion count/events. Exact 501-byte maximum payload per CP1.17.
3. **Preset extension codec:** 16 slot bitmap + 16 × 13 Private values, 210-byte payload per bank.
4. **Runtime cache boundary:** snapshot/restore of P_MC and Private state without persisting DSP working state; generation-aware dirty handling.
5. **Coordinator backend:** real Sidecar A/B read/write callbacks. No target Flash address is authorized until the complete 1 MiB FM-1 V1.5 dump and recovery gate are satisfied.

Do not call the generic pair coordinator directly from UI code. Project and preset wrappers must own the codecs, legacy fallback, runtime snapshot and RAM rollback behavior.

## 4. Current implementation boundary

Implemented:
- Backend-neutral Base + Sidecar coordinator with commit, pair load and tombstone erase.
- Host-only seven-identity A/B history, pair roundtrip, torn-write, orphan retry and corrupt-newest fallback tests.
- Host test for the lifecycle semantics of Rename (same Sidecar payload rebound to a new Base generation) and Save As (independent destination identity).

Not implemented:
- Production project/preset codecs and runtime caches.
- Production Sidecar backend and safe physical storage allocation.
- Project/autosave/preset call-site replacement.
- Archive V2 pair-aware backup/restore.

No production lifecycle hook, Flash map, whitelist, loader, OTA or factory code is modified by this audit. No hardware was flashed.
