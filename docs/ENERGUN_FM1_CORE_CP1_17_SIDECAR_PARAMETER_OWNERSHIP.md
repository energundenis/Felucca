# ENERGUN FM-1 CORE — CP1.17 Sidecar, parameter ownership and persistence inventory

Date: 2026-11-03  
Branch: `energun-metal-core`  
Status: **WORKING INVENTORY / ID ALLOCATION PROPOSAL — no firmware ABI or Flash allocation changed**

## 1. Scope

This record consolidates the accepted 8/16/13 logical allocation with the frozen Sidecar V1 layout and defines the ownership and persistence boundaries needed by CP1.17, CP1.6 and CP1.7.

It is an inventory and integration contract. It does not register METAL CORE, change production parameter descriptors, alter FUN10 or preset serializers, allocate storage object IDs/sectors, or write to hardware.

## 2. Authoritative Engine EDIT allocation

The previously accepted E0–E7 order remains unchanged:

| Slot | Logical control | Source range | Default | Persistence owner |
|---|---|---:|---:|---|
| E0 | FREQ | 30–500 Hz | 160 Hz | Base project / base preset |
| E1 | OSC2 LEVEL | 0–1 | 0.50 | Base project / base preset |
| E2 | TIMBRE | 0–1 | 0 | Base project / base preset |
| E3 | WEST COAST DRIVE | 0–1 | 0 | Base project / base preset |
| E4 | METAL RATIO | 0.5–12 | 4.20 | Base project / base preset |
| E5 | FOLD | 0–1 | 0.25 | Base project / base preset |
| E6 | CUTOFF | 100–8000 Hz | 1200 Hz | Base project / base preset |
| E7 | RESONANCE | 0.5–20 | 2.0 | Base project / base preset |

These eight controls are engine-owned EDIT parameters. They are not Sidecar Private values.

## 3. Common P_MC inventory — 16 controls

The following order is the Sidecar V1 per-part byte order and the proposed append-before-E0 logical ID order.

| Proposed ID | Index | Control | Source range | Default | Type / stored code |
|---:|---:|---|---:|---:|---|
| 103 | 0 | OSC2 DETUNE | 0–30 cents | 7 cents | Continuous, 0..127 |
| 104 | 1 | FM INDEX | 0–1 | 0 | Continuous, 0..127 |
| 105 | 2 | MOD RATIO | 0.25–16 | 3.07 | Continuous, 0..127 |
| 106 | 3 | AM DEPTH | 0–1 | 0 | Continuous, 0..127 |
| 107 | 4 | FM FEEDBACK | 0–0.8 | 0 | Continuous, 0..127 |
| 108 | 5 | FM ENV AMT | 0–1 | 0.50 | Continuous, 0..127 |
| 109 | 6 | FM ENV ATTACK | 0.001–1 s | 0.005 s | Continuous, 0..127 |
| 110 | 7 | FM ENV DECAY | 0.01–3 s | 0.25 s | Continuous, 0..127 |
| 111 | 8 | METAL DECAY | 0.05–3 s | 0.40 s | Continuous, 0..127 |
| 112 | 9 | FOLD SYMMETRY | -1–1 | 0 | Signed, -64..63 |
| 113 | 10 | WAVEFOLDER DRIVE | 0–1 | 0.20 | Continuous, 0..127 |
| 114 | 11 | LPG RESPONSE | 0–1 | 0.50 | Continuous, 0..127 |
| 115 | 12 | LPG DECAY | 0.02–2 s | 0.25 s | Continuous, 0..127 |
| 116 | 13 | VOICE VCA LEVEL | 0–1 | 0.50 | Continuous, 0..127 |
| 117 | 14 | WAVEFOLDER MODULE VOLUME | 0–1 | 1 | Continuous, 0..127 |
| 118 | 15 | WEST COAST VOLUME | 0–1 | 1 | Continuous, 0..127 |

### ID proposal and compatibility

- Proposed P_MC IDs: 103–118.
- Proposed E0–E7 IDs after insertion: 119–126.
- Proposed runtime P_COUNT: 127; this remains below the 128-ID Motion Lock bit boundary.
- This mapping is an inventory proposal, not yet a production descriptor/ABI change. It must be frozen together with the final descriptor table and Motion whitelist before source IDs are assigned.
- Project FUN10 base compatibility remains at the factory-compatible count (111): the project adapter must preserve the legacy common values and compact E0–E7 into the final eight base positions. P_MC is restored from the matching Sidecar.
- User-preset base records have 144 parameter bytes and can carry P_COUNT=127 while preserving the final-eight E0–E7 migration rule; old firmware compatibility fixtures remain mandatory.

## 4. Private inventory — 13 controls per part

Four independent banks exist, one per part. These controls are not P_COUNT entries and must not be placed in `track_t.p[]` or volatile `engine_t.mem`.

| Sidecar index | Key | Type / valid encoded values | Runtime owner | Persistence owner |
|---:|---|---|---|---|
| 0 | WAVE | Enum: 0 SINE, 1 TRI, 2 SAW | Part Private cache / oscillator adapter | Project Sidecar; preset-bank Sidecar |
| 1 | FOLLOW | Boolean: 0 OFF, 1 ON | Part Private cache / FOLLOW stage | Project Sidecar; preset-bank Sidecar |
| 2 | METAL | Boolean: 0 OFF, 1 ON | Part Private cache / METAL stage | Project Sidecar; preset-bank Sidecar |
| 3 | MODULATION | Enum: 0 FM, 1 AM | Part Private cache / modulation router | Project Sidecar; preset-bank Sidecar |
| 4 | MOD OSC WAVE | Enum: 0 SINE, 1 TRIANGLE, 2 SAW, 3 SQUARE | Part Private cache / modulation oscillator | Project Sidecar; preset-bank Sidecar |
| 5 | FM CHARACTER | Enum: 0 SMOOTH, 1 CLASSIC, 2 METALLIC | Part Private cache / FM stage | Project Sidecar; preset-bank Sidecar |
| 6 | COMPLEX MODULE VOLUME | Unsigned 7-bit: 0..127 | Part Private cache / Complex module | Project Sidecar; preset-bank Sidecar |
| 7 | FOLD MODE | Enum: 0 SOFT, 1 CLASSIC, 2 HARD | Part Private cache / wavefolder | Project Sidecar; preset-bank Sidecar |
| 8 | LPG MODULE VOLUME | Unsigned 7-bit: 0..127 | Part Private cache / LPG | Project Sidecar; preset-bank Sidecar |
| 9 | ATTACK | Unsigned 7-bit canonical time code | Part Private cache / amplitude envelope | Project Sidecar; preset-bank Sidecar |
| 10 | DECAY | Unsigned 7-bit canonical time code | Part Private cache / amplitude envelope | Project Sidecar; preset-bank Sidecar |
| 11 | SUSTAIN | Unsigned 7-bit canonical level code | Part Private cache / amplitude envelope | Project Sidecar; preset-bank Sidecar |
| 12 | RELEASE | Unsigned 7-bit canonical time code | Part Private cache / amplitude envelope | Project Sidecar; preset-bank Sidecar |

**Open default decision:** the v98 source object and HTML selector disagree on the initial FM CHARACTER selection (source object ordinal 2 vs markup-selected ordinal 1). Sidecar V1 validates all three ordinals but intentionally does not choose the runtime default. Do not silently pick one; resolve it in the Private descriptor/default table before CP1.17 is frozen.

Missing, unsupported or invalid Sidecar data must not block loading the legacy base object. The Private cache applies deterministic V1 defaults, and the complete extension record is accepted or rejected atomically.

## 5. Motion ownership and eligibility

- P_MC continuous controls are the only proposed new Motion Lock members.
- Proposed whitelist, in the same order as P_MC: IDs 103–118.
- E0–E7 retain their existing engine Motion behavior, subject to existing descriptor validation.
- Private controls, booleans, enums and MCX0–MCX6 are excluded from V1 Motion Lock.
- FOLD SYMMETRY uses signed -64..63; other P_MC controls use canonical 0..127 values. Motion and parameter editing must share the same descriptor conversion.
- Base FUN10 motion plus Sidecar P_MC Motion must never exceed the one global 128-event pool across all four tracks. No second 128-event pool is created.
- Duplicate (track, step, parameter) keys are rejected or updated according to the existing Motion replacement rule; malformed or over-capacity Sidecar data is rejected as a whole.
- P_MC Motion belongs in the project/live Sidecar payload. User-preset banks do not carry Motion.

The numeric whitelist is a proposal pending stable ID freeze and implementation tests; the current Sidecar V1 validator deliberately does not enforce numeric P_MC IDs.

## 6. Persistence ownership matrix

| Data | Runtime owner | Project save/load | Live autosave | User preset save/load | Legacy base behavior |
|---|---|---|---|---|---|
| E0–E7 | METAL CORE engine descriptors / track parameter array | Base FUN10, compacted to final eight | Base autosave FUN10 | Base preset record | Preserved by final-eight count migration |
| P_MC values | Common parameter layer, part-scoped | Project Sidecar, four parts | Live Sidecar identity | Base preset record (P_COUNT=127) | Old project firmware ignores Sidecar; old preset migration preserves E0–E7 |
| Private ×4 parts | Part-owned Private runtime cache | Project Sidecar | Live Sidecar identity | Selected preset slot's Private record in bank Sidecar | Missing extension defaults; base remains loadable |
| P_MC Motion | Existing shared Motion runtime pool, explicit P_MC whitelist | Sidecar extension, merged with base Motion | Live Sidecar identity | Not stored in preset bank | Old firmware ignores Sidecar Motion |
| DSP working state / oscillator phases / envelopes | Engine/voice runtime memory | Never persisted | Never persisted | Never persisted | Reinitialized on load/retrigger |
| Sequencer pattern/step data | Existing sequencer/project state | Existing base project path unless separately specified | Existing autosave path | Only if existing preset contract includes it | Must retain existing Felucca serializer semantics |

The final row is an ownership boundary, not a proposal to add sequencer data to Sidecar. CP3 must inventory the existing Sequencer serializer before any storage change.

## 7. Sidecar V1 identity and size contract

- Project slots: identity kind 1, indices 0–3.
- Live autosave: identity kind 2, index 0.
- User-preset banks: identity kind 3, indices 0–1.
- Seven logical identities, each with A/B copies: 14 logical copies. This is not a physical Flash sector map.
- Project/live payload: 116 value bytes + 1 Motion count + up to 128 × 3 Motion bytes = 501 bytes maximum; 20-byte envelope; 521-byte maximum record.
- Preset-bank payload: 2-byte slot bitmap + 16 × 13 Private bytes = 210 bytes; 20-byte envelope; 230-byte maximum record.
- Pair identity uses identity kind/index + exact base sequence + base payload CRC-32.

## 8. CP1.6 Project Runtime Integration — required contract

1. Preserve the existing FUN10 base serializer and legacy `np=111` representation for project compatibility.
2. On load, load/migrate the base project first; then find a valid Sidecar matching the selected base sequence and payload CRC.
3. If no matching Sidecar exists, load the base normally and apply deterministic extension defaults.
4. On save, snapshot the complete P_MC + Private + P_MC Motion extension for all four parts. Rebind it to the next base generation and commit using the pair transaction coordinator.
5. Route save, save-as, rename, erase and live autosave through one lowest-common persistence boundary; erase must tombstone/invalidate the extension so stale values cannot reappear.
6. Generation-aware dirty clearing: a newer runtime edit made during persistence must remain dirty.
7. Serialize autosave against project writes and FM6 staging ownership. Do not rely on autosave delay alone as a storage lock.
8. Archive V1 remains legacy-only and must restore extension defaults; Archive V2 requires a separate pair-aware restore design and destination-generation rebinding.

## 9. CP1.7 Preset Runtime Integration — required contract

1. Preserve the existing two-bank, 16-slot-per-bank base preset format and its 144-byte parameter capacity.
2. Keep P_MC in the base preset parameter array; do not duplicate it in preset Sidecar.
3. Persist all 13 Private values for each of 16 slots in each bank Sidecar (208 value bytes + 2-byte presence bitmap).
4. Preset load applies base parameters first, then the matching bank Sidecar Private values for the selected slot. Missing/absent slot extension applies deterministic defaults.
5. Save, overwrite, rename and erase must preserve all untouched slots and rebind the complete bank extension to the next base-bank generation.
6. An erased slot must have its presence bit cleared and all 13 stored bytes zeroed.
7. Keep the existing separate FM6 patch-object ordering explicit; the Private Sidecar transaction does not make the FM6 patch object atomic with the preset bank.
8. Test all 32 slots, both banks, legacy presets, missing/corrupt Sidecar, slot reuse and power-loss boundaries.

## 10. CP1.17 closure gates

- [x] Logical 8/16/13 membership reconciled against the accepted E0–E7 order.
- [x] Sidecar V1 envelope and payload field layout frozen for host implementation.
- [x] Sidecar host format, Base binding and A/B selection suites recorded as passing in the prior exact-head evidence.
- [ ] Freeze numeric P_MC IDs 103–118 and E0–E7 IDs 119–126 after descriptor/migration review.
- [ ] Resolve FM CHARACTER default parity and publish the complete 13-key Private default table.
- [ ] Implement and test the exact P_MC Motion whitelist 103–118 against every allowed and denied parameter.
- [ ] Complete real storage pair-transaction fault-injection tests and verify all power-cut states against actual Felucca storage.
- [ ] Confirm all project, autosave, preset-bank save/rename/erase paths use the coordinator.
- [ ] Review AC79 RAM/stack impact and target build integration.

## 11. CP2 / CP3 inventory-only scope

The current task closes only the parameter inventory, identity proposal, ownership and persistence boundaries. It does not close DSP implementation, production engine registration, sequencer integration or target audio qualification.

- CP2 owns the METAL CORE DSP parameter descriptors and transient DSP state; transient DSP state is not project/preset data.
- CP3 owns the host note/voice lifecycle, sequencer event dispatch and the mapping from sequencer events to the existing parameter/Motion contracts.
- The common parameter IDs are owned by the shared parameter model, not by the DSP implementation or Sequencer.
- The 13 Private keys are owned by the per-part Private cache and accessed by typed key, not by fake common parameter IDs.
- Sequencer pattern data remains under the existing Felucca project/preset serializer until its source inventory proves otherwise.
- Numeric ID freeze must precede production Motion whitelist registration and any change to `P_COUNT`.

## 12. Non-goals

No production firmware source or parameter descriptor is changed by this document. No storage object ID, OBJ_COUNT, physical Flash address, sector reservation, bootloader/OTA range, factory image or physical FM-1 state is changed. No branch is merged and no PR is auto-merged.
