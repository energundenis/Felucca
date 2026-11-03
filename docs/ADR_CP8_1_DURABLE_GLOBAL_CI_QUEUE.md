# ADR — CP8.1 durable global CI coordination

Status: **Trusted Git-ref queue and three-lane worker integration implemented; stress qualification remains open**  
Date: 2026-11-03  
Scope: ENERGUN FM-1 CORE repository CI only

## Decision

Use a durable request ledger on the dedicated `energun-ci-queue` Git branch as the source of truth. A trusted enqueue job records exact-SHA lane requests. Three independent lane workers (CP1 Persistence, CP2 DSP/VCA, CP3 Sequencer) claim work from the shared ledger. The system is not a single sequential consumer and does not rely on GitHub Actions concurrency as its queue.

The Git-ref update uses non-forced fast-forward compare-and-swap (CAS). Competing writers retry from the latest branch head. Queue content is data only; execution remains restricted to repository-owned lane commands.

## Queue record

Each request records:
- stable idempotency key: repository + sorted lane set + ref + exact commit SHA;
- source event and originating workflow/run;
- requested lane, state and attempt count;
- claim owner (worker identity and GitHub run ID), claim timestamp and lease expiry;
- exact-SHA terminal outcome and evidence URL.

States are `queued`, `claimed`, `running`, `passed`, `blocked`, `failed`, and `superseded`.

## Runtime behavior

1. Trusted push, schedule or manual events enqueue requests on the durable ledger.
2. CP1, CP2 and CP3 workers register in the same ledger at job start.
3. Before every claim, the worker checks its lane, per-worker capacity, the global three-worker limit and the declared DAG prerequisites.
4. A claim is committed atomically through CAS and bound to the worker identity, run ID and attempt number.
5. The worker checks out and tests the exact requested SHA, then records evidence before taking another task.
6. Independent lanes can run concurrently; each lane has its own Actions concurrency group.
7. The periodic workflow reconciliation wakes workers to drain durable requests left by replaced or cancelled workflow runs.

## Recovery and duplicate-execution rules

- A claim is not a PASS; only exact-SHA terminal evidence is a PASS.
- A worker claim is recovered only after GitHub confirms that the owning *job* has completed. A sibling job in the same workflow run may still be active.
- Recovery prioritizes abandoned claims ahead of new queued work.
- The new attempt increments the fencing counter; the old worker cannot mark the task running or finish it.
- A live owner is never displaced merely because a lease timestamp elapsed.
- Idempotency keys prevent duplicate enqueue for the same repository/lane/ref/SHA.
- CAS prevents two competing workers from acquiring the same queued request.
- Commands must be idempotent because process death can occur after a side effect but before its terminal result is committed.

## Implementation evidence (2026-11-03)

- `tests/cp8_ci_queue.py` and `tests/cp8_ci_queue_store.py` validate queue identity, state transitions, exact-SHA evidence, CAS contention, exclusive claims and stale-owner fencing.
- `tests/cp8_worker_runtime.py` persists worker enrollment, lane, capabilities, availability, active request and run identity in the ledger.
- `tests/cp8_worker_dag.json` declares the three production lanes; `load_worker_dag()` rejects unknown lanes, invalid dependencies and cycles.
- `tests/test_cp8_worker_runtime.py` covers dependency blocking, three independent worker claims, two distinct workers competing for one task, same-run cross-lane fencing and recovery fencing.
- The live `energun-ci-queue` ledger contains real worker registrations and queue outcomes.
- A live CP1 worker job was terminated while holding request `907803acfeb2b8d9508bec29`. A later worker reclaimed the same request with attempt count **2**, recorded the prior owner, and passed the exact requested SHA. The prior worker registry entry was marked offline.
- Three lane workers have run concurrently in GitHub Actions; the CP1, CP2 and CP3 pilot jobs passed on the same exact source SHA in run [41637129117](https://github.com/energundenis/Felucca/actions/runs/41637129117).

## Remaining qualification

- Stress-test many concurrent enqueue and claim writers against the live Git-ref CAS ledger and prove no lost requests or duplicate execution.
- Drain and reconcile the current queue backlog.
- Add periodic worker heartbeats for long-running tasks and bound registry retention.
- Keep untrusted pull-request code away from queue-write credentials.

## Security boundaries

- Only trusted branch workflows receive `contents:write`; pull-request code does not.
- Queue records cannot supply arbitrary shell commands; lane commands are allow-listed.
- No workflow writes to `main` or auto-merges a PR.
- Factory V15 remains immutable.
- No physical FM-1 access, Flash mapping, erase/program, installation or flashing is performed.
