# ENERGUN FM-1 CORE — CP2 / CP3 parameter ownership and persistence boundary audit

Date: 2026-11-03  
Repository: `energundenis/Felucca`  
Branch: `energun-development-orchestrator`  
Status: source-backed allocation audit; numeric range reserved as a candidate, production enum/descriptor changes gated on compatibility fixtures.

## 1. Scope and naming note

This audit follows the active CP8 worker lanes:
- CP2 = DSP / VCA;
- CP3 = sequencer (worker lane `cp3-sequencer`).

There is a documentation naming conflict: the older `ENERGUN_METAL_CORE_CP3_INTEGRATION_READINESS_PLAN.md` uses CP3 for full DSP-engine registration and calls sequencer integration CP5. Do not use the old document's CP3 label to infer the active `cp3-sequencer` lane. The project index should normalize these names in a later documentation cleanup.

## 2. Global numeric namespace candidate

Current source facts from `firmware/src/core.h`:
- current `P_E0 = 103`;
- current `P_COUNT = 111`;
- `params_by_count()` preserves the first `np - 8` common parameters and relocates the last eight stored values to today's E0–E7;
- Motion parameter IDs use seven bits; bit 7 is `MOTION_LOCK`;
- the code requires `P_COUNT < 128`.

To preserve the established insertion-before-E0 rule and keep all IDs inside the 7-bit namespace, reserve the following candidate range:

| Numeric ID | Candidate symbol | Logical control | Owner |
|---:|---|---|---|
| 103 | P_MC_OSC2_DETUNE | OSC2 DETUNE | CP2 / Metal Core |
| 104 | P_MC_FM_INDEX | FM INDEX | CP2 / Metal Core |
| 105 | P_MC_MOD_RATIO | MOD RATIO | CP2 / Metal Core |
| 106 | P_MC_AM_DEPTH | AM DEPTH | CP2 / Metal Core |
| 107 | P_MC_FM_FEEDBACK | FM FEEDBACK | CP2 / Metal Core |
| 108 | P_MC_FM_ENV_AMT | FM ENV AMT | CP2 / Metal Core |
| 109 | P_MC_FM_ENV_ATTACK | FM ENV ATTACK | CP2 / Metal Core |
| 110 | P_MC_FM_ENV_DECAY | FM ENV DECAY | CP2 / Metal Core |
| 111 | P_MC_METAL_DECAY | METAL DECAY | CP2 / Metal Core |
| 112 | P_MC_FOLD_SYMMETRY | FOLD SYMMETRY | CP2 / Metal Core |
| 113 | P_MC_WAVEFOLDER_DRIVE | WAVEFOLDER DRIVE | CP2 / Metal Core |
| 114 | P_MC_LPG_RESPONSE | LPG RESPONSE | CP2 / Metal Core |
| 115 | P_MC_LPG_DECAY | LPG DECAY | CP2 / Metal Core |
| 116 | P_MC_VOICE_VCA_LEVEL | VOICE VCA LEVEL | CP2 / Metal Core |
| 117 | P_MC_WAVEFOLDER_VOLUME | WAVEFOLDER MODULE VOLUME | CP2 / Metal Core |
| 118 | P_MC_WEST_COAST_VOLUME | WEST COAST VOLUME | CP2 / Metal Core |
| 119–126 | P_E0–P_E7 | existing eight Engine EDIT controls | CP2 / engine |
| 127 | P_COUNT | count sentinel, not a parameter ID | core |

This allocation uses all 127 legal parameter slots (IDs 0–126). It leaves no spare common P_* ID in V1. New controls after V1 require a separately reviewed extension/private namespace or a versioned architecture change.

**Compatibility proof required before changing `core.h`:**

- Old FUN10 and old user-preset records with `np=111` must map their first 103 common values unchanged and relocate their last eight values to E0–E7 at 119–126.
- The sixteen inserted P_MC values must receive deterministic defaults when loading legacy records.
- New project Base serialization must continue to write the legacy 111-value projection (103 existing common + E0–E7), not all 127 runtime values. The Base FUN10 object remains exactly 3840 bytes.
- New user-preset records may store all 127 common values in the existing 144-byte packed parameter area; the record remains 192 bytes.
- Motion IDs 103–118 must be accepted only for the sixteen P_MC continuous descriptors. The Sidecar stores their logical P_MC index 0–15, not a global ID; Base Motion keeps its existing IDs and E0–E7.
- Exact old/new project and preset fixtures must pass before the numeric range is promoted from reserved candidate to stable firmware ABI.

## 3. CP2 — DSP / VCA parameter ownership

### Common P_MC values (IDs 103–118 candidate)

All sixteen controls are continuous common per-track parameters. Their order, source ranges, defaults, quantization curves and semantics are owned by the frozen CP1.4.1 map and CP1.4.2 conversion table.

CP2 DSP owns the actual signal interpretation:
- OSC2 DETUNE, FM INDEX, MOD RATIO, AM DEPTH, FM FEEDBACK;
- FM ENV AMT / ATTACK / DECAY;
- METAL DECAY;
- FOLD SYMMETRY / WAVEFOLDER DRIVE;
- LPG RESPONSE / LPG DECAY;
- VOICE VCA LEVEL;
- WAVEFOLDER MODULE VOLUME / WEST COAST VOLUME.

The engine must read these controls through the common per-track parameter interface. They are not duplicates of P_LEVEL, P_DETUNE, host ADSR, or E0–E7.

### E0–E7

The eight frozen Engine EDIT values remain engine-owned descriptors and preserve their original logical order:
FREQ, OSC2 LEVEL, TIMBRE, WEST COAST DRIVE, METAL RATIO, FOLD, CUTOFF, RESONANCE.

They remain the existing modulation-matrix destinations. Their global IDs shift only because the sixteen P_MC IDs are inserted before E0.

### Thirteen Private values

The following are per-part persistent state, not global P_* IDs and not volatile `engine_t.mem`:

1. WAVE
2. FOLLOW
3. METAL
4. MODULATION
5. MOD OSC WAVE
6. FM CHARACTER
7. COMPLEX MODULE VOLUME
8. FOLD MODE
9. LPG MODULE VOLUME
10. ATTACK
11. DECAY
12. SUSTAIN
13. RELEASE

CP2 owns their DSP meaning and range/default interpretation. CP1 owns their storage schema and editor read/write persistence contract.

The four Private ADSR values remain engine-owned VCA controls. The accepted VCA architecture is `ownenv=1`; do not alias them to Felucca P_ATK/P_DEC/P_SUS/P_REL and do not cascade the host ADSR. FM-index, METAL and LPG envelopes remain independent envelope paths.

## 4. CP3 — sequencer ownership and Base persistence

The active CP3 sequencer lane currently exercises Chord, Ratchet, Step 1.5, Sequencer 1.5.5 and Swing. These behaviors use existing Felucca sequencer state and do not require new Metal Core P_* IDs:

- Chord settings use the existing chord/voicing parameter fields.
- Ratchet, slide, tie, accent, nudge and step-condition state use the existing step/event representation.
- Swing and sequence timing use existing common sequencer controls, including the existing swing/division/length/gate fields.
- Note On/Off, Gate, source provenance and Accent metadata are runtime event/voice-lifecycle state; they are not duplicate persistent sound parameters.
- The actual pattern and step fields remain in the Base FUN10 project object and its existing migration rules.

CP3 sequencer does not own the sixteen P_MC sound controls or the thirteen Private sound controls. It may produce sequencer Motion Lock events for P_MC, but those events belong to the Sidecar Motion namespace. Sequencer pattern, chord, ratchet and swing data remain Base-owned.

No new CP3 sequencer numeric IDs are allocated by this audit.

## 5. Persistence boundary matrix

| Data | Runtime owner | Project persistence | User-preset persistence | Motion |
|---|---|---|---|---|
| Existing Felucca common parameters 0–102 | Felucca host | Base FUN10 | Base preset record | Existing Base Motion whitelist |
| P_MC 16 common values | CP2 Metal Core descriptors / host common-param layer | Sidecar V1 per track | Base preset record | Sidecar V1 Motion only |
| E0–E7 | CP2 Metal Core engine | Base FUN10 legacy projection | Base preset record | Existing Base Motion |
| 13 Private values per part | CP2 Metal Core private state adapter | Sidecar V1 per track | Sidecar V1 per preset slot | Not Motion in V1 |
| Sequencer settings and step/pattern state | CP3 sequencer / Felucca host | Base FUN10 | Existing preset pattern fields where already supported | Existing Base Motion / runtime dispatch |
| MCX0–MCX6 | Deferred extension | Not V1 | Not V1 | Not V1 |
| MCX Motion | Deferred extension | Not V1 | Not V1 | Not V1 |

## 6. Required tests before CP1.6 runtime wiring

1. Compile-time namespace assertion: P_MC 103–118, E0–E7 119–126, P_COUNT 127 and `P_COUNT < 128`.
2. Old FUN10 fixture: exact 103 common values and E0–E7 survive count-based migration; P_MC defaults.
3. New Base projection fixture: still writes `np=111`, same 3840-byte object and unchanged step/chain/FM6/Base-Motion offsets.
4. Old user-preset fixture: versions 1–5 still load; P_MC defaults; E0–E7 survive.
5. New user-preset fixture: 127 values fit existing 144-byte array and 192-byte record.
6. Motion whitelist: all sixteen P_MC descriptors accepted; Private/MCX rejected; duplicate and count constraints unchanged.
7. CP3 sequencer regression: Chord, Ratchet, Step 1.5, Seq 1.5.5 and Swing unchanged; step fields remain Base-owned.
8. Sidecar split/merge: Base Motion and P_MC Motion round-trip without duplicate IDs or exceeding the shared 128-event pool.

## 7. Status

- Logical CP2/CP3 ownership and persistence boundaries: **FROZEN**.
- Candidate P_MC numeric range 103–118 and shifted E0–E7 range 119–126: **RESERVED FOR COMPATIBILITY TESTING; NOT YET STABLE ABI**.
- Production parameter enum/descriptors, UI pages and runtime adapters: **NOT IMPLEMENTED**.
- CP1.6 Project Runtime Integration: **OPEN**.
- CP1.7 Preset Runtime Integration: **OPEN**.
- No physical Flash allocation, device write, firmware installation, factory firmware change, or PR merge.
