# ENERGUN FM-1 CORE — CP1.4.1 frozen logical parameter map

Date: 2026-11-02  
Repository: `energundenis/Felucca`  
Branch: `energun-metal-core`  
Status: **LOGICAL V1 BASELINE FROZEN — stable numeric IDs and firmware encoding NOT assigned**

## 1. Purpose and authority

This document consolidates the approved V1 sound-control inventory and the accepted CP1.4 access split into one implementation-facing logical map.

Primary source documents:
- `docs/ENERGUN_METAL_CORE_CP1_3_COMPLETE_PARAMETER_INVENTORY.md` — protected v98 source inventory, 37 independent controls, ranges and defaults.
- `docs/ENERGUN_METAL_CORE_CP1_4_PARAMETER_ACCESS_STORAGE_PROPOSAL.md` — accepted 8 Engine EDIT + 16 common P_MC + 13 Private allocation and persistence constraints.
- `docs/ENERGUN_METAL_CORE_PARAMETER_SEMANTICS_AUDIT_02.md` — semantics of Felucca host ADSR, P_LEVEL and P_DETUNE.
- `docs/ENERGUN_FM1_CORE_V1_SCOPE_DECISION_01.md` — approved V1 signal architecture and scope boundaries.

This map is a logical allocation, not a firmware ABI or storage-format approval. It assigns no numeric parameter IDs, page IDs, byte offsets, Flash addresses or Sidecar layout. The four Private banks are independent per-part persistent state.

## 2. Counting and ownership rules

The V1 inventory contains **37 independent sound controls**:
- 8 Engine EDIT controls (E0–E7);
- 16 common continuous controls (P_MC);
- 13 Private controls **for each of four parts**.

The 13 Private values are therefore four separate per-part banks, not one global bank and not volatile DSP memory. Total logical Private storage is 52 values across four parts.

FOLLOW and METAL are independent booleans. Turning either OFF must not erase or reset any associated parameter values.

MCX0–MCX6 are a separate extension namespace and are not included in this 37-control count.

## 3. Engine EDIT — eight controls

These are the eight existing engine edit slots. They remain the modulation-matrix destinations and the primary E0–E7 edit controls.

| Slot | Control | Type | Source range | Default | Source step / unit | Notes |
|---|---|---|---:|---:|---|---|
| E0 | FREQ | Continuous | 30–500 Hz | 160 Hz | Step 1 Hz | Directly sets the main Web Audio oscillator frequency; the source uses `OscillatorNode.frequency` (Hz). |
| E1 | FM INDEX | Continuous | 0–1 | 0 | Step 0.01 in source UI | Complex FM base depth |
| E2 | METAL RATIO | Continuous | 0.5–12 | 4.20 | Step 0.01 in source UI | Separate METAL FM and ring ratio |
| E3 | WEST COAST DRIVE | Continuous | 0–1 | 0 | Step 0.01 in source UI | FM/METAL depth scaling; not folder drive |
| E4 | FOLD | Continuous | 0–1 | 0.25 | Step 0.01 in source UI | Second-stage fold blend |
| E5 | CUTOFF | Continuous | 100–8000 Hz | 1200 Hz | Step 10 Hz in source UI | Main low-pass filter |
| E6 | RESONANCE | Continuous | 0.5–20 | 2.0 | Step 0.1 in source UI | Main filter Q |
| E7 | LPG DECAY | Continuous | 0.02–2 s | 0.25 s | Step 0.01 s | LPG triggered tail |

The E0–E7 ordering above is the CP1.4 proposal candidate, not the final slot allocation. A previously frozen 2026-10-30 E0–E7 order is recorded in the project decision history and differs from this candidate; the earlier accepted freeze remains authoritative until the conflict is explicitly resolved. The eight controls are candidate modulation/motion destinations because the existing host exposes those eight edit slots.

## 4. Common P_MC — sixteen continuous controls

These are common per-track controls. Their logical membership is frozen; stable numeric IDs and insertion/migration details are not assigned here.

| # | Control | Type | Source range | Default | Source step / unit | Notes |
|---:|---|---|---:|---:|---|---|
| 1 | OSC2 LEVEL | Continuous | 0–1 | 0.50 | Step 0.01 | Direct-audio OSC2 level |
| 2 | OSC2 DETUNE | Continuous | 0–30 cents | 7 cents | Step 0.5 cents | Must not alias host P_DETUNE |
| 3 | TIMBRE | Continuous | 0–1 | 0 | Step 0.01 | First fixed wavefolder-stage input gain |
| 4 | MOD RATIO | Continuous | 0.25–16 | 3.07 | Step 0.01 | Complex modulator ratio; distinct from METAL RATIO |
| 5 | AM DEPTH | Continuous | 0–1 | 0 | Step 0.01 | Complex AM depth |
| 6 | FM FEEDBACK | Continuous | 0–0.8 | 0 | Step 0.01 | Dedicated delayed feedback |
| 7 | FM ENV AMT | Continuous | 0–1 | 0.50 | Step 0.01 | Triggered increase over base FM index |
| 8 | FM ENV ATTACK | Continuous | 0.001–1 s | 0.005 s | Step 0.001 s | Complex FM-index envelope attack |
| 9 | FM ENV DECAY | Continuous | 0.01–3 s | 0.25 s | Step 0.01 s | Complex FM-index envelope decay |
| 10 | METAL DECAY | Continuous | 0.05–3 s | 0.40 s | Step 0.01 s | METAL trigger decay |
| 11 | FOLD SYMMETRY | Continuous | -1–1 | 0 | Step 0.01 | Second-stage transfer offset |
| 12 | WAVEFOLDER DRIVE | Continuous | 0–1 | 0.20 | Step 0.01 | Second-stage pre-fold gain; not WEST COAST DRIVE |
| 13 | LPG RESPONSE | Continuous | 0–1 | 0.50 | Step 0.01 | LPG transient response |
| 14 | VOICE VCA LEVEL | Continuous | 0–1 | 0.50 | Step 0.01 | Triggered voice VCA peak; not host P_LEVEL |
| 15 | WAVEFOLDER MODULE VOLUME | Continuous | 0–1 | 1 | Step 0.01 | Output level after second folder stage |
| 16 | WEST COAST VOLUME | Continuous | 0–1 | 1 | Step 0.01 | Final engine output gain; effective steady-state v98 mapping is VOLUME × 5 |

P_MC motion/parameter-lock support requires an explicit whitelist and tests. Continuous type alone does not imply that the current host automatically accepts these IDs. The current modulation-matrix destination list remains E0–E7 unless separately extended and approved.

## 5. Private — thirteen persistent controls per part

Each of the four parts owns its own independent values for all rows below. These values must be readable, editable, displayed and saved for the selected part. They must never be placed in volatile `engine_t.mem` DSP memory.

| # | Control | Type | Source range / values | Default | Source step / unit | Notes |
|---:|---|---|---|---|---|---|
| 1 | WAVE | Enum | SINE / TRI / SAW | SINE | Discrete | Main waveform shared by OSC1 and OSC2 |
| 2 | FOLLOW | Boolean | OFF / ON | ON | Discrete | Sequencer pitch/octave tracking |
| 3 | METAL | Boolean | OFF / ON | ON | Discrete | Enables separate METAL FM/ring paths and METAL trigger behavior |
| 4 | MODULATION | Enum | FM / AM | FM | Discrete | Complex oscillator mode |
| 5 | MOD OSC WAVE | Enum | SINE / TRIANGLE / SAW / SQUARE | SINE | Discrete | Complex modulator waveform |
| 6 | FM CHARACTER | Enum | SMOOTH / CLASSIC / METALLIC | **METALLIC (2)** | Discrete | Canonical DSP default; UI is synchronized from this value during audio initialization |
| 7 | COMPLEX MODULE VOLUME | Continuous | 0–1 | 1 | Step 0.01 | Complex oscillator output level |
| 8 | FOLD MODE | Enum | SOFT / CLASSIC / HARD | CLASSIC | Discrete | Second-stage folder transfer curve |
| 9 | LPG MODULE VOLUME | Continuous | 0–1 | 1 | Step 0.01 | V1-approved Option C: trigger peak = (0.25 + 0.75 × LPG RESPONSE) × MODULE VOLUME; one LPG gain stage |
| 10 | ATTACK | Continuous | 0.001–1 s | 0.010 s | Step 0.001 s | Normal voice amplitude envelope |
| 11 | DECAY | Continuous | 0.05–3 s | 0.30 s | Step 0.01 s | Normal voice amplitude envelope; distinct from FM and METAL decay |
| 12 | SUSTAIN | Continuous | 0–1 | 0 | Step 0.01 | Normal voice amplitude envelope |
| 13 | RELEASE | Continuous | 0.01–3 s | 0.20 s | Step 0.01 s | Normal voice amplitude envelope |

### 5.1 VCA ownership and host lifecycle

The four normal voice contour values ATTACK, DECAY, SUSTAIN and RELEASE remain in the 13-value Private bank. Felucca P_ATK/P_DEC/P_SUS/P_REL are **not** aliases and must not be used as a drop-in replacement. The accepted V1 architecture preserves the v98-compatible engine-owned VCA (`ownenv=1`) without cascading the host ADSR. Felucca continues to own voice allocation, note/gate delivery, shared voice budgeting and host amplitude ramps/fades; Metal Core owns its VCA contour and reports completion through the verified `done()` lifecycle contract. Velocity and Accent must be applied by the engine, with host amplitude ramps multiplied exactly once. LPG and FM-index envelopes remain independent. See CP1.14/CP1.15 and CP3.0 source contract audit.

## 6. Explicit non-alias rules

- MOD RATIO (default 3.07) and METAL RATIO (default 4.20) are independent.
- TIMBRE, WEST COAST DRIVE and WAVEFOLDER DRIVE are three independent controls.
- OSC2 DETUNE is not Felucca P_DETUNE; P_DETUNE is host UNISON voice spread.
- VOICE VCA LEVEL is not Felucca P_LEVEL; P_LEVEL is the final host track mixer level.
- WEST COAST VOLUME is not P_LEVEL and remains an internal engine output control; v98 steady-state reference scaling is ×5 (the persistent loop overwrites transient ×12 assignments).
- LPG RESPONSE / DECAY / MODULE VOLUME are independent of main filter CUTOFF / RESONANCE.
- FM ENV ATTACK/DECAY, METAL DECAY, LPG DECAY and normal voice ADSR DECAY are separate envelope paths.
- FOLLOW and METAL are separate persistent booleans and remain independently editable.

## 7. Deferred controls and exclusions

The following v98 controls/functions are not part of this V1 37-control map:
- RING MIX and RING RATIO (separate RING2 path);
- SPREAD pending verified routing behavior;
- custom LFO RATE/DEPTH and RANDOM RATE/SLEW;
- full custom PATCH BAY;
- DRONE/SEQ prototype selector (use host note/sequencer lifecycle);
- WEST COAST ON prototype switch (use normal host engine/track enable and mute).

MCX0–MCX6 remain a separate extension namespace. No MCX motion support is implied by this document.

## 8. Motion and modulation policy

- E0–E7 remain the eight existing engine edit/modulation destinations.
- P_MC values are eligible for Motion Lock only after an explicit whitelist/range implementation and regression tests.
- Private values are not Motion Lock or modulation-matrix destinations in V1.
- Discrete booleans and enums are excluded from continuous Motion Lock by default.
- Do not encode Private or MCX values by setting the existing motion-lock high bit or by overloading E0–E7.

## 9. Known unresolved source facts

These are explicit gates, not permission to guess:
1. **Host stored-value resolution:** source physical ranges/steps are not the Felucca stored representation. Project, preset and Motion Lock values are limited to -64..127 (one byte after offset); several v98 controls have more source positions than this permits. **Decision accepted 2026-11-02:** V1 uses standard Felucca 7-bit parameter encoding; frequency/ratio/time controls use suitable nonlinear mappings and levels/depths use linear mappings. The per-control conversion table and quantization evidence are tracked in `ENERGUN_FM1_CORE_CP1_4_2_PARAMETER_CONVERSION_TABLE_DRAFT.md`. Do not claim source UI step granularity is available on FM-1.
2. **E0–E7 allocation conflict:** the 2026-10-30 Frozen Draft used E0 FREQ / E1 OSC2 LEVEL / E2 TIMBRE / E3 WEST COAST DRIVE / E4 METAL RATIO / E5 FOLD / E6 CUTOFF / E7 RESONANCE, while this CP1.4 proposal uses a different E0–E7 order. The earlier frozen order remains the governing baseline until the original approval record is recovered and any superseding user decision is explicit. Do not assign final E descriptors from the candidate table.
3. **Stable IDs and encoding:** no numeric IDs, serialized field IDs, byte encodings or backward-compatibility rules are approved.

## 10. CP1.4.1 acceptance result

**Logical allocation: FROZEN.**  
**Stable numeric IDs / wire encoding: NOT ASSIGNED.**  
**Firmware implementation: NOT PART OF THIS CHECKPOINT.**  
**Physical Flash allocation or device writes: NOT AUTHORIZED.**

Acceptance checks:
- [x] 37-control count reconciled: 8 + 16 + 13 = 37.
- [x] Four independent Private banks recorded (52 Private values across four parts).
- [x] Types, source ranges, defaults and known source steps recorded where available.
- [x] Independent switch, ratio, envelope and gain semantics documented.
- [x] Unresolved source conflicts are explicitly blocked rather than guessed.
- [x] Existing Felucca host parameter reuse is conditional and semantics-based.
- [x] FM CHARACTER default resolved: METALLIC (2), with documented pre-audio HTML selector mismatch.
- [x] WEST COAST VOLUME steady-state mapping resolved: ×5.
- [x] LPG MODULE VOLUME V1 semantics approved as Option C.
- [x] Internal VCA ownership approved: ownenv=1; no host ADSR cascade.
- [x] FOLLOW / MOD RATIO Option A approved.
- [x] Short-Gate VCA Option B approved.
- [x] Verify FREQ unit and quantization: Hz, 1 Hz step, range 30–500 Hz, default 160 Hz.
- [ ] Recover/reconcile the earlier E0–E7 freeze against the CP1.4 candidate order; earlier frozen order remains authoritative pending that reconciliation.
- [x] Derive all previously unspecified source steps from the protected v98 parameter object.
- [x] Select standard Felucca 7-bit parameter encoding for V1; high-resolution sidecar-backed parameter values are not selected.
- [ ] Complete per-control conversion curves, code allocation and quantization-error tests in CP1.4.2.
- [ ] Review and sign off the complete map against the protected source artifact.

The decisions checked above supersede the older unresolved notes in this document. Source step values were verified against the protected reference object's parameter declarations; FREQ's Hz unit is corroborated by its direct assignment to the Web Audio oscillator frequency. The unchecked E0–E7 allocation, per-control conversion/quantization verification and final map sign-off prevent complete source-level encoding closure. CP1.4.2 may continue its compatibility audit, but must not assign stable IDs or final E descriptors until the slot conflict and quantization policy are resolved.

### 10.1 Source-step evidence

The protected `energun_metal_core_v98_reference.html` parameter object explicitly declares the following steps that were previously missing from this map: MOD RATIO 0.01; FM ENV ATTACK 0.001 s; FM ENV DECAY 0.01 s; METAL DECAY 0.01 s; LPG DECAY 0.01 s; COMPLEX MODULE VOLUME 0.01; WAVEFOLDER MODULE VOLUME 0.01; LPG MODULE VOLUME 0.01; WEST COAST VOLUME 0.01; normal ATTACK 0.001 s; DECAY 0.01 s; SUSTAIN 0.01; RELEASE 0.01. The source object's `wcFreq` declaration is `{v:160,min:30,max:500,s:1}`, and `wcInit()` assigns `P.wcFreq.v` to `wcOsc.frequency.value`, establishing Hz and a 1 Hz step. Source artifact Git blob: `4f5ff6c7cadf1f13ac08376a293a354519fcaf37`.

## 11. Safety and change control

This document does not authorize:
- changing the factory firmware;
- writing to physical FM-1 Flash or selecting physical sectors;
- assigning stable numeric parameter IDs;
- changing project, preset, Sidecar or archive binary layouts;
- merging any pull request.

All changes remain on the `energun-metal-core` development branch. Existing reference documents and protected v98 artifact remain unchanged.
