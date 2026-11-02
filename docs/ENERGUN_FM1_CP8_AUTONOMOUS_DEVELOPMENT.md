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
| CP8.1 | Repository-wide CI concurrency coordination | 50% |
| CP8.2 | Coding worker registry and capability model | 0% |
| CP8.3 | Automatic task routing and assignment | 0% |
| CP8.4 | Isolated feature branches and workspaces | 0% |
| CP8.5 | Bounded autonomous source-code changes | 0% |
| CP8.6 | Automated validation and evidence collection | 0% |
| CP8.7 | Automatic reviewable PR creation | 0% |
| CP8.8 | Independent review and quality gate | 0% |
| CP8.9 | Conflict prevention, idempotency and recovery | 0% |

Percentages are engineering estimates, not CI results.

## CP8.1 — initial CI coordination guard

The following existing workflows now share the exact GitHub Actions concurrency group `energun-fm1-ci-global`:

- `.github/workflows/energun-metal-core-fast.yml`
- `.github/workflows/energun-fm1-baseline.yml`

Both use `cancel-in-progress: false`. This prevents the baseline and fast workflows from intentionally running concurrently under the same repository-level group.

### Important GitHub Actions limitation

GitHub Actions concurrency is not a durable FIFO queue or a lease-based distributed lock. A group allows one running and one pending run; a newer pending run can replace an older pending run. Therefore CP8.1 is only partially complete.

Before claiming a robust global lock, implement and test a durable coordination mechanism with atomic acquisition, owner/run identity, expiry/heartbeat, stale-lock recovery and unconditional release. Every future ENERGUN workflow must participate in the same mechanism.

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
