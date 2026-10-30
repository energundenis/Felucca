# ENERGUN METAL CORE — Sidecar Extension and motion design 01

Date: 2026-10-31
Branch: energun-metal-core
Status: design proposal based on current Felucca source; no firmware code changed.

## 1. Design decision

Keep the existing Felucca motion store and its FUN10 representation unchanged. Add a separate, bounded Metal Core extension state and extension-motion store. Do not widen the existing motion ID byte, change MOTION_LOCK, or enlarge/rewrite FUN10.

The seven extension parameters use a distinct namespace:
- ordinary parameters: existing P_* IDs, including the proposed P_MC_* range;
- extension parameters: MCX0..MCX6, local IDs 0..6, never passed as ordinary P_COUNT IDs.

The extension namespace is only active for the ENERGUN METAL CORE engine. Other engines neither display nor consume these values/events.

## 2. Why a separate motion store is necessary

Verified current implementation:
- motion_event_t is a compact 4-byte record: place (track + 64-step index), param (7-bit P_* ID plus MOTION_LOCK in bit 7), value (signed byte).
- MOTION_MAX is 128 records shared by all four tracks.
- motion_param(), motion_put(), motion_step(), motion_unlock(), motion_mask(), and editor MOTION operations assume every ID is below P_COUNT and resolve descriptors through param_desc_of(engine,id).
- motion_step applies values to track_t.p[id]. Parameter locks restore the last automation value or saved patch value on the next step.
- FUN10 serializes the existing motion_store as part of its fixed 3840-byte object. The project serializer/importer validates existing event IDs against P_COUNT.
- Existing editor protocol commands 64+ expose the normal motion event pool and use the 7-bit ID + lock-bit representation.

The seven extension values do not have P_COUNT IDs and must not be smuggled into this path.

## 3. Extension event format

Proposal: a separate mc_motion_store_t, with its own 128-record maximum and count/on fields.

Each record can reuse the 4-byte physical layout because it is in a separate namespace:
- place: (track_index << 6) | step_index, same 0..255 range as current motion;
- param: extension ID 0..6 in bits 0..6; MOTION_LOCK in bit 7;
- value: signed 8-bit value, checked against the MCX descriptor range.

This is not an extension of the normal P_* ID namespace. The same low ID can safely mean MCX0 in this separate store and a different P_* ID in the normal store.

Semantics should match Felucca exactly:
- Automation event: applies at its step and holds until the next automation event, loop restart, or stop.
- Parameter Lock: applies only if the step plays (chance passed), holds through ratchets, and restores at the next step to the latest automation value up to that step, or to the saved MCX patch value if no automation exists.
- Editing/deleting one event replaces/deletes either kind for that track/step/extension ID.
- Clear locks removes only MCX Parameter Locks; clear track removes that track's MCX events.
- Event validation rejects duplicate track/step/ID entries, out-of-range values, invalid track/step/ID, and counts above the fixed capacity.

Keep separate per-track runtime state:
- mcx_base[4][7];
- active and currently-locked bit masks for seven IDs per track;
- base-valid flags.
The sequencer step handler calls the MCX motion step alongside normal motion, only for the METAL CORE engine. At stop, loop restart, engine change, project load and preset load, extension values must be restored/rebased consistently. Never write MCX values into track_t.p[].

## 4. Capacity and real-time budget

Initial proposal: 128 MCX events per project, shared by all four tracks, in addition to the existing 128 normal events. This adds at most 512 bytes of event payload per project sidecar and keeps the format bounded.

Important: this doubles the maximum number of sparse event records the sequencer may inspect on a step. The existing motion path scans at most 128 records. Before enabling 128 MCX events on hardware, benchmark worst-case step processing, lock restore and editor redraw on the AC791N. If timing margin is insufficient, lower MCX capacity (e.g. 64) or use an indexed per-track/per-step structure. Do not silently increase ISR work.

The 4-byte event format is a proposal, not a claim that a second full 128-record scan is already safe.

## 5. Extension descriptors and UI

Add a dedicated seven-entry MCX descriptor table for label, format, min, max, default, unit and enum names. Descriptor lookup must be separate from param_desc_of().

Add a distinct UI page scope, for example SC_MCX, whose value pointers resolve to the selected track's MCX state rather than track_t.p[]. Page visibility is restricted to ENERGUN METAL CORE.

The normal UI must be able to:
- draw and edit MCX values;
- display correct labels/ranges/units;
- mark changed values for editor sync;
- expose the seven controls to the AUTOMATION page's parameter selector;
- edit automation and locks using the separate MCX event APIs;
- show lock/event markers on the step grid;
- undo/redo or restore the previous value/event set without aliasing ordinary parameters.

Do not add MCX controls to the existing modulation matrix in this phase. That would require a separate modulation destination namespace and engine-side modulation routing, not merely parameter storage.

## 6. Editor protocol proposal

Do not overload ED_GET/ED_SET scopes 0/1 or ordinary IDs. Add an explicitly versioned MCX capability and command family.

Minimum operations:
- MCX_INFO: extension schema version, number of controls, event capacity and selected-engine availability;
- MCX_GET / MCX_SET: selected track + MCX ID + value;
- MCX_DESC: descriptor metadata;
- MCX_DUMP: seven base values for selected track;
- MCX_MOTION: bounded list/get/set/delete/clear operations for MCX automation and locks;
- MCX_CHANGED: live push notification with track, ID and value;
- MCX_RELOAD: selected-track/engine change or project load notification.

The web editor must only expose these operations when the firmware advertises the capability. An older editor ignores unknown capabilities; an older firmware remains unaffected because no existing command meaning changes.

The exact command numbers, payload lengths, and schema negotiation belong in the protocol implementation plan and must be checked against the current web editor parser before coding.

## 7. Sidecar payload and integrity

A project sidecar contains:
- magic, schema version, header size;
- project identity: live/autosave or stored slot 0..3;
- generation counter;
- matching main-project sequence and payload CRC;
- four tracks × seven MCX base values;
- MCX event count and up to 128 4-byte MCX events;
- payload CRC and header CRC;
- commit marker written last.

The event pool is project-wide and track-indexed, matching normal motion's shared-pool model.

A missing or invalid sidecar must not prevent loading the main project. Load MCX defaults and no MCX events, and report a nonfatal extension-state warning. Never apply a sidecar whose project identity, sequence, CRC, or schema does not match the selected main project.

## 8. Critical storage correction: autosave needs its own sidecar

The previous eight-sector proposal covered only four stored projects × A/B. That is insufficient.

Verified current project flow:
- Four stored project slots use OBJ_PROJECT0..3.
- Live-project autosave is a separate OBJ_AUTOSAVE object, saved and loaded independently.
- A user can edit a project after loading it; those live unsaved changes are captured in autosave, not necessarily written back to the stored project slot.

Therefore a correct extension store needs five independent identities:
1. live/autosave state;
2. stored project slot 0;
3. stored project slot 1;
4. stored project slot 2;
5. stored project slot 3.

With A/B copies, this is ten 4-KB sectors, not eight. The earlier candidate range 0xE9000..0xF1000 (eight sectors) cannot hold the complete design. Do not use it as currently sized.

The live sidecar must be committed in sync with OBJ_AUTOSAVE and validated against the autosave project's sequence/CRC. Each stored-slot sidecar is committed in sync with its corresponding OBJ_PROJECT object. A missing live sidecar must default MCX extension state safely without corrupting or rejecting autosave.

No new Flash range is approved. Full 1 MiB dump from the user's own FM-1 V1.5 and update-loader map review remain mandatory before selecting ten sectors or any alternative.

## 9. Save, load and power-loss transaction

For each identity independently:
1. serialize the matching main project to a stable staging buffer and calculate its exact sequence/CRC;
2. write the inactive sidecar copy with that matching identity/sequence/CRC;
3. read back and verify sidecar bytes and CRC;
4. commit the main project object using Felucca's existing A/B storage mechanism;
5. update the in-RAM active generation only after both commits succeed.

On boot/load:
1. load and validate main project/autosave using existing code;
2. select only the sidecar copy matching its identity, sequence and CRC;
3. restore MCX values/events if valid; otherwise initialize MCX defaults and empty MCX motion.

Power-loss cases to test include interruption during sidecar erase, payload write, sidecar commit marker, main-project save, and after main-project commit. The invariant is: never pair extension data with a different main project generation.

The precise ordering must be reconciled with st_save()'s own A/B sequence/commit behavior before implementation; this proposal does not assume that a caller can control its internal commit marker.

## 10. Backup and restore

Current backup archive v1 has a fixed list of object IDs and includes the live project, settings, four stored project slots, user preset banks and FM6 patches. It does not include MCX sidecars.

A future archive v2 must add:
- live/autosave MCX sidecar;
- each of the four stored-slot MCX sidecars;
- schema and project-identity metadata;
- per-object length and CRC validation.

Restore must validate the complete archive before the first destructive write. Restore each main project and its matching sidecar as a logical pair; if a sidecar is absent in a legacy archive, explicitly clear/default the corresponding MCX extension state rather than leaving stale values from a previous project. A disconnect during restore must not allow an unrelated old sidecar to attach to the newly restored project.

Backup manifest and protocol object IDs must be versioned; do not silently change the meaning or count of v1 IDs. Browser emulator persistence must include all ten sidecar sectors and power-loss/corruption scenarios.

## 11. Open decisions before implementation

1. Confirm that V1 requires MCX automation and Parameter Lock, not only persistence. This document designs both but does not implement them.
2. Confirm whether 128 MCX events are needed; hardware timing benchmark decides whether 128, 64 or an indexed design is safe.
3. Agree the seven MCX control names/ranges/defaults from the V1 inventory.
4. Audit exact UI undo/redo, editor parser and sequencer step timing before assigning command numbers or implementing SC_MCX.
5. Obtain the user's own full V1.5 1 MiB dump and review the update-loader sector scan before selecting ten sidecar sectors.

## 12. Safety and change boundary

This is a design-only document. No source code, stable parameter IDs, UI pages, editor commands, motion formats, project formats, backup formats, Flash whitelist, firmware image, factory firmware or physical device were changed.
