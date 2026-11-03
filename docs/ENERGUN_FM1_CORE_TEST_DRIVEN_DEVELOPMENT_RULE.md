# ENERGUN FM-1 CORE — Mandatory Test-Driven Development Rule

Date: 2026-11-02
Branch: `energun-metal-core`
Status: Accepted project-wide development rule.

## Rule

Every completed technical development task must add or extend executable automated tests in the same work unit, unless the task is explicitly documentation-only, research-only, or an audit that cannot yet be expressed as an executable test. In those exceptions, the result must identify the concrete blocker and create or update a tracked test case for the first implementation stage where it becomes executable.

This rule applies across CP0–CP7, including parameter handling, DSP, engine lifecycle, editor/UI, sequencer, persistence, recovery, packaging and release qualification.

## Required workflow

1. **Define acceptance before implementation.** Write down observable expected behavior, valid/invalid inputs, boundary cases and failure behavior.
2. **Add tests with the implementation.** Prefer a failing test first when practical. Do not defer the entire test suite until an architectural phase is finished.
3. **Test both normal and adverse paths.** Include boundaries, invalid data, state transitions, resource limits, failure injection and regression cases where relevant.
4. **Run focused tests, then regression.** Run the smallest relevant test target first, followed by the applicable project regression/build workflow.
5. **Require CI evidence.** New or changed executable tests must be wired into CI. Record the exact workflow/run, commit SHA, result and test count. A local-only pass is not a CI pass.
6. **Preserve regression coverage.** Every discovered defect must receive a regression test that fails before the fix and passes after it, where reproducible.
7. **Report honestly.** Distinguish documentation/design coverage, host tests, target compile probes, target runtime tests and physical-device tests. Never present one category as proof of another.
8. **Track unfinished tests explicitly.** If a test cannot yet run because an interface, implementation, hardware capability or safe test fixture is missing, list it as OPEN with its blocker, owner/workstream and next enabling step. Do not count a written test plan as an executed test.
9. **Update status after each test-bearing change.** Keep the CP/task status and test matrix aligned with actual repository and CI evidence. Do not increase completion percentages solely because a test plan or document was written.

## Minimum acceptance for closing a technical subtask

A technical subtask is not complete until:
- its stated acceptance criteria are covered by executable tests, or any justified untestable criteria are explicitly separated and tracked;
- focused tests pass;
- the relevant regression/build CI passes;
- test evidence is linked to the exact commit;
- any remaining gaps and limitations are recorded.

If CI is unavailable or failing for an unrelated infrastructure reason, the subtask remains implementation-complete but **verification-pending**; it must not be reported as fully verified.

## Safety and hardware distinction

Host tests, simulations and compile-only target probes must be labelled as such. They do not authorize or substitute for firmware flashing, Flash-sector allocation, bootloader/OTA/recovery changes, or physical FM-1 validation. Existing project safety gates and explicit user authorization remain in force.

## Immediate application

Apply this rule to all work from 2026-11-02 onward. First priority is to turn the CP1.4.5/CP1.4.7 Sidecar transaction and integrity acceptance matrix into executable host tests around the real storage implementation, including A/B pair selection, power-loss injection, CRC/semantic validation, base-generation binding, stale/orphan rejection and FM6 Voice Bank staging coordination. Do not select physical Flash sectors or write to the device as part of this work.

## Reporting format

For each development update, report:
- tests added since the previous update;
- focused test result and count;
- CI workflow/run URL and exact commit;
- regressions or failures found and their status;
- tests still open, with the specific blocker;
- whether evidence is host-only, target compile, target runtime or physical-device level.

This rule changes the development process only. It does not change firmware behavior, parameter IDs, storage ABI, Flash layout, factory firmware or device state.

## CP2 / CP3 test-growth requirement — accepted 2026-11-03

The general rule above is mandatory for CP2 (DSP/VCA) and CP3 (sequencer/integration) in particular:

1. Every new feature, behavior change, bug fix, or completed implementation subtask in CP2 or CP3 must add a new executable test or extend an existing test with a distinct assertion that covers the changed behavior. A code change must not be considered test-complete merely because the old suite still passes.
2. Every defect found in CP2/CP3 must gain a regression assertion that reproduces the defect and verifies the fix.
3. The existing CP2 and CP3 suites remain mandatory regression gates. Re-running an unchanged suite is useful regression evidence, but it is not new test coverage and must not be reported as progress in test coverage.
4. Each CP2/CP3 development update and CI summary must identify: (a) tests added/extended since the previous update, (b) focused tests executed and pass/fail counts, (c) the unchanged regression suites executed, and (d) open behaviors without executable coverage and their blockers.
5. When a task is research/design/audit-only and cannot yet be expressed as an executable test, record the missing acceptance contract and the concrete implementation milestone at which its test will be added. Do not silently waive the requirement.
6. CP2 and CP3 may share a CI worker or run in parallel, but each lane must preserve clear ownership and traceability of its tests. Do not count the same test twice merely because it is invoked by more than one workflow.

### Current CP2 / CP3 baseline (2026-11-03)

The current CP8 worker invokes:
- CP2: `tests/run_metal_core_fast.sh` (currently 21 focused host regression executables/assertion groups as reported by that script).
- CP3: `chord`, `ratchet`, `step15`, `seq155`, and `swing` host test executables.

These are the existing baseline suites, not proof that new CP2/CP3 coverage is being added. Their test inventory and counts must be refreshed from actual CI output when tests change; do not assume the number of assertions equals the number of executables. A successful repeat run establishes regression status for that SHA only.

