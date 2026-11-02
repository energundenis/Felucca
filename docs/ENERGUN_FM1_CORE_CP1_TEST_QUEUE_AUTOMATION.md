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

## Safety

This is host-only automation using simulated NOR Flash and existing host-test evidence. It does not connect to or write a physical FM-1. It does not select Flash sectors, assign Sidecar object IDs, change `OBJ_COUNT`, or alter bootloader/OTA/recovery behavior. CP1.4.11 and explicit physical-device authorization remain independent gates.
