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
| E0 | FREQ | Continuous | 30–500 | 160 | Step not specified; source UI label is Freq | Base oscillator frequency relationship |
| E1 | FM INDEX | Continuous | 0–1 | 0 | Step 0.01 in source UI | Complex FM base depth |
| E2 | METAL RATIO | Continuous | 0.5–12 | 4.20 | Step 0.01 in source UI | Separate METAL FM and ring ratio |
| E3 | WEST COAST DRIVE | Continuous | 0–1 | 0 | Step 0.01 in source UI | FM/METAL depth scaling; not folder drive |
| E4 | FOLD | Continuous | 0–1 | 0.25 | Step 0.01 in source UI | Second-stage fold blend |
| E5 | CUTOFF | Continuous | 100–8000 Hz | 1200 Hz | Step 10 Hz in source UI | Main low-pass filter |
| E6 | RESONANCE | Continuous | 0.5–20 | 2.0 | Step 0.1 in source UI | Main filter Q |
| E7 | LPG DECAY | Continuous | 0.02–2 s | 0.25 s | Step not specified in source inventory | LPG triggered tail |

The E0–E7 ordering above follows the accepted CP1.4 proposal. The eight controls are candidate modulation/motion destinations because the existing host exposes those eight edit slots. Actual engine descriptor encoding and quantization remain a CP1.4.2 implementation check.

## 4. Common P_MC — sixteen continuous controls

These are common per-track controls. Their logical membership is frozen; stable numeric IDs and insertion/migration details are not assigned here.

| # | Control | Type | Source range | Default | Source step / unit | Notes |
|---:|---|---|---:|---:|---|---|
| 1 | OSC2 LEVEL | Continuous | 0–1 | 0.50 | Step 0.01 | Direct-audio OSC2 level |
| 2 | OSC2 DETUNE | Continuous | 0–30 cents | 7 cents | Step 0.5 cents | Must not alias host P_DETUNE |
| 3 | TIMBRE | Continuous | 0–1 | 0 | Step 0.01 | First fixed wavefolder-stage input gain |
| 4 | MOD RATIO | Continuous | 0.25–16 | 3.07 | Step not specified | Complex modulator ratio; distinct from METAL RATIO |
| 5 | AM DEPTH | Continuous | 0–1 | 0 | Step 0.01 | Complex AM depth |
| 6 | FM FEEDBACK | Continuous | 0–0.8 | 0 | Step 0.01 | Dedicated delayed feedback |
| 7 | FM ENV AMT | Continuous | 0–1 | 0.50 | Step 0.01 | Triggered increase over base FM index |
| 8 | FM ENV ATTACK | Continuous | 0.001–1 s | 0.005 s | Step not specified | Complex FM-index envelope attack |
| 9 | FM ENV DECAY | Continuous | 0.01–3 s | 0.25 s | Step not specified | Complex FM-index envelope decay |
| 10 | METAL DECAY | Continuous | 0.05–3 s | 0.40 s | Step not specified | METAL trigger decay |
| 11 | FOLD SYMMETRY | Continuous | -1–1 | 0 | Step 0.01 | Second-stage transfer offset |
| 12 | WAVEFOLDER DRIVE | Continuous | 0–1 | 0.20 | Step 0.01 | Second-stage pre-fold gain; not WEST COAST DRIVE |
| 13 | LPG RESPONSE | Continuous | 0–1 | 0.50 | Step 0.01 | LPG transient response |
| 14 | VOICE VCA LEVEL | Continuous | 0–1 | 0.50 | Step 0.01 | Triggered voice VCA peak; not host P_LEVEL |
| 15 | WAVEFOLDER MODULE VOLUME | Continuous | 0–1 | 1 | Step not specified | Output level after second folder stage |
| 16 | WEST COAST VOLUME | Continuous | 0–1 | 1 | Step not specified | Final engine output gain; source gain scaling conflict unresolved |

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
| 6 | FM CHARACTER | Enum | SMOOTH / CLASSIC / METALLIC | **Unresolved: source object=METALLIC (2); HTML selector markup=CLASSIC (1)** | Discrete | Do not freeze the default until effective runtime initialization is checked |
| 7 | COMPLEX MODULE VOLUME | Continuous | 0–1 | 1 | Step not specified | Complex oscillator output level |
| 8 | FOLD MODE | Enum | SOFT / CLASSIC / HARD | CLASSIC | Discrete | Second-stage folder transfer curve |
| 9 | LPG MODULE VOLUME | Continuous | 0–1 | 1 | Step not specified | LPG gain control |
| 10 | ATTACK | Continuous | 0.001–1 s | 0.010 s | Step not specified | Normal voice amplitude envelope |
| 11 | DECAY | Continuous | 0.05–3 s | 0.30 s | Step not specified | Normal voice amplitude envelope; distinct from FM and METAL decay |
| 12 | SUSTAIN | Continuous | 0–1 | 0 | Step not specified | Normal voice amplitude envelope |
| 13 | RELEASE | Continuous | 0.01–3 s | 0.20 s | Step not specified | Normal voice amplitude envelope |

### 5.1 Conditional host ADSR reuse

ATTACK, DECAY, SUSTAIN and RELEASE remain in the logical 13-value inventory until parity is demonstrated. Felucca P_ATK/P_DEC/P_SUS/P_REL may serve as the normal voice ADSR only after tests prove acceptable v98 parity for:
- attack/decay/release curves and time mapping;
- note retrigger and mono legato behavior;
- velocity and accent interaction;
- gate-off and voice-lifecycle behavior;
- ENV SYNC behavior (must not be silently imposed if it changes v98 semantics).

If all checks pass, the four Private values may be represented by the host ADSR without duplicate persistent storage. This is a later verified optimization, not an assumption in this frozen logical map. They are not substitutes for FM-index, LPG or METAL envelopes.

## 6. Explicit non-alias rules

- MOD RATIO (default 3.07) and METAL RATIO (default 4.20) are independent.
- TIMBRE, WEST COAST DRIVE and WAVEFOLDER DRIVE are three independent controls.
- OSC2 DETUNE is not Felucca P_DETUNE; P_DETUNE is host UNISON voice spread.
- VOICE VCA LEVEL is not Felucca P_LEVEL; P_LEVEL is the final host track mixer level.
- WEST COAST VOLUME is not P_LEVEL and remains an internal engine output control.
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
1. **FM CHARACTER default:** source parameter object initializes METALLIC (2), while HTML selector markup marks CLASSIC (1) selected. Verify effective runtime state in the protected v98 reference.
2. **WEST COAST VOLUME scaling:** v98 direct knob handler writes `VOLUME × 12`, while the recurring volume loop writes `VOLUME × 5`. Keep logical range/default, but do not claim exact gain parity until calibrated.
3. **FREQ quantization and physical unit:** source inventory records raw range 30–500 and default 160; a definitive step and unit label must be checked against the actual source control implementation before fixed-point encoding.
4. **Unspecified source steps:** where the tables say “not specified”, implementation quantization must be derived from the protected source behavior and documented before assigning a storage encoding.
5. **ADSR host reuse:** conditional on the parity test matrix in §5.1.

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
- [ ] Verify FM CHARACTER effective default in v98.
- [ ] Verify FREQ unit and quantization.
- [ ] Verify WEST COAST VOLUME gain behavior.
- [ ] Complete ADSR host parity tests.
- [ ] Review and sign off the logical map against the protected source artifact.

The unchecked items do not invalidate the logical 8/16/13 allocation. They prevent claiming complete source-level encoding closure and must be resolved before CP1.4.2 assigns stable IDs or implementation encodings.

## 11. Safety and change control

This document does not authorize:
- changing the factory firmware;
- writing to physical FM-1 Flash or selecting physical sectors;
- assigning stable numeric parameter IDs;
- changing project, preset, Sidecar or archive binary layouts;
- merging any pull request.

All changes remain on the `energun-metal-core` development branch. Existing reference documents and protected v98 artifact remain unchanged.
