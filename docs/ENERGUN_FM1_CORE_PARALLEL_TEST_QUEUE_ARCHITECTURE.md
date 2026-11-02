# ENERGUN FM-1 CORE — Parallel test queue architecture

Date: 2026-11-02  
Branch: `energun-metal-core`

## Decision

The CP1 persistence test queue is now a dependency-aware directed acyclic graph (DAG), not a strictly linear list. Independent ready stages can run concurrently; a stage starts only after all declared dependencies pass.

The configured worker limit is `max_parallel: 3`. This is deliberately conservative for GitHub-hosted runners and can be raised later after measuring resource use.

## Current CP1 graph

```text
Storage A/B evidence
        |
FM6 staging/recovery evidence
        |
        +------------------------------+
        |                              |
Sidecar format + semantics     Base-generation binding
        |                              |
        +---------------+--------------+
                        |
              Sidecar A/B selection
                        |
                Pair Commit Ordering
                        |
                Pair Power-loss
                        |
                Sidecar Integrity
                        |
              Pair Recovery/Roundtrip
                        |
                 Archive V2 Restore
                        |
              FM6 Staging Coordination
                        |
               Final CP1 regression
```

The Sidecar format suite and Base-generation binding suite are independent acceptance programs: both compile the actual production sources into separate test executables and use isolated simulated-NOR state. They now execute in parallel. Sidecar A/B selection joins both results before it is allowed to pass the next gate.

The broader fast host regression and AC79 baseline build remain separate GitHub Actions workflows and are also triggered independently on the same source push. GitHub schedules those workflows independently; this is CI parallelism, not a persistent background agent.

## Scheduler behavior

- Validate unique stage IDs, valid dependency references and acyclic graph before execution.
- Allow dependencies to appear later in the manifest.
- Launch all currently ready independent stages up to the configured worker limit.
- A stage becomes eligible only after every dependency is PASS.
- BLOCKED or FAIL propagates only to descendants; unrelated branches can continue.
- With `stop_on_first_failure: true`, no new work is scheduled after a failure; already-running peer jobs are allowed to finish and report.
- Every stage records PASS, FAIL or BLOCKED. A BLOCKED result is never represented as a passing test.
- Emit JSON and Markdown queue reports plus GitHub Step Summary.
- Queue is host-only and uses simulated NOR. No physical FM-1 access or Flash write is permitted.

## Automated scheduler tests

`tests/test_cp1_test_queue.py` verifies:
- independent stages really overlap (barrier-based test, not timing-only);
- a join stage waits for all parallel prerequisites;
- blocked stages block descendants but not independent branches;
- a failed stage blocks descendants while an already-running peer can finish;
- forward dependency declarations work;
- dependency cycles are rejected.

## Next expansion

As independent CP2 DSP, CP3 integration, CP4 UI, and CP5 sequencer acceptance suites gain explicit executable contracts, they can become additional DAG branches. They must not be placed in parallel merely because they have different names: shared generated files, global state, hardware resources, or prerequisite code must first be isolated and declared. Physical-device and Flash operations remain outside this host CI queue until separately audited and explicitly authorized.
