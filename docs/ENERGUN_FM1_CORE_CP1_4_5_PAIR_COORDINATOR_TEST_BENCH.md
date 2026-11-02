# ENERGUN FM-1 CORE — CP1.4.5 pair coordinator and power-loss test bench

Date: 2026-11-02  
Branch: `energun/cp1-4-5-pair-coordinator`  
Status: implementation candidate; exact-head CI pending.

## Purpose

Provide an executable, backend-neutral Base + Sidecar pair coordinator and a deterministic host test bench that simulates power loss at each persistent write boundary. The coordinator is separate from the existing Felucca storage implementation: all physical reads/writes are supplied through callbacks. It assigns no Sidecar object ID, changes no `OBJ_COUNT`, and selects no Flash sector.

## Coordinator implementation

Files:

- `firmware/src/energun_sidecar_pair_coordinator.h`
- `firmware/src/energun_sidecar_pair_coordinator.c`

The coordinator:

1. Rejects invalid arguments and re-entrant calls.
2. Reads the exact selected Base generation from the backend.
3. Reads both Sidecar copies into caller-owned workspace and passes their storage-valid metadata to the existing orphan-aware Sidecar A/B selector.
4. Protects the newest Sidecar matching the active Base. A higher-sequence orphan cannot displace it.
5. Captures the next Base sequence/copy and calculates the Base payload CRC.
6. Encodes and semantically validates the new Sidecar against the future Base sequence + payload CRC before any write.
7. Writes the Sidecar to the safe opposite copy, with the next local Sidecar sequence, then reads it back and validates its exact future Base binding.
8. Only then writes the Base to its inactive copy.
9. Re-reads the selected Base and both Sidecar copies and requires an exact matching pair before returning success.
10. Calls the generation-aware dirty-clear callback only after full pair verification.

The backend contract distinguishes absent/invalid copies from I/O errors. All snapshots and work buffers are caller-owned; the coordinator uses no heap allocation or hidden static scratch. The workspace reserves three maximum Sidecar records (1,563 bytes at the current V1 maximum); target RAM/stack placement remains a separate audit gate.

## Executable test bench

Files:

- `tests/sidecar_pair_coordinator_test.c`
- `tests/run_sidecar_pair_coordinator_test.sh`

The test compiles the new coordinator with the real `storage.c` and Sidecar V1 module. The test backend maps the Base to an existing project A/B pair and temporarily uses `OBJ_SETTINGS` as a Sidecar A/B container **only inside simulated NOR tests**. This is not a production object-ID assignment or physical Flash mapping.

The fixture intentionally creates:
- Base A, sequence 1;
- Sidecar B, sequence 1, matching the active Base;
- Sidecar A, sequence 2, valid but bound to the future Base (orphan).

The coordinator must protect Sidecar B, overwrite Sidecar A with sequence 3, then commit Base B at sequence 2.

The byte-level NOR hooks inject two power-cut modes at each of the nine write operations:

| Operation | Write boundary |
|---:|---|
| 1 | Sidecar target erase |
| 2–4 | Sidecar payload pages |
| 5 | Sidecar commit header |
| 6 | Base target erase |
| 7–8 | Base payload pages |
| 9 | Base commit header |

For every operation the harness tests both a partial/torn operation and power loss immediately after the full operation: **18 injected cuts**. After each cut it discards coordinator RAM and re-reads persistent NOR. Recovery must produce either the complete old Base + old matching Sidecar pair, or the complete new Base + new matching Sidecar pair—never a mixed generation.

Additional checks cover:
- exact old-pair protection against a higher-sequence orphan;
- Sidecar-before-Base ordering;
- full post-commit pair verification;
- invalid Sidecar rejection before the first erase/program;
- dirty generation change during save;
- re-entrant transaction rejection.

## Automated queue

The CP1 queue now enables:

- `pair-commit-order`: `sh tests/run_sidecar_pair_coordinator_test.sh --ordering`
- `pair-power-loss`: `sh tests/run_sidecar_pair_coordinator_test.sh --power-loss`

The second stage depends on the first. The fast workflow is configured to run on this feature branch as well as the canonical coordination branch. Exact-head CI must pass before either queue stage is accepted.

## Evidence boundary and remaining gates

This work provides host C execution and byte-level simulated-NOR power-loss evidence only. It does not:
- connect Sidecar persistence to project, autosave, preset or archive call sites;
- freeze the final Sidecar ABI or assign object IDs;
- allocate Flash sectors or modify `OBJ_COUNT`;
- implement production Sidecar load/recovery or Archive V2;
- coordinate FM6 voice-bank staging in production;
- establish AC79 RAM/stack/cycle cost;
- access, erase, write or flash the physical FM-1.

The backend-neutral coordinator is an integration candidate. Its current test adapter proves transaction ordering against the existing Felucca A/B driver, not final hardware storage integration.
