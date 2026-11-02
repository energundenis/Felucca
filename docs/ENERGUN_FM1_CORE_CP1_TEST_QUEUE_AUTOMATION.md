# ENERGUN FM-1 CORE — CP1 automated test queue

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **ORCHESTRATOR IMPLEMENTED — Sidecar stages remain gated until executable tests exist**

## What is automated

The `ENERGUN METAL CORE fast regression` GitHub Actions workflow now runs in this order:

1. Generate the normal checked-in-source headers.
2. Compile and execute the focused host regression suite.
3. Run `tests/test_cp1_test_queue.py` to test the queue controller itself.
4. Run `tests/run_cp1_test_queue.py` to evaluate the CP1 persistence queue in dependency order.
5. Upload both the host-test results and CP1 queue report as workflow artifacts.

The queue is triggered automatically by relevant source/test/workflow changes on `energun-metal-core`, and can also be started with the workflow's manual dispatch control.

## Queue stages and gates

The queue manifest is `tests/cp1_test_queue.json`. Stages are sequential and each stage depends on the previous stage:

1. Existing Felucca Storage A/B tests — verified from the same workflow's `PASS: storage` evidence.
2. Existing FM6 Voice Bank staging/recovery tests — verified from `PASS: fm6bank`.
3. Sidecar schema/envelope/semantic validation.
4. Exact Base-generation binding.
5. Sidecar A/B matching-copy selection, including stale/orphan rejection.
6. Pair transaction ordering.
7. Power-loss injection at every pair-commit boundary.
8. Sidecar integrity and semantic validation.
9. Pair recovery and full roundtrip.
10. Archive V2 pair-aware restore and destination rebinding.
11. FM6 staging ownership/reservation and Sidecar autosave coordination.
12. Final focused host regression.

Stages 3–11 are deliberately marked `blocked` because the corresponding production Sidecar implementation and executable acceptance tests do not yet exist. They are not represented as passed. The first blocked stage stops execution of dependent stages; the report still lists all later stages as BLOCKED.

When a stage is implemented, the same work unit must add its executable test and update that stage in `tests/cp1_test_queue.json` from `blocked` to `ready`, with a concrete host-test command. The queue runner executes ready commands in order and halts on the first failure. The final full regression is already configured as the last gated stage.

## Reports and failure behavior

- Runner: `tests/run_cp1_test_queue.py`
- Queue manifest: `tests/cp1_test_queue.json`
- Controller unit tests: `tests/test_cp1_test_queue.py`
- Generated report: `build/cp1-test-queue/report.md`
- Machine-readable report: `build/cp1-test-queue/report.json`
- Both report files are uploaded with the focused regression artifact.
- A missing required baseline evidence item is a CI failure.
- A ready-stage command failure is a CI failure and blocks all downstream stages.
- A not-yet-implemented stage is reported as BLOCKED, not PASS. The orchestration step completes so the report can be inspected; its explicit queue status is BLOCKED.

## First verified automation run (2026-11-02)

- Workflow: [ENERGUN METAL CORE fast regression](https://github.com/energundenis/Felucca/actions/runs/41455462517)
- Exact tested commit: `5fc1228d032f39aa84abd6bb9684d94f35dd4e95`
- GitHub Actions result: **success**.
- Focused regression: **21 test programs/suites passed**; the queue-controller Python suite contains **4 unit tests**, all passed.
- Queue prerequisite stages: Felucca Storage A/B **PASS**; existing FM6 staging/recovery **PASS**.
- Sequential CP1 queue result: **BLOCKED at `sidecar-format`**, as intended, because the production Sidecar ABI/parser and its executable acceptance tests do not yet exist. All dependent Sidecar stages remain blocked; none is reported as passed.
- Evidence artifact: [download from the workflow run](https://github.com/energundenis/Felucca/actions/runs/41455462517/artifacts/13146433895).

## Sidecar Format stage now enabled (2026-11-02)

- Stage `sidecar-format` is `ready` in `tests/cp1_test_queue.json` and runs `sh tests/run_sidecar_v1_test.sh` after the existing Storage A/B and FM6 staging prerequisites.
- Verified workflow: [41460427671](https://github.com/energundenis/Felucca/actions/runs/41459497619), exact tested code commit `651da48f95a90743b77383e181907c9ac4fc9942`, **SUCCESS**.
- Strict C11 compile (`-Wall -Wextra -Werror`) passed; Sidecar V1 acceptance suite: **38 checks, 0 failures**.
- Queue outcome: Storage A/B PASS → FM6 staging/recovery PASS → Sidecar Format PASS → BLOCKED at Base-generation binding. This is expected because the production sequence-aware Base metadata reader has not been implemented. All later dependent stages remain blocked.
- The Sidecar V1 module is host-tested and storage-independent. It is not yet connected to `storage.c`, project/preset/autosave call sites or physical Flash.

## Base-generation binding stage now enabled (2026-11-02)

- Added the internal `st_current_meta()` helper in the real `firmware/src/storage.c`; it returns the exact selected valid Base copy, sequence, payload length and payload CRC after normal A/B fallback validation. `st_load()` now uses this helper without changing its behavior.
- Added `tests/sidecar_base_binding_test.c` and `tests/run_sidecar_base_binding_test.sh`; the test compiles real storage and Sidecar V1 code against simulated NOR.
- Verified workflow: [41462258057](https://github.com/energundenis/Felucca/actions/runs/41462258057), exact tested code commit `95a95e383015d0fb584902770d3ea9125cd3074c`, **SUCCESS**.
- Sidecar V1 format: **38 checks, 0 failures**. Base-generation binding: **15 checks, 0 failures**.
- Queue now passes Storage A/B → FM6 staging/recovery → Sidecar Format → Base-generation binding, then blocks at Sidecar A/B selection because production dual-copy inspection is not implemented yet.
- Still host-only simulated NOR. No physical sectors, object IDs, `OBJ_COUNT`, archive integration or device writes.

## Safety

This is host-only automation using simulated NOR Flash and existing host-test evidence. It does not connect to or write a physical FM-1. It does not select Flash sectors, assign Sidecar object IDs, change `OBJ_COUNT`, or alter bootloader/OTA/recovery behavior. CP1.4.11 and explicit physical-device authorization remain independent gates.

## Sidecar A/B selection stage now enabled (2026-11-02)

- Added `st_read_copy_meta()` in the real `firmware/src/storage.c`: explicit A/B read, outer header/payload CRC validation, caller-owned payload buffer and local storage sequence metadata. No object ID or physical sector was allocated.
- Added `energun_sc_v1_inspect()` and `energun_sc_v1_select_ab()`: the newest Sidecar matching exact Base sequence + payload CRC is protected; newest valid local sequence is tracked separately, including orphan records; write target is the opposite copy. Sequence wrap and ties follow the defined half-range/A-wins policy.
- Added `tests/sidecar_ab_selection_test.c` and strict runner `tests/run_sidecar_ab_selection_test.sh` using the real storage and Sidecar code against simulated NOR.
- Verified fast workflow: [41478848470](https://github.com/energundenis/Felucca/actions/runs/41478848470), exact tested code commit `277313eeda4bd3805ea0c910e3298633c041d03b`, **SUCCESS**.
- Results: existing focused regression **21/21**; Sidecar V1 **38/38**; Base-generation binding **16/16**; Sidecar A/B selection **17/17**.
- Queue passes Storage A/B → FM6 staging/recovery → Sidecar Format → Base-generation binding → Sidecar A/B selection, then stops at **Pair Commit Ordering**. The production pair coordinator is the next unimplemented prerequisite.
- Latest baseline build [41478848463](https://github.com/energundenis/Felucca/actions/runs/41478848463) for the same code HEAD was still running the host/web regression at the time of this update; no final baseline PASS is claimed yet.
- No physical Flash map, object IDs, `OBJ_COUNT`, archive V1/V2, bootloader, OTA, recovery or physical FM-1 access changed.


## Parallel DAG queue automation — 2026-11-02

- Replaced the strictly sequential CP1 runner with a dependency-aware DAG scheduler in `tests/run_cp1_test_queue.py`.
- Manifest sets `max_parallel: 3`. Sidecar format/semantic validation and Base-generation binding now share the FM6 foundation prerequisite and run concurrently. Sidecar A/B selection waits for both suites to pass; pair transaction and later stages remain gated.
- BLOCKED/FAIL propagates to dependent stages only. Independent branches can finish; after a failure no new work is scheduled (`stop_on_first_failure`), while already-running peer jobs are collected.
- Scheduler validates duplicate IDs, unknown dependencies and cycles; forward references are supported. It writes JSON/Markdown reports and GitHub Step Summary.
- Added barrier-based scheduler tests for actual overlap, dependency join, independent branch continuation, failure handling and cycle rejection in `tests/test_cp1_test_queue.py`.
- Fast workflow step renamed to `Run dependency-aware parallel CP1 persistence test queue`.
- Architecture: [Parallel test queue](ENERGUN_FM1_CORE_PARALLEL_TEST_QUEUE_ARCHITECTURE.md).
- This is CI job parallelism, not an always-on background agent. Existing fast host and AC79 baseline workflows remain independently triggered. Host/simulated-NOR only; no physical FM-1 operations.


## Parallel queue CI acceptance — 2026-11-02

- Fast regression [41481399097](https://github.com/energundenis/Felucca/actions/runs/41481399097) **SUCCESS**, exact tested code HEAD `ae7d01399396c4e40b3eae24d2e0ebb9aebbcd9a`.
- Focused host regression: **21/21 PASS**. Parallel scheduler unit tests: **5/5 PASS**. The barrier-based test proves two independent stages overlap, then a join stage waits for both.
- In the real CP1 run, `sidecar-format` and `base-generation-binding` started together; both passed (Sidecar V1 **38/38**, Base binding **16/16**). `sidecar-ab-selection` started only after both passed and passed **17/17**.
- Queue ended **BLOCKED at Pair Commit Ordering**, as expected: the production Base+Sidecar transaction coordinator and fault-injection acceptance are the next implementation work. Queue BLOCKED is a correct gate, not a CI failure; the workflow itself succeeded.
- The first CI attempt found a shell-quoting defect in the concurrency barrier test (`NameError`); the test was corrected to use shell-safe quoting, and the subsequent full fast workflow passed. This regression is now covered.
- Baseline workflow [41481399187](https://github.com/energundenis/Felucca/actions/runs/41481399187) for the same code HEAD was still running host/web regression at this update.
