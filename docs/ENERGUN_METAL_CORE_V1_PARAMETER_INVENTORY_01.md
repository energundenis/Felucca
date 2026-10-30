# ENERGUN METAL CORE — V1 parameter inventory and storage budget 01

Date: 2026-10-31
Branch: energun-metal-core
Status: inventory baseline for review; no parameter IDs assigned and no firmware code changed.

## 1. Storage limits confirmed from current Felucca source

Current source:
- P_COUNT = 111.
- P_E0 = 103.
- P_LEVEL through P_ESYNC occupy common per-track IDs 0..102 (103 IDs).
- P_E0..P_E7 are the eight engine-specific edit values (IDs 103..110).
- P_COUNT must remain <=127 because bit 7 of a motion parameter ID is reserved for MOTION_LOCK.
- Therefore, at most 16 additional common IDs can be inserted immediately before P_E0: common IDs 103..118, followed by E0..E7 at 119..126. This is the absolute format/protocol ceiling, not a recommendation to consume all 16.
- The existing editor exposes E0..E3 on EDIT 1 and E4..E7 on EDIT 2. Engine descriptors provide labels/ranges/defaults for those eight values.
- Project and user-preset migration maps the last eight stored values to the current E0..E7 positions; adding common parameters before E0 is the established append-before-engine-values pattern. Nevertheless, project/preset/editor/backup regression tests are mandatory before changing P_COUNT.

## 2. V1 control inventory

This is a count of independent stored controls, not a proposed ID assignment. Toggles, enum selections, and continuous values each require storage when their state must survive project/preset save and load.

### A. Voice / oscillators — 6
1. FREQ (base oscillator frequency).
2. OSC1 WAVE.
3. OSC2 LEVEL.
4. OSC2 DETUNE.
5. TIMBRE (first fixed wavefolder-stage input gain).
6. WEST COAST DRIVE (FM-depth scaling; distinct from folder DRIVE).

### B. Complex FM / AM — 9
7. COMPLEX MODE (FM / AM).
8. MOD RATIO.
9. MOD OSC WAVE.
10. FM CHARACTER (SMOOTH / CLASSIC / METALLIC).
11. FM INDEX.
12. AM DEPTH.
13. FM FEEDBACK.
14. FM ENV AMOUNT.
15. FM ENV ATTACK.
16. FM ENV DECAY.

### C. METAL path — 3 engine controls plus the two global switches
17. METAL RATIO.
18. METAL DECAY.
19. METAL enable.
20. FOLLOW enable.

METAL and FOLLOW are separate stored booleans. Their location is the OSCILLATORS / SYNTH VOICE header, not the FOLD module.

### D. WAVEFOLDER — 5
21. FOLD amount.
22. FOLD SYMMETRY.
23. FOLD DRIVE (second-stage folder drive; not WEST COAST DRIVE).
24. FOLD MODE (SOFT / CLASSIC / HARD).
25. WAVEFOLDER MODULE VOLUME.

### E. Main filter — 2
26. CUTOFF.
27. RESONANCE.

### F. LPG — 3
28. LPG RESPONSE.
29. LPG DECAY.
30. LPG MODULE VOLUME.

### G. Normal voice envelope / level — 5
31. Voice VOLUME / VCA level (the v98 wcLevel per-trigger peak).
32. ATTACK.
33. DECAY.
34. SUSTAIN.
35. RELEASE.

The normal ADSR may initially use Felucca P_ATK/P_DEC/P_SUS/P_REL subject to the lifecycle audit's required curve/retrigger/legato calibration. P_LEVEL remains the host final track mixer level and is not item 31.

## 3. Count and scope boundary

The current inventory contains 35 independently stored engine controls:
- 35 controls total, including the independent METAL and FOLLOW switches.
- 8 existing E0..E7 slots can store 8 engine controls.
- Up to 16 additional common IDs are technically available before P_E0.
- The remaining values require the approved extension storage (Sidecar), unless a future audit proves an existing parameter has exactly matching semantics.

This inventory deliberately excludes:
- RING2 (RING MIX / RING RATIO), deferred from V1;
- full custom PATCH BAY, deferred from V1;
- custom LFO/RANDOM/SLEW modulation controls unless a later V1 scope change is approved;
- SPREAD until its v98 routing is fully verified;
- sequencer controls, which remain Felucca host functionality;
- FX/EQ controls outside the approved core voice path.

## 4. Reuse candidates and cautions

- P_ATK/P_DEC/P_SUS/P_REL: conditional reuse for normal amplitude ADSR only. Curve, retrigger, legato, velocity and accent parity must be measured.
- P_LEVEL: retain as host final track mixer level; not a substitute for the v98 voice VCA level.
- P_DETUNE: retain host UNISON spread semantics; not OSC2 DETUNE.
- Existing LFO and modulation matrix: no automatic mapping to v98 patchbay; full custom patchbay remains deferred.
- No current common parameter is approved as an alias for FREQ, OSC2 level/detune, FOLLOW, METAL, ratios, FM, FOLD, filter or LPG controls.

## 5. Storage allocation principles — not yet an ID map

1. Keep the eight E0..E7 engine edit values for the eight most useful engine controls shown across the two existing EDIT pages. Exact selection and ordering are pending UI review.
2. Use any of the 16 new common IDs only for controls that genuinely need Felucca's common parameter protocol, motion-lock compatibility, or direct editor access. Do not consume them simply because they are available.
3. Store the remaining independent engine values in the approved per-track Sidecar extension.
4. Sidecar must be versioned, CRC-protected, A/B committed, matched to the active project generation/CRC, and included in project backup/restore.
5. Sidecar motion/parameter-lock events need an explicit extended-ID encoding; do not set bit 7 or collide with existing MOTION_LOCK semantics.
6. Missing/invalid Sidecar must load the project safely with documented defaults and must never block loading a legacy project.
7. Do not allocate IDs until the editor, project/preset formats, backup protocol, motion recording and restore paths have a complete regression plan.

## 6. Next decision gate

Before stable IDs are assigned:
- decide which eight controls deserve E0..E7;
- decide whether any of the 16 common IDs are needed in V1 or whether the new controls should remain engine-local in Sidecar;
- verify whether the existing EDIT pages can present the eight values with useful labels and units;
- design Sidecar editor read/write, project association, backup/restore and motion event handling;
- test old project and user-preset compatibility with a synthetic P_COUNT=127 build.

No IDs were assigned. No source code, editor pages, storage format, firmware image, factory firmware, or physical device were changed.
