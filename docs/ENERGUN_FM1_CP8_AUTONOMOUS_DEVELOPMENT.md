# ENERGUN FM-1 CORE — CP8 Autonomous Development Infrastructure

Date: 2026-11-03  
Status: **Foundation in progress**  
Repository: `energundenis/Felucca`  
Implementation branch: `energun-development-orchestrator`

## Objective

Extend the CP0–CP7 test/development orchestrator into a controlled development system that can coordinate CI, assign bounded tasks to coding workers, validate their changes and open reviewable pull requests.

CP8 is infrastructure work. It does not mean autonomous coding agents are already connected or running.

## CP8 workstream registry

The machine-readable plan is `tests/cp8_autonomous_development_orchestrator.json`.

| ID | Workstream | Initial status |
|---|---|---:|
| CP8.1 | Durable queue and trusted coordinator | 75% |
| CP8.2 | Runtime Worker registry and capability model | 85% |
| CP8.3 | DAG-gated automatic task routing and assignment | 80% |
| CP8.4 | Isolated feature branches and workspaces | 0% |
| CP8.5 | Bounded autonomous source-code changes | 0% |
| CP8.6 | Automated validation and evidence collection | 0% |
| CP8.7 | Automatic reviewable PR creation | 0% |
| CP8.8 | Independent review and quality gate | 0% |
| CP8.9 | Conflict prevention, idempotency and recovery | 70% |

Percentages are engineering estimates, not CI results.

## CP8.1 — durable queue and trusted coordinator (75%)

A dedicated `energun-ci-queue` branch now stores the shared `queue/ledger.json` record set. Trusted GitHub Actions enqueue jobs write exact-SHA requests using non-forced Git-ref CAS. CP1, CP2 and CP3 lane workers claim requests from this shared ledger; GitHub concurrency is lane-scoped scheduling, not the queue itself.

The queue is live and has processed real tasks. Remaining work: stress concurrent enqueue/claim writers, prove no request loss across a large contention run, reconcile the backlog, and harden long-running heartbeat/retention behavior. There is no separate global mutex; atomic ledger CAS protects each state transition.

## CP8.2 — persistent runtime Worker registry (85%)

The runtime registry is stored in the queue ledger. Each ephemeral GitHub Actions worker registers a unique identity, run ID, lane, capabilities and per-worker capacity. The ledger tracks available/busy/offline state and the active request. Real CP1/CP2/CP3 worker registrations have been observed.

The connected workers execute trusted repository-owned test/build scripts. No external AI coding agent is connected, and no arbitrary source-code generation is claimed.

## CP8.3 — live DAG-gated task dispatch (80%)

The coordinator now:
- loads and validates the worker-lane DAG, rejecting unknown nodes and cycles;
- checks prerequisites before every claim;
- enforces per-worker capacity and the global three-worker limit;
- atomically claims an exact-SHA request through CAS;
- fences older attempts and records exact-SHA evidence.

The production CP1/CP2/CP3 lanes are independent. A parent/child dependency test proves that a dependent task waits until its prerequisite passes.

## CP8.9 — crash recovery and duplicate prevention (70%)

The live system recovered a CP1 request after its owning GitHub Actions job was cancelled. A later worker reclaimed the same request with attempt count 2, recorded the previous owner, and completed the exact-SHA test successfully. The old worker identity was marked offline.

Unit tests cover two distinct workers competing for one task, same-run cross-lane ownership, dependency blocking and stale-owner fencing. Remaining work includes repeated recovery stress, long-running heartbeat qualification and full queue replay/load testing.

## CP8.2–CP8.9 — planned architecture

1. Register coding workers with explicit capabilities, versions and least-privilege permissions.
2. Route tasks based on workstream type, dependencies, acceptance criteria and available concurrency.
3. Give each task an isolated feature branch/worktree and immutable base SHA.
4. Allow bounded code changes only inside the task's approved file scope.
5. Run tests and target builds on the exact resulting commit; retain logs and artifacts.
6. Create a reviewable PR with task scope, diff summary and evidence.
7. Require an independent review and all required checks before promotion.
8. Add idempotency, conflict detection, stale-work recovery and duplicate-task protection.

## Safety rules

- No automatic writes to `main`.
- No automatic PR merge.
- No physical FM-1 access or Flash writes.
- No factory firmware modifications.
- No coding worker may bypass task scope, tests or review.
- A CI-green result is not automatically a workstream PASS; acceptance evidence must match the exact commit.
- Secrets and credentials must not be exposed to untrusted task code or pull-request workflows.

## Implementation sequence

CP8.1 durable coordination → CP8.2 worker registry → CP8.3 dispatcher → CP8.4 isolated workspaces → CP8.5 bounded code changes → CP8.6 validation → CP8.7 PR creation → CP8.8 independent review → CP8.9 recovery/idempotency hardening.

CP8 may be developed alongside CP1–CP3, but must not weaken their acceptance gates or safety boundaries.
