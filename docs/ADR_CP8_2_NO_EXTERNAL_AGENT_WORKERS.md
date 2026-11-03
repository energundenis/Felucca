# ADR — CP8 worker architecture without external AI agents

Status: **Accepted; first GitHub Actions worker runtime implemented**  
Date: 2026-11-03  
Scope: ENERGUN FM-1 CORE development automation

## Decision

Build the first operational CP8 worker system using GitHub Actions, the repository's Python orchestration code, and GitHub-native queue/branch/PR primitives. Do not make external coding agents, third-party model APIs, paid inference services, or API keys prerequisites.

The system is an automation and CI execution fabric. It is not represented as an autonomous AI programmer.

## Initial worker lanes

- **Worker CP1 — Persistence:** Sidecar, Base/Sidecar transaction logic, storage host tests and compile checks.
- **Worker CP2 — DSP/VCA:** DSP and voice-engine host qualification, target compile checks and bounded integration tests.
- **Worker CP3 — Sequencer:** sequencer/control logic, host tests and integration checks.
- **Worker QA — Validation:** focused regression, baseline build, evidence collation and result verification.

CP4 integration work and additional lanes may be enabled only after their dependencies and safe task definitions are explicit.

## Required coordinator behavior

1. Maintain one durable global queue with idempotent task identities and exact commit SHA binding.
2. Claim tasks with exclusive leases and stale-owner fencing.
3. Dispatch only allow-listed workflows and task definitions.
4. Enforce declared dependencies, per-worker capacity and a configurable global parallelism limit.
5. Run independent lanes concurrently; serialize conflicting tasks.
6. Keep each code-changing task isolated to its own branch/workspace.
7. Collect logs, artifacts, test results and exact-SHA evidence.
8. Retry only classified transient failures; do not blindly retry deterministic test or compile failures.
9. Recover expired leases without duplicating active work.
10. Report queue state, worker state, failures and evidence.

## Capability boundary

GitHub Actions workers can run predefined scripts, tests, builds, static checks, packaging and deterministic repository maintenance. They can distribute and coordinate bounded tasks.

Without a coding agent or a human authoring the change, these workers do not independently understand free-form engineering requests or invent and implement arbitrary new firmware features. Code-generation tasks must therefore be explicitly scripted, template-driven, or prepared by a human. This limitation must be visible in status reports.

## Implementation phases

- **CP8.1:** finish live durable queue integration and trusted single coordinator.
- **CP8.2:** register and start real GitHub Actions worker lanes; prove worker identity and capacity accounting.
- **CP8.3:** connect queue claims to allow-listed worker workflows and enforce dependencies/parallelism.
- **CP8.4:** provide isolated branch/workspace handling for code-changing tasks.
- **CP8.5:** execute bounded, pre-authored automation tasks.
- **CP8.6:** collect and validate exact-SHA evidence.
- **CP8.7:** create Draft PRs for permitted changes; never auto-merge.
- **CP8.8:** independent automated checks and review gates.
- **CP8.9:** timeout, cancellation, lease recovery, idempotency and operational hardening.

## Security and hardware constraints

- No secrets or queue-write credentials are exposed to untrusted pull-request code.
- Untrusted PR code is never executed in a privileged pull_request_target context.
- No automatic merge and no writes to main.
- Factory V15 remains immutable.
- No physical FM-1 access, Flash map allocation, Flash erase/program, firmware installation or device flashing.
- All development changes remain on the existing feature branch and Draft PR until separately approved.

## Acceptance criteria for the first real parallel workers

The first release is accepted only after:
- at least three independently dispatched worker lanes run concurrently on independent tasks;
- the configured global and per-worker concurrency limits are enforced;
- dependent tasks wait until prerequisites pass;
- two queue requests are not lost or duplicated under contention;
- failed and expired tasks recover according to policy;
- each result is tied to the exact requested commit SHA and includes inspectable evidence;
- the coordinator produces a truthful summary of passed, failed, blocked and pending work.

## Current implementation evidence — 2026-11-03

The first trusted GitHub Actions workers are now registered and executing:
- CP1 Persistence
- CP2 DSP/VCA
- CP3 Sequencer

The shared queue ledger stores each ephemeral worker's identity, run ID, lane, capabilities, availability/busy/offline state and active request. Workers claim through CAS, check the lane DAG and global concurrency limit, and record exact-SHA outcomes. The live queue has demonstrated three concurrent lanes and recovery of a CP1 task after its owning job was cancelled.

The automated test suite covers dependency blocking, distinct-worker claim contention, stale-owner fencing and recovery. Full stress qualification and registry retention/heartbeat hardening remain open.

## Explicit non-goal

These GitHub Actions workers are real automation executors, not autonomous AI programmers. They run repository-owned build and test scripts; they do not independently interpret open-ended engineering requirements or author arbitrary new firmware features. External AI coding agents remain intentionally unconnected.
