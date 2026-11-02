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
- CP1 is the first registered executor. A zero process exit is not enough: the runner imports CP1's explicit JSON `PASS` / `BLOCKED` / `FAIL` result.
- Added unit coverage for dependency waits, blocked descendants, max parallel, allow-list enforcement, shell-free invocation and refusing to execute tasks whose prerequisites have not passed.
- The fast regression workflow invokes the executor and uploads its report.
- CP0 remains `checking` until baseline evidence is tied to the exact candidate commit. Therefore CP1 is correctly held at the dependency gate; no CP1 command is dispatched by the global executor yet.
- CP2 and CP3 are represented as independent workstreams, but have no separate registered executor commands yet. They are not falsely reported as running.

### Phase 3 — unified CI dispatch (not complete)

- Register separate CP2 DSP and CP3 lifecycle host-test commands.
- Route the relevant checks through the shared dispatcher without duplicating the existing fast regression.
- Add a global concurrency lock across workflows, task timing, critical-path and dependency-wait metrics.
- Benchmark equivalent sequential and parallel task sets on the same commit.

### Phase 4 — controlled implementation execution (not complete)

- Add isolated branch/worktree execution only when a real coding executor is available.
- Require task scope, acceptance criteria, timeout, artifacts and rollback boundary.
- Open reviewable PRs; never auto-merge.
- Keep physical-device actions disabled.

## 9. Current implementation status

Implemented: CP0–CP7 registry, DAG validation/planning, bounded shell-free allow-listed command execution, CP1 report handoff, unit tests and fast-workflow integration.

Not implemented: CP2/CP3 executor commands, automatic promotion of implementation tasks, a global concurrency lock across separate GitHub workflows, autonomous coding agents, persistent background workers, automatic branch/PR creation or automatic merges.

A green workflow does not imply that every CP0–CP7 workstream is passed. Workstream status is determined by explicit evidence and dependency gates.

## 9. First acceptance criteria

This architecture increment is accepted when:

- the CP0–CP7 registry parses as valid JSON;
- every dependency ID exists and the graph is acyclic;
- CP2 and CP3 host lifecycle are represented as parallel lanes after CP0;
- CP3 production integration waits for both;
- CP6 joins the required upstream gates;
- CP7 remains blocked behind CP6 and explicit device authorization;
- the existing CP1 runner and manifest are unchanged.

## 10. Current implementation status

This commit establishes the registry and contract only. It does not yet implement the global scheduler, a coding agent, a persistent background worker, a global concurrency lock or automatic branch/PR creation. Those are explicit Phase 2–4 tasks, not implied capabilities.
