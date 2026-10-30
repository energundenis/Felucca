# ENERGUN METAL CORE — UI and sidecar feasibility audit 02

Date: 2026-10-31
Branch: energun-metal-core
Status: source-backed architecture review; no firmware source or storage code changed.

## 1. Confirmed UI architecture

The current Felucca UI supports engine-specific page visibility through the static PAGES[] table and page_visible(). Existing examples include ANALOG FILTER, DRUM LANES, FM6 operator pages, and SLICE pages.

The current parameter page model supports:
- SC_TRACK pages backed by track_t.p[id] and common TP[] descriptors;
- SC_ENGINE pages backed by the engine's eight edit values P_E0..P_E7;
- engine-specific visibility conditions in page_visible();
- four controls per page, with the existing EDIT 1 and EDIT 2 pages showing E0..E3 and E4..E7.

This is sufficient to add several METAL CORE-specific SC_TRACK pages for new common P_MC_* IDs. They can be visible only when the selected engine is METAL CORE, like existing specialized pages. Their labels, ranges, units and defaults can live in the common parameter descriptor table.

The UI does not currently have a third generic storage scope for sidecar-only controls. A further seven controls cannot simply be inserted into PAGES[] as ordinary parameter IDs without extending the UI parameter binding and editor protocol.

## 2. Corrected parameter architecture

The V1 inventory currently contains 35 independent controls. The four normal ADSR controls are conditional reuse of P_ATK/P_DEC/P_SUS/P_REL, leaving 31 engine-specific controls to persist.

The architecture has three distinct storage groups:

A. Eight engine EDIT values (E0..E7)
- Already part of track_t.p[] and existing project/preset serialization.
- Already have generic editor descriptors, two four-control EDIT pages, modulation-matrix destinations E1..E8, and motion/parameter-lock support.
- Can hold eight METAL CORE controls without adding common IDs.
- The exact eight assignments remain pending UI and sound-control priority review.

B. Up to sixteen new common P_MC_* values
- These are inserted immediately before P_E0, preserving P_COUNT <= 127 and the 7-bit motion ID limit.
- With current P_COUNT=111 and P_E0=103, the maximum is 16 new IDs: P_COUNT=127, P_E0=119, E0..E7=119..126.
- These values use ordinary SC_TRACK pages, TP[] descriptors, and can use existing automation/parameter-lock machinery after motion_param() is explicitly updated to allow the new P_MC range.
- They are not persisted in the current FUN10 main object: its 3840-byte format has only three unused bytes, insufficient for one 4-byte per-track parameter.
- Therefore all new P_MC values must be serialized in the project sidecar, not copied into the main FUN10 object.

C. Seven sidecar-only extended controls
- After eight E values and sixteen P_MC IDs, seven of the 31 engine-specific controls remain.
- They cannot be represented as ordinary P_COUNT IDs while preserving the 7-bit lock encoding and P_COUNT <=127.
- They need a separate Metal Core extension parameter namespace and UI scope, backed by sidecar RAM values.
- Add dedicated editor read/write/change notifications for the extension scope; do not alias them onto existing P_* IDs.
- If V1 requires motion/parameter-lock automation for these seven controls, sidecar must carry an extended event namespace and the sequencer's runtime motion engine must merge/apply/restore these events without setting MOTION_LOCK bit 7 on a normal 7-bit ID. This is a separate feature to design and test, not something the current motion_store can represent directly.
- A simpler V1 fallback is to persist these seven values in the sidecar but defer their per-step motion/locks. This trade-off needs explicit approval if chosen.

## 3. Sidecar scope and transaction

The previously proposed sidecar remains the preferred storage design, with an important clarification: it stores all sixteen new P_MC values plus the seven extended values for each of four tracks, and their extension motion data if that feature is implemented. E0..E7 remain in the main project object.

Sidecar must:
- be A/B committed per project slot;
- include project slot, schema version, generation, exact main-project sequence and payload CRC;
- include the four tracks' P_MC values and seven extension values;
- optionally include extended motion/lock records with a distinct ID namespace;
- use payload/header CRCs and commit header last;
- load only when it matches the selected main project generation;
- default all extension controls if absent/invalid without blocking the main project;
- be included in live-project and stored-project backup/restore;
- be invalidated/cleared when a project slot is erased or replaced.

Save order remains:
1. write and verify inactive sidecar copy;
2. commit matching main project A/B object;
3. on load, select main project first and accept only the sidecar matching its sequence and payload CRC.

The browser emulator's full 1 MiB NOR persistence is suitable for exercising this transaction, but sidecar-specific power-loss and corruption tests must be added.

## 4. Flash allocation status

Candidate range 0xE9000..0xF1000 (eight sectors for four slots × A/B) remains unapproved. The update loader scans the surrounding XDATA range and checks sector offset 0xF00; keep each candidate sector's last 256 bytes erased.

Felucca storage_hw.c currently permits the main storage areas plus only the FM6 bank sector 0xE7000..0xE8000. It does not currently permit the candidate sidecar sectors. A dedicated explicit whitelist and storage API would be required.

No candidate sector may be enabled or written until a full 1 MiB dump from the user's own FM-1 V1.5 has been obtained and checked for device-specific data. A dump from another FM-1 is not a substitute.

## 5. Backup and editor protocol consequences

Current editor parameter transport is generic over P_COUNT. Increasing P_COUNT to 127 will increase its ordinary parameter read/write/watch payloads, but the editor protocol already announces P_COUNT and P_E0 in ED_INFO. Host/editor compatibility must be versioned and tested.

Sidecar-only extended controls require new protocol operations or an explicit extension to existing commands; do not overload ordinary scope 0/1 or IDs 0..126.

Current web/fm1backup.js supports backup archive version 1 and fixed object IDs/sizes. It does not include sidecar objects. A versioned archive extension is required to include:
- the live project's extension state;
- the four stored project slots' matching sidecars;
- integrity metadata and generation matching;
- safe restore order (sidecar first, matching main project commit second).

Existing archives without sidecars must continue to restore the base project with extension defaults.

## 6. UI recommendation

Do not create one enormous METAL CORE page. Use engine-specific pages of four controls each:
- OSCILLATORS / VOICE
- COMPLEX FM
- METAL
- WAVEFOLDER
- FILTER
- LPG
- EXTENDED (only if the seven sidecar-only controls require a separate page/scope)

FOLLOW and METAL remain independent switches in OSCILLATORS / SYNTH VOICE, matching the original v98 placement.

The existing EDIT 1/EDIT 2 pages remain generic engine EDIT pages. They may display eight carefully selected high-priority METAL controls, but the complete V1 interface should use dedicated pages backed by P_MC and extension scopes. Avoid duplicating the same control on EDIT and a dedicated page unless the duplicate is intentional and synchronized.

## 7. Next decision gate

Before code or IDs:
1. Approve whether the seven extended controls need V1 per-step motion/parameter-lock support, or whether that can be deferred while their values remain persistent.
2. Review the exact 8 E-slot and 16 P_MC assignments against the control inventory and UI layout.
3. Define the sidecar-only extension namespace and editor protocol.
4. Define backup archive v2 and restore behavior.
5. Obtain and inspect the user's own full V1.5 dump before any XDATA allocation.

No source code, parameter IDs, UI pages, project format, backup format, flash map, firmware image, factory firmware, or physical device were changed.
