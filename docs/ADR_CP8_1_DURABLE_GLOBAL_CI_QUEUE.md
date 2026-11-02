# ADR — CP8.1 durable global CI coordination

Status: **Proposed design; not implemented**  
Date: 2026-11-03  
Scope: ENERGUN FM-1 CORE repository CI only

## Context

A shared GitHub Actions `concurrency` group was experimentally applied to the existing fast-regression and baseline workflows. Both runs were cancelled while competing for the single pending slot. GitHub Actions concurrency is therefore not an adequate durable task queue by itself.

The repository needs to serialize expensive CI lanes without silently losing work requests, while preserving exact commit identity and recoverability.

## Decision proposal

Use a **durable request ledger plus one coordinator consumer**. GitHub Issues (or a dedicated repository data store) hold the durable queue records; a single coordinator workflow is the only consumer. GitHub Actions concurrency is used only to prevent two coordinator consumers from running simultaneously, not as the queue itself.

### Queue record

Every request must record:
- stable idempotency key: repository + workflow/lane + ref + exact commit SHA;
- source event and originating PR/push/run identifiers;
- requested validation lanes;
- state: `queued`, `claimed`, `running`, `passed`, `blocked`, `failed`, `superseded`;
- owner workflow/run ID and claim timestamp;
- lease expiry/heartbeat and attempt count;
- links to logs, artifacts and final evidence.

### Consumer behavior

1. Workflow events create or update durable queue records; they do not run expensive jobs directly.
2. A single coordinator workflow wakes on queue events and on a periodic reconciliation schedule.
3. The coordinator is serialized with one dedicated concurrency group and `cancel-in-progress: false`.
4. It scans durable records, deduplicates by idempotency key and claims one request.
5. It validates the exact SHA and runs the allow-listed CI lanes sequentially.
6. It records the terminal state and evidence before claiming another request.
7. The periodic sweep recovers queued records even if an event-triggered coordinator run was replaced in GitHub's one-pending-run mechanism.

### Recovery rules

- A claim is not a PASS. It must have an owner, exact SHA, start time and lease expiry.
- If a coordinator disappears, a later consumer may recover only an expired lease.
- A live lease must never be stolen.
- Every state transition is auditable.
- Repeated requests for the same idempotency key must not start duplicate builds.
- Newer commits may supersede older unstarted requests for the same ref/lane, but this policy must be explicit and recorded; different PRs must not silently supersede one another.

## Required implementation phases

1. Add a queue-record schema and validation tests.
2. Implement durable request creation and idempotency.
3. Implement the single coordinator and periodic reconciliation.
4. Route fast-regression and baseline lanes through the coordinator; prevent direct expensive execution outside approved manual recovery.
5. Implement lease expiry, recovery, timeout and cancellation handling.
6. Prove mutual exclusion, no lost requests, no duplicate execution and exact-SHA evidence using integration tests.

## Security boundaries

- Queue content is data, never executable shell input.
- Only fixed workflow/lane identifiers are accepted; arbitrary commands are forbidden.
- Pull-request code must not receive queue-write or privileged dispatch credentials.
- No secret is exposed to untrusted PR code.
- No workflow may write to `main`, merge a PR, modify factory firmware or access/flash physical FM-1.

## Exit criteria

CP8.1 remains incomplete until two independently triggered requests are queued, processed without overlapping expensive CI, neither request is lost, stale claims recover safely, duplicate requests are idempotent, and every result is tied to the exact tested SHA.
