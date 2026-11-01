# ENERGUN METAL CORE — CP1.4 parameter access and persistence architecture proposal

Date: 2026-10-31
Branch: energun-metal-core
Status: architecture proposal only; no IDs assigned and no firmware code changed.

## 1. Source-backed Felucca constraints

### Engine interface
The pinned Felucca-Engines interface `src/felucca_engine.h` defines:
- exactly eight engine edit descriptors: `engine_t.edit[8]`, shown on EDIT 1 / EDIT 2;
- an optional `desc(track, k)` hook that can change the label/names of one of those eight values, not add more values;
- `track_t.p[]` as the host parameter array;
- `note_on`, additive `render`, host voice lifecycle and optional per-part memory.

Therefore the current engine ABI does not expose 37 independent controls. E0..E7 alone are insufficient.

### Common parameter pages
The current `params.c` page table is static. Each page has four parameter IDs. `page_desc()` resolves common parameters through `TP[id]` and engine edit values through `engine_t.edit[]`. Adding new common descriptors and engine-specific pages is a supported extension pattern, subject to project/preset/motion migration.

### Motion and modulation
- `motion_param()` currently accepts common sound parameters by explicit ranges and accepts E0..E7 via `id >= P_E0`.
- If 16 new common IDs are inserted immediately before E0, they are below the new P_E0. The current motion whitelist will not automatically accept them. Add an explicit P_MC range and tests.
- The modulation matrix's destination list currently exposes E1..E8 (the eight engine edit values), not arbitrary common parameter IDs. The proposed common IDs can be parameter-locked after the whitelist is extended, but they will not become modulation-matrix destinations unless that UI/protocol is separately extended.
- Discrete toggles and enums should not be admitted to continuous automation/locks unless a specific safe behavior is designed.

### Persistence
- Project FUN10 and user-preset records serialize parameters by count; the last eight remain E0..E7. New common parameters must be inserted before E0 to preserve that migration rule.
- `UP_PMAX=72` gives 144 packed byte values, but `P_COUNT < 128` remains the ID limit because bit 7 is the motion-lock flag.
- FUN10 is 3840 bytes with only three spare bytes. No complete new common per-track parameter fits in its main object.
- The current complete archive list has fixed object IDs and does not yet include a Metal Core sidecar. Backup/restore must be extended as part of any sidecar implementation.

## 2. Proposed access split for the 37-control inventory

The following is a candidate allocation, not a final ID assignment. It preserves eight high-value modulation destinations, up to 16 common parameter IDs, and a private extension bank for the remaining values.

### A. E0..E7 — eight modulation / motion destinations

| Slot | Candidate control | Reason |
|---|---|---|
| E0 | FREQ | Master pitch relationship for all tracked oscillator sources |
| E1 | FM INDEX | Main Complex FM timbre depth |
| E2 | METAL RATIO | Main metallic sideband/ring frequency relationship |
| E3 | WEST COAST DRIVE | Shared FM/METAL depth scaler |
| E4 | FOLD | Main nonlinear amount |
| E5 | CUTOFF | Main filter tone |
| E6 | RESONANCE | Main filter character |
| E7 | LPG DECAY | Trigger-tail shaping |

These eight values would be visible on EDIT 1 / EDIT 2 and available as the existing E1..E8 modulation-matrix destinations. Their actual descriptor ranges/defaults must be mapped to the Felucca parameter representation and validated before implementation.

### B. Sixteen proposed common P_MC parameters

| Candidate common control | Reason for common ID |
|---|---|
| OSC2 LEVEL | Continuous source balance |
| OSC2 DETUNE | Continuous tuning control |
| TIMBRE | First-stage drive |
| MOD RATIO | Continuous Complex modulator ratio |
| AM DEPTH | Continuous AM depth |
| FM FEEDBACK | Continuous feedback amount |
| FM ENV AMT | Continuous FM transient amount |
| FM ENV ATTACK | Continuous envelope time |
| FM ENV DECAY | Continuous envelope time |
| METAL DECAY | Continuous METAL transient time |
| FOLD SYMMETRY | Continuous waveshaper shape |
| WAVEFOLDER DRIVE | Continuous second-stage drive |
| LPG RESPONSE | Continuous transient response |
| VOICE VCA LEVEL | Continuous triggered VCA peak |
| WAVEFOLDER MODULE VOLUME | Independent stage output gain |
| WEST COAST VOLUME | Independent final engine output gain |

This uses the full 16-ID ceiling if P_COUNT moves from 111 to 127 and P_E0 moves from 103 to 119. Do not implement this maximum without project/preset/motion migration tests and an explicit compatibility review.

### C. Private sidecar values — up to thirteen

| Control | Why not in the proposed E/common set |
|---|---|
| WAVE | Discrete oscillator waveform |
| FOLLOW | Independent pitch-follow switch |
| METAL | Independent METAL switch |
| MODULATION mode | FM / AM enum |
| MOD OSC WAVE | Discrete modulator waveform |
| FM CHARACTER | Three-way enum |
| COMPLEX MODULE VOLUME | Independent module gain; retained in private bank if the 16 common slots are used as above |
| FOLD MODE | SOFT / CLASSIC / HARD enum |
| LPG MODULE VOLUME | Independent module gain |
| ATTACK | Normal voice ADSR |
| DECAY | Normal voice ADSR |
| SUSTAIN | Normal voice ADSR |
| RELEASE | Normal voice ADSR |

If Felucca host P_ATK/P_DEC/P_SUS/P_REL pass the v98 parity tests, those four values can reuse the host ADSR and the private bank falls from 13 to 9. If parity fails, all four remain independent private values. Host ADSR reuse is conditional, not assumed.

FOLLOW and METAL must remain separate booleans and remain visible in OSCILLATORS / SYNTH VOICE. Their values must survive OFF state changes.

## 3. Required private-parameter interface

The current `engine_t` ABI has only `edit[8]`; private values cannot be displayed or edited merely by adding a field to the engine descriptor. A deliberate extension is required:
- per-track RAM representation for the private bank, populated from a validated sidecar on project/preset load;
- engine-facing read access that does not misuse `engine_t.mem` (which is volatile DSP state and is cleared on engine switches);
- engine-specific physical UI pages for private values, including enum labels, ranges, defaults and value formatting;
- edit/write callbacks that update the in-memory track extension and mark the sidecar dirty;
- save/load association with the correct project generation;
- preset save/load behavior, including whether the extension is embedded in a versioned preset archive or stored in a matching preset sidecar;
- deterministic defaults for old projects, old presets, missing/corrupt extension data and engine changes.

Do not store persistent controls inside `engine_t.mem`; that pool is runtime DSP state, not patch storage.

## 4. Parameter-lock and modulation policy

- E0..E7 remain the eight modulation-matrix destinations and can be motion-locked under existing rules.
- The proposed 16 P_MC common parameters require an explicit addition to `motion_param()`, because insertion before P_E0 places them below the existing `id >= P_E0` condition.
- Private sidecar values are not motion-lockable or modulation-matrix destinations in this proposal. If the user requires step locks for any of them, promote that control to a common ID or design an extended motion event format.
- FOLLOW, METAL and discrete mode selectors should not be motion-locked by default; abrupt per-step switching can alter oscillator routing and envelope selection.

## 5. Project, preset and backup compatibility

The current sidecar proposal is incomplete for the 37-control model unless it is extended to include:
- the 9 or 13 private values per track;
- association to each of four project slots and to the live project;
- user-preset save/load semantics;
- archive manifest IDs and versioning in `editor_backup.c` and `web/fm1backup.js`;
- project and user-preset migration tests for pre-extension records;
- A/B writes, CRCs, generation matching, interrupted-write tests, missing/corrupt sidecar fallback and clear/replace behavior.

The candidate XDATA sectors remain unapproved. Do not write to them until the full FM-1 V1.5 device dump is audited. The update loader's scan range and reserved final 256 bytes per sector remain hard constraints.

## 6. Main risks and decision gates

1. **UI/API gap:** 9 or 13 private values need a deliberate editor extension; current E0..E7 ABI cannot display them.
2. **Motion gap:** P_MC IDs inserted before E0 need an explicit motion whitelist update and tests.
3. **Backward compatibility:** P_COUNT=127 is within the declared project/preset numeric limits but consumes all 16 available common IDs and requires migration/restore tests. Older firmware may reject new FUN10 records with a larger parameter count; old projects must still load in the new firmware.
4. **Preset gap:** current user preset records have room for parameter values, but the private extension needs its own versioned representation and restore behavior.
5. **Storage safety:** no Flash sector allocation until the user's full device dump is verified.
6. **Sound parity:** ADSR reuse, FM CHARACTER effective default and WEST COAST VOLUME gain must be resolved before freezing the map.

## 7. Recommendation

Retain the 37-control V1 sound scope. Use E0..E7 for the eight highest-value modulation targets, use up to 16 P_MC common IDs for continuous controls that benefit from parameter locks and standard pages, and implement a small versioned private sidecar bank for the remaining 9/13 values. Do not reduce sound functionality merely to fit the existing eight edit values.

This remains a proposal. No parameter IDs, engine ABI, project format, backup protocol, sidecar, Flash address, firmware code, or hardware state was changed.


## CP1.4 execution addendum — adopted V1 working baseline (2026-11-02)

The user instructed the project to proceed with the recommended CP1.4 closure work. The following is now the **adopted logical working baseline**, replacing the earlier status of an unapproved proposal for planning purposes. It is not yet an implemented parameter map or a physical storage approval.

- **37 V1 controls remain in scope.**
- Adopt **8 Engine EDIT + 16 common P_MC + 13 private values**.
- All 13 private values are stored independently per track/part (four parts). They are persistent patch state and must not be stored in volatile engine DSP memory.
- FOLLOW and METAL remain independent switches in OSCILLATORS / SYNTH VOICE; OFF must not erase their associated values.
- E0..E7 remain the eight Engine EDIT / modulation destinations.
- Continuous P_MC controls are eligible for Motion Lock only after explicit whitelist/range handling and tests. Private discrete enums/switches are not Motion Lock or modulation-matrix destinations in V1.
- MCX0..MCX6 are a separate extension namespace, not a substitute for the 13 private V1 values. MCX extension-motion is deferred from this CP1.4 package pending a separate scope decision.
- Sidecar must be versioned and logically associated with four stored projects plus live autosave; use A/B copies, CRC, generation matching, deterministic defaults, and safe fallback that never blocks loading the base project.
- Preset save/load and archive/backup/restore must carry Sidecar data as a compatible versioned unit.
- The ten-sector A/B capacity estimate remains arithmetic only. No physical Flash range or sector is approved.

### Remaining work before CP1.4 can close

1. Freeze the exact 8/16/13 table against the 37-control inventory: ownership, units, ranges, defaults and enum encoding.
2. Define/implement private per-track read/write/edit access and UI/API behavior.
3. Define and test the P_MC Motion Lock whitelist; exclude discrete/private values.
4. Specify exact Sidecar record fields and byte size, then verify capacity.
5. Specify A/B atomic transaction ordering and run host/emulator interruption, CRC, stale-generation, missing/corrupt record and fallback tests.
6. Define preset and archive-v2 backup/restore migration, including four projects and live autosave.
7. Complete legacy/new project and preset migration tests.
8. Audit the full FM-1 V1.5 dump and factory recovery route before any physical Flash allocation.

**Closure rule:** CP1.4 can close at the logical/software-architecture level after items 1–7 are implemented, tested and reviewed. Physical Flash sector selection is deliberately separate and remains blocked until item 8. This addendum assigns no stable parameter IDs and authorizes no device or Flash write.
