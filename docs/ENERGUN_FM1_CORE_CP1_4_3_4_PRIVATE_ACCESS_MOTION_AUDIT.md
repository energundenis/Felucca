# ENERGUN FM-1 CORE — CP1.4.3 / CP1.4.4 Private access and Motion Lock source audit

Date: 2026-11-02
Branch: `energun-metal-core`
Status: source-backed architecture audit; no firmware source, parameter IDs or storage format changed.

## 1. Scope and source pins

This audit checks the current Felucca implementation path for the approved 8 Engine EDIT + 16 common P_MC + 13 Private logical allocation.

Source files and blob SHAs:
- `firmware/src/core.h` — `c66f259d48f970aaa206e4e77818a15fb634db4f`
- `firmware/src/params.c` — `b6d2ab64419be763fcbe4c1be70ef7188af693`
- `firmware/src/motion.c` — `43fbec779d94b95c8cae587aec98b269c13557f9`
- `firmware/src/project.c` — `c972d6e3a289b93f20b632007869b3b8e009e929`
- `firmware/src/upreset.c` — `4766bf6e7f29d6bafbdc3288e5045fe293dfece5`
- `firmware/src/eng_metal_core.c` — `427c6181b41ef47868072330671e2b1918b2246a`

## 2. CP1.4.3 — Private parameter access/UI

### Verified host constraints

1. `track_t` stores common and engine edit values in `int16_t p[P_COUNT]`. The engine interface exposes exactly eight `edit[8]` descriptors.
2. `track_desc()` dispatches E0–E7 to the selected engine's descriptor. The optional engine `desc(track, k)` hook can supply a mode-dependent descriptor/name for one of those eight values; it does not add more parameter slots.
3. The page table in `params.c` is static. EDIT 1 contains E0–E3 and EDIT 2 contains E4–E7. There is no registered METAL CORE private-parameter page or generic engine-private accessor in the current production path.
4. `engine_t.mem` is runtime DSP memory owned by the current engine. Felucca's engine memory allocator may zero this region when another memory-owning engine claims the part. It is not persistent patch storage and must not contain the 13 Private values.
5. The current METAL CORE file is an isolated scaffold and is not registered in `engines.c`; its own comments explicitly defer parameter mapping and UI pages.

### Required architecture

- Keep the 13 Private values in a versioned per-part extension/sidecar, separate from `track_t.p[]` and `engine_t.mem`.
- Provide a per-part runtime cache with deterministic defaults, explicit load/replace semantics and dirty tracking. It must be owned by the project/track layer, not by the transient engine-memory allocator.
- Provide typed get/set accessors keyed by part and Private logical key. Access must remain valid while the part changes engine, and values must survive engine switching.
- Add an engine-specific UI page family that reads/writes the selected part's private cache. Reuse Felucca's page/navigation conventions, but do not pretend these values are E0–E7 descriptors.
- Connect project load/save, live autosave, user preset load/save and archive backup/restore to the same versioned extension unit.
- Missing, old or invalid extension data must yield deterministic V1 defaults and must not prevent the base FUN project from loading.
- UI writes update the runtime cache and mark the correct project identity/generation dirty; persistence must not be performed from the audio ISR.

### CP1.4.3 status

**API/UI design requirements identified; implementation not started.** No private IDs, pages or binary layout are assigned by this audit.

## 3. CP1.4.4 — P_MC Motion Lock

### Verified current behavior

- `motion_param(id)` accepts selected common ranges and then `id >= P_E0`. It does not have a dedicated P_MC range.
- The CP1.4 proposal inserts 16 P_MC common parameters immediately before E0. Therefore, after insertion, those 16 IDs are below P_E0 and will **not** pass the current `id >= P_E0` clause.
- `motion_put()` obtains a parameter descriptor through `param_desc_of()`, checks descriptor min/max and the signed event range, then stores a compact event.
- Motion event IDs use 7 bits; bit 7 is `MOTION_LOCK`. Valid parameter IDs must remain below 128.
- `MOTION_MAX` is 128 records shared across the complete project/four tracks. It is not 128 per track. Automation and Parameter Lock records share the same pool.
- Motion values are stored as signed int8 in the range -64..127. This is a storage representation constraint; each P_MC descriptor's own range still governs accepted values.
- Project FUN10 has a fixed 3840-byte layout and the current source documents only three spare bytes. A full 16-parameter P_MC extension cannot be added to FUN10's main object; the accepted sidecar design must also carry P_MC values and any P_MC motion events without silently overflowing the base project.
- User-preset records have 144 packed parameter bytes and require `P_COUNT < 128`. Current `P_COUNT` is 111 and `P_E0` is 103; adding 16 common values before E0 would make `P_E0=119` and `P_COUNT=127`, exactly consuming the currently available common-ID headroom.

### Required implementation and tests

1. Add an explicit P_MC whitelist interval to `motion_param()`; do not broaden the rule to every ID below E0.
2. Ensure every P_MC descriptor is continuous and its range can be represented in the event's signed int8 domain. If a logical range needs scaling, define and test a reversible mapping before implementation.
3. Audit all motion paths: validation, insert/update, record capture, restore, step application, lock release, project load/save and editor protocol.
4. Preserve the global 128-record cap. Test mixed events across all four tracks and deterministic full-pool rejection without corrupting existing events.
5. Test one event per (track, step, parameter), automation and lock replacement, chance-skipped lock behavior, lock restore to the latest automation/base value, and project round-trip.
6. Keep Private values, booleans, enums and MCX extension values excluded from the V1 P_MC whitelist.
7. Do not assign final P_MC numeric IDs until CP1.4.1 E0–E7 allocation conflict and CP1.4.2 compatibility audit are closed.

### CP1.4.4 status

**Source gap confirmed; implementation and regression not started.** The exact whitelist and event mapping remain design work, not an approved code change.

## 4. Cross-check: persistence boundaries

- Project and user-preset migration intentionally preserves the final eight values as E0–E7 when common parameters are inserted before E0.
- `P_COUNT=127` would satisfy the current 7-bit parameter-ID limit, but leaves no additional common IDs available in the current namespace.
- The user-preset parameter byte capacity is sufficient for 127 values; this does not prove project FUN10 capacity.
- Sidecar must be the persistence unit for the 13 Private values and any P_MC motion records moved outside FUN10.
- No physical Flash address/sector is selected by this audit. Under the user's 2026-11-02 confirmation, the factory firmware image already held by the project is the exact image for this unit; verify that file's integrity and the restore path at the pre-write checkpoint, without creating a duplicate read stream.

## 5. Acceptance boundary

This audit confirms the host-source constraints and identifies the smallest architecture gaps. It does not:
- add P_MC IDs or a Private runtime cache;
- add UI pages or editor-protocol messages;
- change `motion_param()` or any production source;
- alter project, preset, Sidecar or archive formats;
- register METAL CORE or merge any PR;
- write to physical FM-1 or Flash.

**CP1.4.3 and CP1.4.4 remain OPEN.**

## 6. Subsequent CP1.4.1 logical reconciliation — 2026-11-02

The earlier source audit's symbolic P_MC candidate table reflected the later candidate E ordering and is superseded for membership purposes by `ENERGUN_FM1_CORE_CP1_4_1_E0_E7_RECONCILIATION.md`.

The controlling E0–E7 freeze remains:
- E0 FREQ; E1 OSC2 LEVEL; E2 TIMBRE; E3 WEST COAST DRIVE;
- E4 METAL RATIO; E5 FOLD; E6 CUTOFF; E7 RESONANCE.

The reconciled P_MC set removes OSC2 LEVEL and TIMBRE (already E1/E2) and includes FM INDEX and LPG DECAY instead. The 16-control whitelist and value-domain candidate are updated in `ENERGUN_FM1_CORE_CP1_4_3_4_PRIVATE_MOTION_IMPLEMENTATION_DESIGN_DRAFT.md`.

This resolves the logical overlap only. It does not assign numeric IDs, change `motion_param()`, implement the whitelist, or alter firmware/storage/editor source. CP1.4.3/4.4 implementation and regression remain open.


## 7. Implementation reconciliation — 2026-11-04

The original audit's statement that the Private cache/bridge implementation had not started is superseded by the current host-testable modules:

- firmware/src/energun_private_v1.h provides a typed 4-part × 13-key runtime cache with canonical defaults, per-part generation, dirty masks, typed get/set, snapshot/compare-and-replace and generation-safe persistence acknowledgement.
- firmware/src/energun_private_payload_v1.h bridges all four project/live parts and individual preset slots to the Sidecar payload codecs. Capture/restore validates before publishing cache changes.
- tests/energun_private_v1_test.c and tests/private_payload_v1_test.c exercise cache validation, stale-generation rejection, all four parts, all 13 keys, atomic project restore and preset-slot isolation.
- The Sidecar V1 validator has an explicit candidate P_MC motion range 103–118 in firmware/src/energun_sidecar_v1.h. This is Sidecar payload validation only; it does not change core.h parameter count or assign live Felucca parameter IDs.

**Revised CP1.4.3 status:** typed cache and payload bridge primitives are implemented and host-tested. Production project.c, upreset.c, autosave and editor/UI call-site integration remains open. The current source still does not provide a production Private page/protocol or connect the cache to real project/preset load/save paths.

**Revised CP1.4.4 status:** the Sidecar semantic validator's P_MC candidate whitelist is host-tested. The production motion_param() path remains unchanged and still uses the existing Felucca parameter namespace. Do not wire the 103–118 candidate into live Motion Lock until the controlling parameter conversion/compatibility decision is accepted and the production event migration/restore path is tested. Private/discrete values and MCX remain excluded.

Fast Regression [run 41796239006](https://github.com/energundenis/Felucca/actions/runs/41796239006) passed on exact code/test HEAD 404a51ab03171fe893721ce17665ff461625734f and includes the Private cache/bridge tests among 24 focused programs/suites. This confirms host behavior only; the production call sites and target storage backend are not connected.

No stable live parameter IDs, engine IDs, project ABI, physical Flash allocation or device write is authorized by this reconciliation.
