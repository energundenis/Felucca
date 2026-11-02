# ENERGUN FM-1 CORE — development checkpoint CP1.20

Date: 2026-11-01
Coordination branch: `energun-metal-core`
Checkpoint commit before this document: `4fc80496e6acfa459eec574974a2121f8912b14b`
Status: stage recorded; CP1 remains OPEN; CP3 production integration remains gated.

## 1. Purpose

Record the current development stage after the user requested automation and acceleration. This is a documentation and workflow-efficiency checkpoint. It does not claim that the firmware engine is complete, that CP1 is closed, or that the device is ready to install.

## 2. Current verified repository state

- Repository: `energundenis/Felucca`.
- Active coordination branch: `energun-metal-core`.
- Branch HEAD at audit: `4fc80496e6acfa459eec574974a2121f8912b14b`.
- `main` remains untouched.
- PR #1 (isolated Q2.30 DF2T biquad prototype) remains Draft and unmerged.
- PR #2 (host lifecycle workstream) remains Draft and unmerged.
- No release or installable ENERGUN FM-1 firmware package is established.

## 3. CI evidence at checkpoint

For exact HEAD `4fc80496e6acfa459eec574974a2121f8912b14b`:

### Fast focused regression

- Workflow run: `41338093488`.
- Workflow: ENERGUN METAL CORE fast regression.
- Result: completed successfully.
- Job: Focused Metal Core host tests — success.
- The workflow invokes `tests/run_metal_core_fast.sh`, which compiles and runs the 11 focused tests listed in that script.
- This is host regression evidence only; it is not an AC79 target runtime test or a complete firmware integration test.

### Full baseline build

- Workflow run: `41338093471`.
- Workflow: ENERGUN FM-1 CORE baseline build.
- At the time of this checkpoint audit: still in progress.
- Confirmed successful steps: checkout/submodules, pinned AC79 SDK verification, unmodified Felucca firmware build, and AC79 pi32v2 VCA compile-only probes.
- The Felucca host/web regression step was still running; final workflow conclusion and artifact upload were not yet confirmed.
- Do not report the full workflow as passed until GitHub records a completed/success conclusion.

### Previous run handling

Superseded runs for earlier intermediate commits were cancelled by the branch concurrency policy after subsequent commits arrived. Their cancellation is not a failure of the latest exact-HEAD fast regression.

## 4. CI acceleration change

Two workflow files were updated on `energun-metal-core`:

- `.github/workflows/energun-fm1-baseline.yml`
- `.github/workflows/energun-metal-core-fast.yml`

Both now use `paths-ignore` for documentation-only paths (`docs/**` and root-level `*.md`) on push and pull-request triggers. Explicit `workflow_dispatch` remains available.

Intent:
- documentation-only commits do not trigger the full firmware build or focused code regression;
- code, tests, build scripts and firmware changes continue to trigger the relevant workflows;
- this reduces redundant CI time without weakening the code-change validation path.

The filters were checked by fetching both files from the updated branch. The workflow runs above were triggered by the workflow-file commits themselves, so they provide evidence that the changed workflows were parsed and executed. A dedicated docs-only push test has not been separately recorded.

## 5. Automation

A daily task titled `Продолжать ENERGUN FM-1` was successfully created and is enabled in ChatGPT automation.

Its instructions are to:
- inspect current branch, files, PRs and CI before acting;
- make small independently verifiable changes on `energun-metal-core`;
- run focused tests for code changes and full build when firmware/build/toolchain changes;
- prefer tests that execute real production source over copied behavioral models;
- update docs and acceptance criteria;
- stop and request the user's decision for unresolved architecture or storage decisions.

The automation is not evidence of continuous background development. No completed automation run was confirmed at this checkpoint.

## 6. Stage assessment

| Workstream | Recorded state |
|---|---|
| CP0 — repository/baseline audit | Completed in prior checkpoint |
| CP1 — specification and architecture | Open; approved decisions recorded, several closure gates unresolved |
| CP1.18 — VCA note-source mapping audit | Documented |
| CP1.19 — host voice/sequencer lifecycle source audit | Documented |
| CP3.1 — isolated biquad candidate | Focused host tests and AC79 compile-only probe passed on its candidate SHA; production codegen/runtime claim remains limited |
| CP3 — actual Felucca host lifecycle integration | Not complete; real dispatcher/sequencer integration test gap remains |
| CP4–CP7 | Not ready to claim complete |
| Installable firmware/device validation | Not established |

## 7. Next execution priorities

1. Confirm the final conclusion and artifacts for full baseline run `41338093471`.
2. Inspect the actual Felucca host test build seams and dependencies for `firmware/src/voice.c` and `firmware/src/seq.c`.
3. Build a test that compiles/links and executes the real production dispatcher/sequencer path with the METAL CORE test descriptor, if technically feasible without a copied model.
4. If direct compilation is blocked by platform dependencies or static linkage, document the exact blockers and the smallest safe seam; do not mislabel a source trace as an integration test.
5. Keep CP1 parameter-access allocation (including 8/16/13), Sidecar payload details and physical Flash layout unresolved until the user approves the required decisions and complete FM-1 V1.5 dump/recovery evidence exists.
6. Keep production engine registration gated until the required integration and compatibility checks pass.

## 8. Safety and scope

- No change to `main`.
- No PR merge.
- No factory firmware modification.
- No physical device access or flashing.
- No Flash write or physical sector allocation.
- No stable parameter/engine IDs assigned.
- No claim that a compile-only target probe proves target runtime performance.
- This checkpoint records evidence and workflow changes only; it does not authorize installation.


## CP3 progress update — 2026-11-01

- Added `tests/metal_core_cp3_real_dispatch_test.c`, compiled against the real Felucca host simulation sources. It executes production `voice.c` and `seq.c` functions, not a reimplementation.
- Added the new test to `tests/run_metal_core_fast.sh`; fast lane now runs 12 focused tests.
- Exact HEAD: `4cf1b7011f5e7c45f32a1f018ca1dddba9df535e`.
- Fast regression run `41339112020`: SUCCESS. The new dispatcher/sequencer test's 19 checks passed.
- Coverage: POLY, MONO, LEGATO, UNISON, actual Note Off and host render retirement, actual sequencer step and Gate expiry, explicit Accent provenance, velocity 127 without Accent metadata, engine-switch queue provenance.
- Limitation: this exercises the actual Felucca dispatcher and sequencer with the existing ANALOG engine callback. It does not yet route the METAL CORE VCA callback through a production engine registry or prove the complete METAL CORE audio graph.
- Full baseline run `41339111968` on this exact HEAD was still in progress at checkpoint time; do not count it as passed until its final result is confirmed.
- Next: create a safe test seam for the METAL CORE own-envelope callback through the real host dispatcher, then cover shared voice stealing/fade and full engine state/memory.


## CP3 progress update — real METAL VCA callback through Felucca

- Added `tests/metal_core_cp3_real_vca_dispatch_test.c` and a compile-time host-only registry seam in `firmware/src/engines.c`. The registry remains immutable in normal firmware builds; no production engine ID or selectable engine was added.
- The test uses Felucca's actual `voice.c`, `seq.c`, `track_render()` and `engine_block()` code, with a private descriptor that invokes the real METAL VCA module.
- Verified: `ownenv=1` host ADSR bypass, explicit Accent, Note Off / Release / `done()`, MONO retrigger from Release, shared eight-voice budget, cross-part steal, stage-4 fade, fresh-slot VCA reset, engine-switch fade and queued-note replay with source/Accent preserved.
- New VCA dispatch checks: 19 PASS. Earlier real dispatcher/sequencer checks: 19 PASS. Total: 38 integration assertions.
- Exact HEAD: `80a37fa7b06ac24d5664a2681c9fb4df9dc34ce1`.
- Fast regression run `41340134547`: SUCCESS; all 13 tests passed.
- Full baseline run `41340134381` on this exact HEAD was pending at checkpoint time.
- Remaining: production callback implementation, real sequencer ratchet/rest/slide/tie coverage, full DSP graph, production RAM/CPU accounting, and exact-HEAD full baseline CI. Production engine registration remains gated.


## Final CI result — source HEAD 42e79344b561df8b106d4183c593b010ae8b88c9

- Full baseline run 41356473081: SUCCESS. Pinned submodules and AC79 SDK verified; unmodified Felucca firmware built; AC79 VCA compile-only probe passed; host and web regression passed; build artifact uploaded.
- Focused Metal Core run 41356472886: SUCCESS. The log reports 15 focused tests passed; artifact uploaded.
- Scope: these results do not register or build METAL CORE as a production engine, do not qualify the complete DSP graph on AC79 and do not establish an installable package or hardware validation.
- The current branch advanced after this source HEAD through documentation-only commits. Comparison confirmed no firmware, test, workflow, submodule or build-script changes; the successful CI still covers the current code tree, but the current documentation HEAD has not been rerun.


## CP1.4.1 inventory contract and latest exact-HEAD CI — 2026-11-02 09:00 UTC

- The logical V1 parameter map is frozen in `ENERGUN_METAL_CORE_CP1_4_1_FROZEN_PARAMETER_MAP.md`: 8 Engine EDIT + 16 common P_MC + 13 Private controls per part. Four independent Private banks contain 52 values. The 37 logical controls are unique; no numeric IDs, page IDs, firmware descriptors or storage ABI are assigned by this freeze.
- Added `tests/metal_core_parameter_inventory_test.c` and registered it in `tests/run_metal_core_fast.sh`. The host-only contract guards the accepted E0–E7 order, 8+16+13 count, four Private banks, uniqueness, the E1/E2 versus P_MC non-duplication rule, FM INDEX/LPG DECAY membership, and independent FOLLOW/METAL booleans. It does not implement parameter storage or firmware behavior.
- Fast regression run [41417141686](https://github.com/energundenis/Felucca/actions/runs/41417141686) completed SUCCESS on exact branch HEAD `7ec54a19bb1e18992c1dbc3603deba60015d411f`; all 18 focused host test programs passed.
- Exact-HEAD full baseline run [41417141732](https://github.com/energundenis/Felucca/actions/runs/41417141732) was IN PROGRESS at the time of this update. Do not claim full baseline success until GitHub reports a completed SUCCESS conclusion.
- This is a documentation/test-contract milestone. No production parameter IDs, descriptors, private API, Motion Lock implementation, Sidecar ABI, Flash allocation, production engine registration, installable METAL CORE package, factory image, or physical FM-1 state changed.
- CP1.4.1 logical inventory contract is now covered by fast regression; this does not close CP1.4 as a whole. CP1.4.2 conversion acceptance, CP1.4.3/4 Private/Motion implementation, CP1.4.5 Sidecar transaction ABI, CP1.4.7 CRC, CP1.4.8 fault/regression completion, CP1.4.9 archive V2, CP1.4.10 integration and CP1.4.11 Flash/recovery audit remain separate gates.

### Supersession note

This update supersedes the earlier CP1.20 priority wording that described the 8/16/13 logical allocation itself as unresolved. The logical map is now frozen and guarded by a host test. Stable IDs, runtime APIs, serialization, Sidecar and physical Flash/recovery remain unresolved and must not be inferred from that freeze.


## Part 7 documentation synchronization — 2026-11-02

- Added authoritative project status: [ENERGUN FM-1 CORE Master Status](ENERGUN_FM1_CORE_MASTER_STATUS_2026-11-02.md). It now owns the current CP0–CP7 and CP1.4.1–CP1.4.11 percentage matrix and execution order.
- CP1.4.1 logical membership remains frozen at 8 Engine EDIT + 16 P_MC + 13 Private per part; the 18-program fast lane passed on exact code HEAD `7ec54a19bb1e18992c1dbc3603deba60015d411f` in run [41417141686](https://github.com/energundenis/Felucca/actions/runs/41417141686).
- Full baseline run [41417141732](https://github.com/energundenis/Felucca/actions/runs/41417141732) was re-inspected: build and AC79 VCA compile-only probe completed successfully, but Felucca host/web regression remained in progress. It is not a completed PASS.
- CP1.4.2 remains open. The standard 7-bit V1 encoding decision is accepted; the 30-control conversion reference and mathematical quantization bounds do not by themselves approve musical resolution or close legacy project/preset migration and Sidecar compatibility gates.
- CP1.4.3/4 are design drafts, CP1.4.5 is a logical Sidecar schema/transaction draft, CP1.4.6 logical capacity arithmetic is verified only, CP1.4.9 source audit is complete only, and CP1.4.7/.8/.10/.11 remain implementation/qualification gates as detailed in the Master Status.
- DSP Stage 1–7, VCA and Sequencer V0.3 remain branch-local prototypes/work products; none is promoted to a complete registered production engine by this documentation update.
- The Master Status supersedes older task wording or percentages where those descriptions conflict. Historical CI results remain tied to their exact source commits.
- Documentation-only change. No firmware code, `main`, factory image, physical device, Flash allocation or engine registration changed.


## CP1.4.7 integrity audit and branch synchronization — 2026-11-02

- Added `docs/ENERGUN_FM1_CORE_CP1_4_7_SIDECAR_INTEGRITY_AUDIT.md`.
- Audited `storage.c`: existing zlib-compatible CRC-32 protects the complete storage payload; a separate CRC-32 protects the 32-byte A/B header; writes commit the header last and read back the committed object. Existing `storage_test.c` covers the standard check vector, torn writes, corrupt-copy fallback, sequence wrap and failure cases.
- Drafted the Sidecar policy: reuse the outer CRC-32 rather than adding a duplicate inner CRC by default; validate envelope/schema/identity/base-sequence/values separately; inspect both A/B copies and select only a CRC-valid candidate bound to the exact current base sequence. The existing `st_current()` helper alone is insufficient for that predicate.
- CP1.4.7 is recorded at **30%** for source audit and policy draft only. No Sidecar reader, transaction code, binary ABI or Sidecar-specific acceptance suite is implemented.
- Manually synchronized `metal-core-dsp-stage1-osc` with `energun-metal-core` in two merge commits. Latest documentation-resync commit: `7b47c18556e94b60d66c40f2676fac4d91c4d2af`; branch was ahead 89 / behind 0 at verification. Resolved the source overlaps by retaining current base CP3 registry safeguards and FOLLOW host include, carrying forward the isolated DSP implementation/adapter and adding its four tests to the current host suite. `ENGINES[]` remains unchanged; the adapter is still unregistered.
- Latest DSP stage workflow [41430073973](https://github.com/energundenis/Felucca/actions/runs/41430073973) completed SUCCESS. Its syntax-only `engines.c` check and OSC/FM/feedback tests passed at 44.1 and 96 kHz. This is host/compile evidence, not physical FM-1 validation or complete target-engine qualification.
- PR #1 baseline [41426208286](https://github.com/energundenis/Felucca/actions/runs/41426208286), fast regression [41426208152](https://github.com/energundenis/Felucca/actions/runs/41426208152) and biquad probe [41426208192](https://github.com/energundenis/Felucca/actions/runs/41426208192) completed SUCCESS.
- PR #2 fast regression [41426956119](https://github.com/energundenis/Felucca/actions/runs/41426956119) and baseline [41426956021](https://github.com/energundenis/Felucca/actions/runs/41426956021) both completed SUCCESS, including unmodified firmware build, AC79 VCA compile-only probe and host/web regression.
- No PR was merged into the canonical branch, `main` remains untouched, no engine was registered, no factory firmware or physical FM-1 state was changed, and no Flash sectors were allocated.
- The [Master Status](ENERGUN_FM1_CORE_MASTER_STATUS_2026-11-02.md) remains authoritative for CP0–CP7 and CP1.4 percentages.
