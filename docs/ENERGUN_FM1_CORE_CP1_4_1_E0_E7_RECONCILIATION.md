# ENERGUN FM-1 CORE — CP1.4.1 E0–E7 and P_MC allocation reconciliation

Date: 2026-11-02  
Repository: `energundenis/Felucca`  
Branch: `energun-metal-core`  
Status: **RECONCILED FROM PREVIOUSLY ACCEPTED USER DECISIONS — NO NUMERIC IDS OR FIRMWARE ABI ASSIGNED**

## 1. Decision authority and chronology

The controlling earlier user-approved Engine EDIT freeze is the 2026-10-30 decision explicitly accepted with «фиксируем»:

| Slot | Frozen control |
|---|---|
| E0 | FREQ |
| E1 | OSC2 LEVEL |
| E2 | TIMBRE |
| E3 | WEST COAST DRIVE |
| E4 | METAL RATIO |
| E5 | FOLD |
| E6 | CUTOFF |
| E7 | RESONANCE |

The later CP1.4 work separately adopted the logical ownership split of:
- 8 Engine EDIT controls;
- 16 common continuous P_MC controls;
- 13 Private controls per part.

No later explicit user decision superseding the 2026-10-30 E0–E7 freeze was found in the available project history. Therefore the earlier E0–E7 freeze remains authoritative; the later CP1.4 candidate E ordering is not adopted.

## 2. Reconciliation rule

The 37-control source inventory contains 37 independent controls. The later candidate P_MC list accidentally repeated two controls already present in the earlier frozen E0–E7 map:
- OSC2 LEVEL (frozen E1);
- TIMBRE (frozen E2).

Keeping those duplicates would count only 35 unique controls while claiming 37. Treating them as aliases would violate the explicit independent-control/non-alias rules.

The consistent set operation is:
1. Preserve all eight previously frozen E0–E7 controls unchanged.
2. Preserve the accepted 13 Private controls per part unchanged.
3. Define P_MC as the remaining 16 unique continuous controls from the approved 37-control inventory.

This removes OSC2 LEVEL and TIMBRE from P_MC and includes FM INDEX and LPG DECAY in P_MC. FM INDEX and LPG DECAY were in the later candidate E list, but are not in the controlling frozen E0–E7 list.

## 3. Reconciled logical allocation

### 3.1 Engine EDIT — unchanged from the accepted freeze

| Slot | Control | Type | Source range | Default | Source step / unit |
|---|---|---|---:|---:|---|
| E0 | FREQ | Continuous | 30–500 Hz | 160 Hz | 1 Hz |
| E1 | OSC2 LEVEL | Continuous | 0–1 | 0.50 | 0.01 |
| E2 | TIMBRE | Continuous | 0–1 | 0 | 0.01 |
| E3 | WEST COAST DRIVE | Continuous | 0–1 | 0 | 0.01 |
| E4 | METAL RATIO | Continuous | 0.5–12 | 4.20 | 0.01 |
| E5 | FOLD | Continuous | 0–1 | 0.25 | 0.01 |
| E6 | CUTOFF | Continuous | 100–8000 Hz | 1200 Hz | 10 Hz |
| E7 | RESONANCE | Continuous | 0.5–20 | 2.0 | 0.1 |

### 3.2 Common P_MC — corrected 16 unique continuous controls

| # | Control | Type | Source range | Default | Source step / unit |
|---:|---|---|---:|---:|---|
| 1 | OSC2 DETUNE | Continuous | 0–30 cents | 7 cents | 0.5 cents |
| 2 | FM INDEX | Continuous | 0–1 | 0 | 0.01 |
| 3 | MOD RATIO | Continuous | 0.25–16 | 3.07 | 0.01 |
| 4 | AM DEPTH | Continuous | 0–1 | 0 | 0.01 |
| 5 | FM FEEDBACK | Continuous | 0–0.8 | 0 | 0.01 |
| 6 | FM ENV AMT | Continuous | 0–1 | 0.50 | 0.01 |
| 7 | FM ENV ATTACK | Continuous | 0.001–1 s | 0.005 s | 0.001 s |
| 8 | FM ENV DECAY | Continuous | 0.01–3 s | 0.25 s | 0.01 s |
| 9 | METAL DECAY | Continuous | 0.05–3 s | 0.40 s | 0.01 s |
| 10 | FOLD SYMMETRY | Continuous | -1–1 | 0 | 0.01 |
| 11 | WAVEFOLDER DRIVE | Continuous | 0–1 | 0.20 | 0.01 |
| 12 | LPG RESPONSE | Continuous | 0–1 | 0.50 | 0.01 |
| 13 | LPG DECAY | Continuous | 0.02–2 s | 0.25 s | 0.01 s |
| 14 | VOICE VCA LEVEL | Continuous | 0–1 | 0.50 | 0.01 |
| 15 | WAVEFOLDER MODULE VOLUME | Continuous | 0–1 | 1 | 0.01 |
| 16 | WEST COAST VOLUME | Continuous | 0–1 | 1 | 0.01 |

### 3.3 Private — unchanged

The 13 Private values remain WAVE, FOLLOW, METAL, MODULATION, MOD OSC WAVE, FM CHARACTER, COMPLEX MODULE VOLUME, FOLD MODE, LPG MODULE VOLUME, ATTACK, DECAY, SUSTAIN and RELEASE. Each of four parts owns an independent bank.

## 4. Count and duplicate checks

- Engine EDIT: 8 unique controls.
- P_MC: 16 unique controls.
- Private: 13 unique controls per part.
- Total unique sound controls: 8 + 16 + 13 = **37**.
- OSC2 LEVEL and TIMBRE occur only in E1/E2, not in P_MC.
- FM INDEX and LPG DECAY occur in P_MC, not in E0–E7.
- No control is aliased, silently removed or counted twice.
- MCX0–MCX6 remain a separate optional extension namespace outside the 37-control count.

## 5. Downstream consequences

1. Update the CP1.4.1 logical map and the CP1.4.3/4.4 symbolic Motion Lock candidate list to use this membership.
2. P_MC whitelist candidates include FM INDEX and LPG DECAY; they exclude OSC2 LEVEL and TIMBRE because those are E1/E2.
3. The existing CP1.4.2 conversion reference already contains both FM INDEX and LPG DECAY among its 30 continuous-control mappings; its physical curves and quantization results are unchanged by this membership correction.
4. Sidecar capacity remains 16 P_MC values + 13 Private values per part; byte-count estimates do not change.
5. The accepted standard Felucca 7-bit value architecture remains unchanged.
6. Stable numeric IDs, descriptor registration, editor page IDs, Sidecar field offsets, storage object IDs and physical Flash sectors remain unassigned.

## 6. Acceptance boundary

This reconciliation applies the user's previously accepted E0–E7 freeze together with the later accepted 8/16/13 logical ownership split. It does not request or imply a new sound-design choice.

It does not authorize:
- changing the factory firmware;
- assigning stable numeric IDs or editor command/page IDs;
- changing project, preset, Sidecar or archive binary layouts;
- registering METAL CORE or merging source branches;
- selecting physical Flash sectors or writing to the FM-1.

**CP1.4.1 logical membership and E0–E7 ordering are reconciled. CP1.4.2 curve sign-off, stable-ID allocation and persistence implementation remain open.**
