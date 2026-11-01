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
