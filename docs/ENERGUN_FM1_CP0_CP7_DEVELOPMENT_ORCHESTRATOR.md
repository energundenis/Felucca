# ENERGUN FM-1 CORE — CP0–CP7 Development Orchestrator

Date: 2026-11-03  
Status: **Phase 1 complete; Phase 2 plan-only DAG planner added**  
Coordination repository: `energundenis/Felucca`  
Coordination branch: `energun-metal-core`  
Implementation branch: `energun-development-orchestrator`

## 1. Objective

Create one dependency-aware control plane for the complete CP0–CP7 development plan. The orchestrator must schedule independent work, preserve prerequisite gates, collect evidence, expose blockers and prevent unqualified work from being promoted into the integrated firmware.

This is not a claim that an autonomous coding agent is already running. The registry and execution contract are in place, and a plan-only DAG planner now validates dependencies, identifies ready workstreams and emits CI reports. It does not launch task commands or coding agents.

## 2. Existing capability reused

The current CP1 queue remains the proven implementation for its own persistence test DAG:

- Manifest: `tests/cp1_test_queue.json`
- Runner: `tests/run_cp1_test_queue.py`
- Controller tests: `tests/test_cp1_test_queue.py`
- Maximum CP1 worker count: 3
- It supports dependency validation, concurrent independent stages, dependency joins, BLOCKED/FAIL propagation, JSON/Markdown reports and GitHub Step Summary.

Do not replace or weaken this runner as part of the first orchestrator increment.

The CP1 runner's exit code alone is not an acceptance result: a queue may correctly finish with status BLOCKED while the CI step succeeds so its report can be uploaded. The global orchestrator must parse the explicit queue report status and preserve PASS, BLOCKED and FAIL as distinct states.

## 3. Global workstream registry

The machine-readable source of truth is `tests/cp0_cp7_development_orchestrator.json`.

It records:

- CP0 exact-source baseline and evidence refresh;
- CP1 persistence and the existing CP1 DAG;
- CP2 DSP qualification;
- CP3 host lifecycle work and production integration as separate gates;
- CP4 UI/control;
- CP5 sequencer;
- CP6 integrated qualification;
- CP7 package/recovery and separately authorized device validation.

Each workstream has a stable ID, checkpoint, state, dependencies, parallel group, executor type, acceptance criteria and evidence/blocker notes.

## 4. Scheduling model

The eventual scheduler will use a dependency DAG, not a fixed CP0, then CP1, then CP2 serial chain.

Initial intended concurrency lanes:

1. **Foundation:** CP0 baseline evidence refresh.
2. **Core foundations:** CP1 persistence work.
3. **DSP and lifecycle:** CP2 DSP qualification and CP3 host lifecycle work can proceed independently after CP0 baseline is accepted.
4. **Integration:** CP3 production integration waits for both CP2 qualification and CP3 lifecycle acceptance.
5. **Post-engine:** CP4 UI/control and CP5 sequencer may proceed independently after the production engine interface is accepted.
6. **System qualification:** CP6 joins CP1, CP2, CP3, CP4 and CP5.
7. **Release:** CP7 waits for CP6 and a separate explicit physical-device authorization.

The configured future scheduler ceiling is three concurrent tasks initially. It must be configurable, measured, and not confused with the number of GitHub Actions workflows. Until all work is routed through one dispatcher or a shared concurrency controller, independent existing workflows can exceed that global ceiling.

## 5. Required scheduler behavior

The implementation must:

- validate unique task IDs, known dependencies and acyclic graph structure;
- support forward dependency references;
- schedule only tasks whose prerequisites have passed;
- run independent ready tasks concurrently within the configured limit;
- collect already-running peer results after a failure;
- block descendants of failed or blocked tasks without falsely passing them;
- distinguish `planned`, `ready`, `running`, `checking`, `passed`, `blocked` and `failed`;
- record task start/end timestamps, duration, runner identity, commit SHA and evidence references;
- calculate queue wall time, summed task time, critical path, dependency wait and observed concurrency;
- produce JSON and Markdown reports plus GitHub Step Summary;
- make no automatic merge to the coordination branch or `main`;
- keep each implementation lane isolated in a feature branch and require review/evidence before integration.

A task without an audited executable command remains `planned` or `blocked`; the scheduler must never fabricate a successful test result.

## 6. Failure and promotion policy

- **PASS:** all declared acceptance checks for that task have actual evidence on the reported commit.
- **BLOCKED:** a prerequisite or required implementation is missing. This is not a CI infrastructure failure, but it prevents downstream promotion.
- **FAIL:** an executable check ran and failed, required evidence is missing/invalid, or the scheduler itself is invalid.
- **PLANNED:** scope is registered but no executable task is ready.

A CI workflow can be green while an engineering workstream remains BLOCKED. Reports must show both statuses separately.

No automatic merge is enabled. A passing task produces reviewable evidence; it does not authorize production registration or release by itself.

## 7. Safety boundaries

The orchestrator is host/CI-only by default.

It must not:

- write to a physical FM-1 or its Flash;
- modify or overwrite the factory firmware;
- change bootloader, OTA or recovery behavior without separately reviewed evidence;
- allocate physical Flash sectors or persistent object IDs outside the approved CP1 gates;
- merge unreviewed branches or push to `main`;
- promote unqualified DSP into the production render callback;
- treat a simulated-NOR test as proof of physical-device safety.

CP7 physical-device validation remains a manual approval gate requiring a separate, explicit user authorization.

## 8. Implementation phases

### Phase 1 — registry and contract (complete)

- Added the CP0–CP7 machine-readable workstream registry and dependency contract.
- Preserved the existing CP1 queue and its manifest.

### Phase 2 — global DAG planner and safe executor (initial slice implemented)

- Added `tests/run_development_orchestrator.py` to validate the registry and produce JSON/Markdown reports.
- Added `--execute` with a bounded worker pool. It runs only exact argv entries in `execution_policy.allowed_commands`, uses `shell=False`, applies per-task timeouts and records results.
- CP1 remains the specialized executor/queue owner, but its dedicated fast-workflow step runs it exactly once. The global orchestrator imports the existing JSON `PASS` / `BLOCKED` / `FAIL` report and does not invoke the CP1 runner a second time.
- Added unit coverage for dependency waits, blocked descendants, max parallel, allow-list enforcement, shell-free invocation, refusal to execute tasks whose prerequisites have not passed, and a barrier-based proof that independent executor workers overlap.
- The fast regression workflow invokes the executor and uploads its report.
- CP0 remains `checking` until baseline evidence is tied to the exact candidate commit. Therefore CP1 is correctly held at the dependency gate; no CP1 command is dispatched by the global executor yet.
- CP2's eight listed host tests and CP3's six lifecycle/dispatch host tests are already compiled and executed by `tests/run_metal_core_fast.sh`. Separate CP2/CP3 worker scripts were therefore duplicate execution paths and have been removed from the registry, allow-list and repository.
- CP2 and CP3 remain `checking` with no executor until their additional acceptance evidence is produced. Fast-regression host-test coverage is evidence for a subset only; it does not establish CP2 sample-rate/reference-vector, AC79 target/compiler and resource-budget qualification, nor CP3 production engine integration.
- Unit tests now assert that CP2/CP3 are not dispatched as duplicate workers even after CP0 passes. CP1 remains the only registered child executor in this iteration.

### Phase 3 — unified CI dispatch (not complete)

- Route already-existing CP2/CP3 host-test coverage through the canonical fast regression only; add new executors only for genuinely new acceptance checks, not for tests already run in the fast lane.
- Add dedicated CP2 reference-vector, sample-rate, target-compiler and resource-budget checks, and CP3 production-integration checks when their implementations and acceptance criteria are ready.
- A coarse repository-wide GitHub Actions concurrency group (`energun-fm1-ci-global`) is now shared by the fast-regression and baseline workflows. This serializes those two workflow lanes but is not a durable FIFO lock: GitHub allows one running and one pending run, and newer pending runs may replace older pending runs. CP8 tracks the durable lease/lock implementation as unfinished work.
- Add task timing, critical-path and dependency-wait metrics.
- Benchmark equivalent sequential and parallel task sets on the same commit.

### Phase 4 — controlled implementation execution (not complete)

- Add isolated branch/worktree execution only when a real coding executor is available.
- Require task scope, acceptance criteria, timeout, artifacts and rollback boundary.
- Open reviewable PRs; never auto-merge.
- Keep physical-device actions disabled.

## 9. Current implementation status

Implemented: CP0–CP7 registry, DAG validation/planning, bounded shell-free allow-listed command execution, CP1 report handoff without a second queue execution, duplicate CP2/CP3 host-worker removal, unit tests and fast-workflow integration.

Not implemented: automatic CP0 evidence handoff from the separate baseline workflow, automatic promotion of implementation tasks, a durable FIFO/lease-based global lock across workflows, autonomous coding agents, persistent background workers, automatic branch/PR creation or automatic merges. A coarse shared GitHub Actions concurrency group is present on the two existing workflows; see the CP8 design document for its limitations.

A green workflow does not imply that every CP0–CP7 workstream is passed. Workstream status is determined by explicit evidence and dependency gates.

