# ENERGUN METAL CORE — CP1.3 complete V1 parameter inventory

Date: 2026-10-31
Branch: energun-metal-core
Reference: Library file `energun_westcoast_synth_v98_metal_core_follow_metal_on.html`, inspected directly.
Status: source-backed inventory and proposed scope; no parameter IDs or runtime code assigned.

## 1. Counting rules

This inventory distinguishes:
- **Stored sound controls**: independent controls whose values belong to a Metal Core patch.
- **Host controls**: Felucca functions that remain host-owned and are not aliases for internal Metal Core controls.
- **Deferred reference controls**: present in the browser v98 prototype but excluded from the initial FM-1 V1 engine scope.
- **Operational modes**: prototype execution modes that should be represented by Felucca's normal engine/note lifecycle rather than copied as sound parameters.

The exact V1 sound inventory is **37 independent controls**, including the two independent switches FOLLOW and METAL and the four normal voice ADSR values. This corrects the earlier 35-control draft.

## 2. V1 stored sound controls — 37

Ranges/defaults below are taken from the protected v98 HTML parameter object. Units are retained as represented by the source; where the source has no physical unit, the value is normalized or enumerated.

### A. OSCILLATORS / SYNTH VOICE — 8 controls

| # | Control | Range | Default | Type / role |
|---:|---|---:|---:|---|
| 1 | FREQ | 30–500 | 160 | Base oscillator frequency; source UI labels it Freq |
| 2 | WAVE | SINE / TRI / SAW | SINE | Main waveform shared by OSC1 and OSC2 |
| 3 | OSC2 DETUNE | 0–30 cents | 7 | OSC2 fine detune; 0.5-cent source step |
| 4 | OSC2 LEVEL | 0–1 | 0.50 | Direct-audio OSC2 level |
| 5 | TIMBRE | 0–1 | 0 | First fixed wavefolder-stage input gain |
| 6 | WEST COAST DRIVE | 0–1 | 0 | FM and METAL FM depth scaling via drive curve |
| 7 | FOLLOW | OFF / ON | ON | Sequencer note/octave pitch tracking |
| 8 | METAL | OFF / ON | ON | Enables separate METAL FM and METAL ring contributions and selects METAL trigger behavior |

OSC2 is a direct-audio source. It is not the Complex FM modulator. FOLLOW and METAL are independent switches and remain on this page/block.

### B. COMPLEX OSCILLATOR — 11 controls

| # | Control | Range | Default | Type / role |
|---:|---|---:|---:|---|
| 9 | MODULATION | FM / AM | FM | Selects Complex FM or AM path |
| 10 | MOD RATIO | 0.25–16 | 3.07 | Independent wcMod frequency ratio |
| 11 | MOD OSC WAVE | SINE / TRIANGLE / SAW / SQUARE | SINE | Complex modulator waveform |
| 12 | FM CHARACTER | SMOOTH / CLASSIC / METALLIC | METALLIC | FM-index response curve |
| 13 | FM INDEX | 0–1 | 0 | Complex FM base depth |
| 14 | AM DEPTH | 0–1 | 0 | Complex AM depth |
| 15 | FM FEEDBACK | 0–0.8 | 0 | Dedicated delayed feedback amount; reference delay 3 ms |
| 16 | FM ENV AMT | 0–1 | 0.50 | Triggered increase over base FM index |
| 17 | ENV ATTACK | 0.001–1 s | 0.005 s | Complex FM-index envelope attack |
| 18 | ENV DECAY | 0.01–3 s | 0.25 s | Complex FM-index envelope decay |
| 19 | COMPLEX MODULE VOLUME | 0–1 | 1 | Complex oscillator output level |

This is **11**, not 10: COMPLEX MODULE VOLUME is a separate v98 control and was missing from the previous 35-control inventory. MOD RATIO (3.07 default) must never be aliased to METAL RATIO (4.20 default).

### C. METAL-specific ratio / envelope — 2 controls

| # | Control | Range | Default | Type / role |
|---:|---|---:|---:|---|
| 20 | METAL RATIO | 0.5–12 | 4.20 | Frequency ratio for separate METAL FM and METAL ring oscillators |
| 21 | METAL DECAY | 0.05–3 s | 0.40 s | METAL trigger decay; METAL selects a 2 ms attack |

The METAL switch itself is already counted in the OSCILLATORS / SYNTH VOICE section.

### D. WAVEFOLDER — 5 controls

| # | Control | Range | Default | Type / role |
|---:|---|---:|---:|---|
| 22 | FOLD | 0–1 | 0.25 | Blend with second-stage folded transfer |
| 23 | SYMMETRY | -1–1 | 0 | Second-stage transfer offset |
| 24 | WAVEFOLDER DRIVE | 0–1 | 0.20 | Pre-fold gain within second-stage transfer |
| 25 | FOLD MODE | SOFT / CLASSIC / HARD | CLASSIC | Transfer-curve mode |
| 26 | WAVEFOLDER MODULE VOLUME | 0–1 | 1 | Output level after second folder stage |

TIMBRE, WEST COAST DRIVE and WAVEFOLDER DRIVE are three different controls. Stage 1 is a fixed reflection curve; Stage 2 is the adjustable transfer with 4× oversampling in the browser reference. The fixed-point implementation must be independently designed and tested.

### E. MAIN FILTER — 2 controls

| # | Control | Range | Default | Type / role |
|---:|---|---:|---:|---|
| 27 | CUTOFF | 100–8000 Hz | 1200 Hz | Main low-pass filter cutoff |
| 28 | RESONANCE | 0.5–20 | 2.0 | Main low-pass filter Q |

These are main-filter controls, not LPG controls.

### F. LOW-PASS GATE — 3 controls

| # | Control | Range | Default | Type / role |
|---:|---|---:|---:|---|
| 29 | LPG RESPONSE | 0–1 | 0.50 | Transient gain and opening response |
| 30 | LPG DECAY | 0.02–2 s | 0.25 s | Triggered LPG tail |
| 31 | LPG MODULE VOLUME | 0–1 | 1 | LPG gain control |

v98 uses a separate LPG low-pass biquad initially at 1800 Hz, Q 0.7. LPG MODULE VOLUME and the LPG envelope share a Web Audio gain node in the reference. A separate post-envelope multiplier would be a behavior change and requires approval.

### G. VOICE VCA / NORMAL ADSR — 5 controls

| # | Control | Range | Default | Type / role |
|---:|---|---:|---:|---|
| 32 | VOICE VOLUME / VCA LEVEL | 0–1 | 0.50 | Triggered voice VCA peak level |
| 33 | ATTACK | 0.001–1 s | 0.010 s | Normal voice attack |
| 34 | DECAY | 0.05–3 s | 0.30 s | Normal voice decay |
| 35 | SUSTAIN | 0–1 | 0 | Normal voice sustain |
| 36 | RELEASE | 0.01–3 s | 0.20 s | Normal voice release |

### H. FINAL ENGINE OUTPUT — 1 control

| # | Control | Range | Default | Type / role |
|---:|---|---:|---:|---|
| 37 | WEST COAST VOLUME | 0–1 | 1 | Final voice output gain |

This control was also missing from the previous inventory. The v98 source contains conflicting assignments: the direct knob handler writes `VOLUME × 12`, while the recurring volume loop writes `VOLUME × 5`. The intended gain scale must be calibrated against the protected reference. Do not copy either multiplier by assumption.

**Count check:** 8 + 11 + 2 + 5 + 2 + 3 + 5 + 1 = **37 independent stored controls**.

## 3. Conditional host reuse — not an alias decision

| Felucca host parameter/function | Proposed role | Gate |
|---|---|---|
| P_ATK / P_DEC / P_SUS / P_REL | Candidate host normal voice ADSR | Reuse only after v98 vs host retrigger, legato, curve, velocity and gate tests |
| P_LEVEL | Final Felucca track mixer level after engine render | Keep host-owned; does not replace WEST COAST VOLUME or internal module levels |
| P_DETUNE | Felucca UNISON voice spread | Keep host-owned; does not replace OSC2 DETUNE |
| Existing LFO/modulation matrix | No automatic v98 mapping | Defer until routing equivalence is specified |
| Sequencer note events | Source for FOLLOW pitch updates and trigger timing | Use host lifecycle; do not create a duplicate private sequencer |

The 37-control inventory counts the four ADSR values as sound controls even if they are eventually mapped to Felucca's host ADSR. They must not be counted twice in storage after a mapping is approved.

## 4. Deferred v98 controls and prototype-only modes

These controls exist in the v98 HTML but are **not part of the initial V1 sound inventory**:

| Control / function | v98 source state | V1 decision | Reason |
|---|---|---|---|
| RING MIX | 0–1, default 0 | Defer | Independent RING2 path |
| RING RATIO | 0.25–16, default 2.01 | Defer | Independent RING2 oscillator |
| SPREAD | 0–1, default 0.50 | Defer pending route proof | Independent audible stereo routing is not established |
| LFO RATE | 0.05–20 Hz, default 1 | Defer | Custom LFO modulation not in initial scope |
| LFO DEPTH | 0–1, default 0.30 | Defer | Custom LFO modulation not in initial scope |
| RANDOM RATE | 0.1–20, default 2 | Defer | Custom random modulation not in initial scope |
| SLEW | 0.01–2 s, default 0.15 | Defer | Custom random/slew modulation not in initial scope |
| DRONE / SEQ selector | DRONE / SEQ, default SEQ | Map to host note/sequencer lifecycle; no duplicate stored parameter | Prototype execution mode, not a core timbre control |
| WEST COAST ON | OFF / ON, default ON | Use normal host engine/track enable and mute | Avoid duplicate engine-enable state |

The prototype's full patch bay and its custom modulation sources are also deferred. The existing Felucca sequencer and modulation architecture remain the host integration points.

## 5. Parameter/storage implications

- 37 is the complete independent V1 sound-control count before host reuse is finalized.
- Existing E0..E7 provide eight engine edit values.
- At most 16 new common parameter IDs can be inserted before E0 while keeping IDs below 128. This is a hard ceiling, not a target.
- The four host ADSR controls are conditional reuse candidates. P_LEVEL and P_DETUNE are not internal-control substitutes.
- The project object does not have enough free bytes for one additional common parameter per track. The separately versioned sidecar proposal remains necessary if common parameter IDs are added.
- The sidecar flash-sector candidate is not approved. No sector may be used until the user's full FM-1 V1.5 dump is audited.
- No stable parameter IDs, page assignments, storage bytes, or flash addresses are assigned by this document.

## 6. Required decisions before CP1 closure

1. Approve the 37-control V1 inventory and deferred list.
2. Calibrate the conflicting WEST COAST VOLUME behavior against the protected v98 reference.
3. Decide whether Felucca's host ADSR can be reused without unacceptable retrigger/legato differences.
4. Decide which eight controls belong on E0..E7 and which, if any, need new common IDs for direct motion/editor support.
5. Complete sidecar association, backup/restore, migration and power-loss design before any storage implementation.
6. Confirm detailed signal taps and gain staging, then freeze the V1 parameter/page map.

No firmware code, parameter IDs, editor pages, sidecar, flash allocation, firmware package, factory firmware, or physical device was changed.
