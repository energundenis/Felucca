# ENERGUN FM-1 CORE — cross-branch consolidation and canonical decision register

Date: 2026-11-02  
Canonical repository: `energundenis/Felucca`  
Canonical working branch: `energun-metal-core`  
Status: **DOCUMENTATION CONSOLIDATED; SOURCE BRANCHES PRESERVED; NO CODE MERGE OR DEVICE WRITE**

## 1. Purpose and authority

This register is the current cross-workstream project reference. It reconciles decisions and evidence found in the active branch and parallel development branches. It does not silently merge source code, rewrite history, or promote host-only prototypes to production.

Authority order when documents conflict:
1. Explicitly accepted user decisions recorded in a decision document.
2. Later source-trace / architecture decisions that explicitly supersede earlier recommendations.
3. Verified source facts from the protected v98 reference and pinned Felucca host source.
4. Host test evidence, with its exact scope.
5. Proposals and recommendations, which remain non-binding until accepted.

The branch-local source documents remain intact. This canonical register records their exact branch and commit context so that their provenance is not lost.

## 2. Canonical project state

### Product and safety
- Target: custom ENERGUN firmware for the physical M-VAVE FM-1, based on Felucca.
- The factory M-VAVE V1.5 firmware remains protected. **Accepted project premise (user-confirmed 2026-11-02): the factory firmware image already available to the project is 100% identical to the firmware on the user's physical FM-1. Treat this image as the exact factory baseline for this unit; do not schedule another device read solely to recreate that image.**
- No physical device read, SysEx write, Flash write, firmware installation, bootloader operation or recovery operation has been performed by this consolidation.
- Before the first physical write, verify the integrity and availability of the accepted factory image and confirm a practical restore route. A second device dump is not a prerequisite under the accepted premise.
- No production engine registration, persistent-format change, stable parameter-ID assignment or PR merge is authorized by this documentation update.

### Current project truth
- METAL CORE DSP primitives, VCA, adapter and storage design exist at different maturity levels; they are not one integrated, registered production engine.
- CP2.23 is a passing VCA-only real-track integration harness, not a complete METAL CORE integration.
- CP3 lifecycle tests and CP3 DSP/filter/platform workstreams are branch-local; exact-head CI and target qualification are not uniformly complete.
- Sequencer V0.3 JSON/dry-run bridge is host/offline only; no physical device write has been validated.
- The separate Mac/M-Flasher two-read backup workflow is no longer an active development stream: it is superseded for planning purposes by the user-confirmed exact factory image already held by the project. Keep its technical notes as reference only; do not spend parallel development time on duplicate reads.

## 3. Approved sound and control semantics

### Parameter inventory and allocation
- Logical inventory: 37 independent controls = 8 Engine EDIT + 16 common P_MC + 13 Private controls per part.
- Private state is four independent banks (52 values total), one bank per part; it is not volatile `engine_t.mem`.
- FOLLOW and METAL are independent persistent booleans.
- MCX0–MCX6 remain a separate extension namespace, outside the 37-control count.
- E0–E7, P_MC and Private membership are logically documented; **stable numeric IDs, binary encodings, page IDs, Sidecar offsets and Flash addresses are not assigned**.
- Important unresolved conflict: the earlier 2026-10-30 E0–E7 freeze differs from the later CP1.4 candidate ordering. CP1.4.2 must not assign stable IDs until this is reconciled.

### Resolved reference behavior / V1 decisions
| Topic | Canonical decision | Evidence / boundary |
|---|---|---|
| FM CHARACTER | Default METALLIC (2); retain SMOOTH/CLASSIC/METALLIC curves. Initialize UI from the same canonical value. | CP1.13 source trace; static HTML selector mismatch before audio init is documented. |
| WEST COAST VOLUME | Stable v98 output mapping is VOLUME × 5; transient ×12 writes are overwritten by the recurring loop. | CP1.5 source trace. |
| LPG MODULE VOLUME | Approved V1 Option C: trigger peak = (0.25 + 0.75 × RESPONSE) × MODULE VOLUME; one LPG gain stage, RESPONSE attack and DECAY tail retained. | CP1.8 / CP1.12; isolated peak helper exists, engine integration is pending. |
| FOLLOW / MOD RATIO | Option A: FOLLOW OFF = panel FREQ × MOD RATIO; FOLLOW ON = tracked note frequency × 1.5. MOD RATIO does not alter FOLLOW ON. | Approved decision 2026-10-31. |
| VCA ownership | Dedicated engine-owned VCA matching v98 behavior; `ownenv=1`; do not cascade host ADSR. Host still owns voice allocation/lifecycle and supplies ramps/fades that are multiplied exactly once. | CP1.14; CP1.15; CP3.0 host contract. |
| ADSR controls | ATTACK/DECAY/SUSTAIN/RELEASE remain independent Private controls; no alias to host P_ATK/P_DEC/P_SUS/P_REL. | CP1.14 supersedes earlier CP1.12 recommendation. |
| Short Gate | Option B: Gate-off / Release starts from instantaneous level; suppress any pending Decay that would reopen the VCA after Release. | CP1.16 accepted; implementation/regression still pending. |
| Host gate timing | First integration sees Gate-off at render-block boundary; sample-accurate intra-block release requires a separate host API/event change. | CP3.0 source audit. |

### Still open before stable parameter ABI
- Reconciliation of the older frozen E0–E7 order with CP1.4 candidate ordering (still open; earlier frozen order remains authoritative until explicit supersession).
- Target stored-value precision: the host project/preset/motion representation is limited to -64..127; several v98 source steps exceed 192 codes. Choose standard quantization or high-resolution sidecar-backed values.
- Final sign-off of the complete map against the protected source artifact.
- Stable ID namespace and 7-bit parameter-ID capacity constraints.
- Serialization and migration behavior for old projects/presets.
- P_MC Motion Lock whitelist/ranges and 128-record pool audit.
- Final Sidecar schema, A/B transaction, CRC, capacity, stale-generation and power-loss tests.
- Physical Flash sector map remains blocked pending the user's own complete V1.5 dump and recovery audit.

## 4. DSP and engine integration status

### DSP Stage 1–7 branch
Source branch: `metal-core-dsp-stage1-osc`  
Audited branch HEAD: `cf22f01b23f618feda65ccde85b0503fccacc7b0`

Artifacts include OSC1/OSC2, Complex Modulator, feedback delay/route, part-state lifecycle prototype, Felucca adapter audit and FOLLOW/MOD RATIO decision.

- Stage 1–6: isolated implementation/prototype and host evidence, not target-hardware proof.
- Stage 7: unregistered callback adapter prototype; host syntax/callback checks are not a full target build.
- The engine is not registered in production `engines.c` / `ENGINES[]`.
- Approved FOLLOW behavior is as listed in §3.
- Feedback delay remains a candidate: 3 ms at the source-derived 44,117.647 Hz frame rate = 132.35294 samples; 133-slot ring, fractional Q15 split 21203/32768 and 11565/32768. Host stability tests and compile-only target probes do not prove target callback timing, audio equivalence or production DSP safety.

### VCA
- CP1.14 architecture decision: accepted.
- CP1.15 source specification: source trace recorded; exact numeric/sample-domain port and lifecycle integration remain gated.
- CP1.16 short-Gate rule: accepted; implementation/regression pending in that decision record.
- CP2.23: VCA-only real-track adapter harness passed; eight independent VCA states, 36 B per voice / 288 B per part, 98 reference renders unchanged, regression 0. This is not full engine integration.
- CP3 host contract: `ownenv=1`, velocity/accent supplied explicitly, host amplitude ramp/fade multiplied once, Gate-off block-boundary in first integration.
- Lifecycle matrix exact-head CI was reported pending in the audit; do not call it passed until a run on the current exact HEAD is recorded.

### CP3 parallel workstreams
| Branch | Audited HEAD | Work | Status |
|---|---|---|---|
| `energun-fm1-cp3-host-integration` | `9dab2aac5c4170413437da1daf393aa93b5246a2` | Real Felucca lifecycle contract tests, state accounting, VCA adapter | Test-only; engine unregistered; exact-head CI pending in recorded audit |
| `energun-fm1-cp3-dsp-filter` | `9c113f2cdf8a197a4b1a0461814d742f54fe2b42` | Candidate fixed-point DF2T low-pass filter qualification | Host-only candidate; not production-approved |
| `energun-fm1-cp3-platform-qa` | `fd60d047354a530d433a6e3b3e4fccd8f24a19a4` | AC79 compiler probes, disassembly, memory/package/recovery audit | Qualification workstream launched; not target qualification closure |

CP3.1 state accounting remains partial. Exact primitive sizes are recorded, but full per-part state layout, filters, FM envelope, feedback lifecycle, alignment and target CPU/RAM remain incomplete. 96 kHz is high risk and not approved for V1; 44.1 kHz is the initial reference target, subject to final target qualification.

## 5. Sequencer V0.3 bridge

Source branch: `fm1-dryrun-pattern-20261031`  
Audited HEAD: `4d66cbf5d08f1abe33563fac8d288d9ff1561439`

- V0.3 accepted data model: on/rest, scale-relative note, octave, accent boolean, gate percentage; pattern lengths 8/16/32.
- JSON Bridge, Mapping Audit, offline converter and 64-step dry-run are prepared; recorded Python 3.11 CI: 6/6 unit tests passed.
- Mapping: root/scale to FM-1 P_ROOT/P_SCALE; active/rest to ST_NOTE/ST_REST; complete replacement fills trailing steps with REST and retains a 64-step before-image for rollback.
- Accent is not one-to-one: FM-1 SF_ACCENT forces velocity 127, while V0.3 has graded velocity. User-facing mapping decision and listening test remain open.
- Per-step Gate remains unresolved: P_SGATE is track-wide and Motion Lock support is not hardware-verified; 128 shared motion-record capacity must be audited.
- No physical FM-1 read/write, SysEx write or live integration test has occurred.

## 6. Factory image and recovery gate

**Accepted premise, user-confirmed 2026-11-02:** the factory firmware image already available to the project is 100% identical to the firmware on the user's physical FM-1. It is therefore treated as the exact factory firmware baseline for this unit. No separate device read or duplicate two-read backup workflow is scheduled.

The branch `fm1-factory-backup-plan-20261031` (audited HEAD `053129351fd951fe364ee01958cdf7dee94d0255`) and its macOS M-Flasher 0.7.0 procedure remain reference material only; the workflow is not an active parallel stream.

Before the first physical write, require:
1. Verify the accepted factory image file is present, intact and preserved unchanged.
2. Confirm a practical restore route for this FM-1 and the required tool/connection mode.
3. Audit the exact before/after region diff and write scope.
4. Obtain separate explicit user authorization.

This is a pre-write safety checkpoint, not a separate development workstream. No device read or write is authorized by this decision.

## 7. Source branch inventory and provenance

| Branch | Audited HEAD / commit | Principal contribution |
|---|---|---|
| `energun-metal-core` | includes CP1.4.1 correction commit `73aafa76d252a18fe1843b1a06ff860367561b00` | Canonical working branch; parameter access/storage docs and consolidated project base |
| `metal-core-dsp-stage1-osc` | `cf22f01b23f618feda65ccde85b0503fccacc7b0` | DSP Stage 1–7 and FOLLOW/MOD RATIO |
| `energun-fm1-cp3-host-integration` | `9dab2aac5c4170413437da1daf393aa93b5246a2` | CP1.5, CP1.8, CP1.12–16, CP2.23, CP3 host contract and memory evidence |
| `energun-fm1-cp3-dsp-filter` | `9c113f2cdf8a197a4b1a0461814d742f54fe2b42` | Fixed-point filter qualification plan |
| `energun-fm1-cp3-platform-qa` | `fd60d047354a530d433a6e3b3e4fccd8f24a19a4` | AC79/toolchain/platform qualification |
| `fm1-dryrun-pattern-20261031` | `4d66cbf5d08f1abe33563fac8d288d9ff1561439` | Sequencer V0.3 JSON bridge and offline dry-run |
| `fm1-factory-backup-plan-20261031` | `053129351fd951fe364ee01958cdf7dee94d0255` | macOS read-only factory backup preparation |

Additional repositories referenced by the sequencer bridge:
- `energundenis/energun-generative-sequencer` — accepted `versions/v0.3.html` and separate `versions/v0.3-fm1-bridge.html`.
- `energundenis/ENERGUN-FM1-ANALYSIS` — analysis artifacts.
- `energundenis/energun-wc-fm1` — related FM-1 analysis/development material.

## 8. Canonical work plan from this consolidated state

1. **CP1.4.1 correction review** — parameter semantics corrected in the canonical branch; remaining open items are FREQ units/steps and E0–E7 reconciliation.
2. **CP1.4.2** — ID namespace, 7-bit capacity, project/preset serialization and backward-compatibility audit. Do not allocate IDs until step 1 is closed.
3. **Parallel CP3 evidence closure** — exact-head lifecycle CI, target compiler disassembly review, filter candidate numerical qualification, complete memory accounting.
4. **CP1.4.3–4.4** — Private API/UI per-part read/write/display and P_MC Motion Lock whitelist/regression.
5. **CP1.4.5–4.7** — Sidecar schema, capacity and A/B transaction/power-loss tests.
6. **CP1.4.8–4.10** — project/preset integration, four stored projects, autosave, user presets, Backup/Restore and migration regression.
7. **CP1.4.11** — physical Flash audit against the accepted exact factory image and known platform constraints; no new device dump is required under the user-confirmed premise. Resolve any device-specific non-firmware data/sector questions before write scope is approved.
8. **Engine integration gate** — only after parameter ABI, lifecycle, DSP/filter, memory and target CPU gates pass; separate reviewable change and explicit approval before registration/merge.
9. **Sequencer live integration** — only after read-only compatibility and device-specific recovery gates; resolve accent and Gate mapping separately.

## 11. Latest audit and CI updates — 2026-11-02

### CP1.4.1 source parameter verification
- The protected v98 source object confirms FREQ = 160, min 30, max 500, step 1; the engine assigns it directly to Web Audio OscillatorNode.frequency, so the unit is Hz.
- All previously unspecified source steps in the logical map were recovered from the protected v98 parameter object and added to CP1.4.1.
- CP1.4.1 remains open only for the older frozen E0–E7 allocation conflict and final map sign-off. No final E descriptors or stable IDs may be assigned until the conflict is resolved.
- Updated map: `docs/ENERGUN_METAL_CORE_CP1_4_1_FROZEN_PARAMETER_MAP.md`.

### CP1.4.2 compatibility audit
- Added `docs/ENERGUN_FM1_CORE_CP1_4_2_COMPATIBILITY_AUDIT.md`.
- Confirmed `P_COUNT=111`, `P_E0=103`; 16 common IDs would yield `P_COUNT=127`, `P_E0=119`, within the 7-bit ID ceiling but consuming all common-ID headroom.
- Count-based migration preserves the final eight E values and corresponding motion IDs. FUN10 rejects `np > P_COUNT`; naive 127-value project writes would be unreadable by the factory firmware and exceed the fixed 3840-byte project object.
- Recommended compatibility strategy for review: keep base FUN10 at legacy `np=111` and pair P_MC/private values in a sidecar. This is a proposal, not approved implementation.
- User presets have 144 parameter bytes and two persisted banks of 16 slots. Private data must be represented for all 32 presets, not only five project identities; this expands logical sidecar scope to seven identities × A/B = 14 sectors if separate bank sidecars are used.
- Critical new finding: project/preset/motion parameter values are stored in -64..127, one byte after offset. Several v98 source ranges/steps require more than 192 codes. Target quantization versus high-resolution sidecar-backed values remains undecided.

### CP1.4.3 / CP1.4.4 host source audit
- Added `docs/ENERGUN_FM1_CORE_CP1_4_3_4_PRIVATE_ACCESS_MOTION_AUDIT.md`.
- Private access gap confirmed: current engine API exposes eight EDIT descriptors only; no generic persistent per-part private bank/UI API exists. `engine_t.mem` is volatile and cannot hold patch values.
- Motion Lock gap confirmed: `motion_param()` admits E0+ but does not admit common IDs inserted before E0. P_MC needs an explicit whitelist. `MOTION_MAX=128` is a shared four-track event pool, not 128 events per track.
- Current `P_COUNT=111`, `P_E0=103`; adding 16 common IDs before E0 would produce `P_COUNT=127`, `P_E0=119`, within the 7-bit ID ceiling but consuming all common-ID headroom. FUN10's main project object has only three spare bytes; P_MC/private persistence therefore remains a sidecar concern.
- CP1.4.3 and CP1.4.4 are source-audited, not implemented or closed. CP1.4.2 compatibility constraints are source-audited, but quantization and base-project compatibility decisions remain open.

### CP1.20 CI completion
- Exact source HEAD `42e79344b561df8b106d4183c593b010ae8b88c9`: full baseline run `41356473081` SUCCESS; focused Metal Core run `41356472886` SUCCESS, 15 tests passed.
- Full baseline confirms pinned AC79 SDK, unmodified Felucca firmware build, AC79 VCA compile-only probe and complete host/web regression. It does not establish a production METAL CORE target build or hardware validation.
- Both uploaded artifacts were present and unexpired at audit time. Subsequent commits through the documentation audit head changed documentation only; no firmware, tests, workflow, submodule or build scripts changed.
- CP1.20 was updated with these final CI results.

## 9. Explicit non-actions

This consolidation:
- does not merge or cherry-pick any source branch;
- does not register METAL CORE in production;
- does not assign stable parameter IDs or change storage formats;
- does not alter factory firmware or query/write the physical FM-1; the exact factory image is treated as already available per the user's confirmation;
- does not authorize PR merge, firmware packaging or flashing.

## 10. Source documents

Key source records, with branch-local provenance:
- CP1.5 v98 behavior: `energun-fm1-cp3-host-integration:docs/ENERGUN_METAL_CORE_CP1_5_V98_BEHAVIOR_VERIFICATION.md`
- CP1.8 LPG trace: `energun-fm1-cp3-host-integration:docs/ENERGUN_METAL_CORE_CP1_8_LPG_VOLUME_SOURCE_TRACE.md`
- CP1.12 V1 decision package: `energun-fm1-cp3-host-integration:docs/ENERGUN_METAL_CORE_CP1_12_V1_DECISION_PACKAGE.md`
- CP1.13 FM Character: `energun-fm1-cp3-host-integration:docs/ENERGUN_METAL_CORE_CP1_13_FM_CHARACTER_STARTUP_DECISION.md`
- CP1.14 VCA ownership: `energun-fm1-cp3-host-integration:docs/ENERGUN_METAL_CORE_CP1_14_VCA_OWNERSHIP_DECISION.md`
- CP1.15 VCA specification: `energun-fm1-cp3-host-integration:docs/ENERGUN_METAL_CORE_CP1_15_VCA_SOURCE_SPEC.md`
- CP1.16 short Gate: `energun-fm1-cp3-host-integration:docs/ENERGUN_METAL_CORE_CP1_16_SHORT_GATE_VCA_DECISION.md`
- DSP adapter: `metal-core-dsp-stage1-osc:docs/ENERGUN_METAL_CORE_DSP_STAGE7_FELUCCA_ADAPTER_AUDIT.md`
- FOLLOW / MOD RATIO: `metal-core-dsp-stage1-osc:docs/ENERGUN_METAL_CORE_FOLLOW_MOD_RATIO_DECISION_01.md`
- CP3 host contract and memory: `energun-fm1-cp3-host-integration:docs/ENERGUN_METAL_CORE_CP3_0_HOST_CONTRACT_AUDIT.md`, `...CP3_1_STATE_MEMORY_ACCOUNTING.md`
- Sequencer mapping: `fm1-dryrun-pattern-20261031:docs/ENERGUN_V03_FM1_PATTERN_MAPPING_AUDIT_02.md`
- Factory image/recovery reference (not an active stream): `fm1-factory-backup-plan-20261031:docs/FM1_FACTORY_V15_BACKUP_PLAN_MACOS.md`

---
This register is the canonical cross-branch status snapshot as of 2026-11-02. If a source branch advances, update its audited HEAD and re-evaluate the affected decisions before treating its new contents as canonical.
