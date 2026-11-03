# ENERGUN FM-1 CORE — CP1 pair storage adapter and simulated NOR

Date: 2026-11-03

## Scope

This increment binds the backend-independent pair transaction coordinator to
Felucca's existing Base A/B object storage while keeping the Sidecar physical
backend injectable. It deliberately defines no Sidecar Flash address, XDATA
allocation, sector ownership, or hardware write path.

The adapter is included by `firmware/src/felucca.c` immediately after
`storage.c`, so it can use the existing private storage primitives without
exporting them:

- `st_read_copy_meta()`: reads one explicit Base A/B copy and validates the
  Felucca header and payload CRC.
- `st_current_meta()`: returns the current fully CRC-validated Base copy.
- `st_save_to()`: writes an explicitly selected Base copy, with the existing
  header-last commit and readback behavior.

## Pair-aware target selection

Before writing anything, the adapter scans both Base copies and both Sidecar
copies. It selects the newest Base generation that has a Sidecar record with
the exact Base sequence and payload CRC and passes Sidecar V1 semantic
validation.

If a complete pair exists, the next transaction targets the opposite Base
copy and the opposite Sidecar copy. This protects the last complete pair even
when the other Base copy is a newer orphan left by an interrupted earlier
transaction. The new Base sequence is predicted from Felucca's current valid
Base generation; after the Base write, the adapter checks the actual sequence,
length and CRC against that prediction.

If no complete pair exists, the adapter falls back to the ordinary Base A/B
selection rule and chooses the Sidecar copy opposite the newest valid outer
Sidecar record.

The Sidecar backend contract is explicit:
- `read_copy` must return only a fully committed outer A/B record after
  header and payload CRC validation.
- `write_copy` must erase/program only the selected copy, write its commit
  header last, read it back, and return failure unless readback validates.
- Neither callback receives nor derives a physical address from this adapter.

## Recovery

`energun_pair_storage_adapter_load()` scans both Base copies and both
Sidecar copies and selects the newest complete matching pair. It returns both
the Base payload and the Sidecar logical payload. A newer Base without a
matching Sidecar is ignored. If no complete pair exists, it returns failure;
the caller must apply deterministic defaults and must not rewrite storage
during load.

## Simulated NOR coverage

`tests/pair_storage_adapter_test.c` uses a test-only NOR image and test-only
Sidecar offsets. The model enforces:
- erase-to-`0xFF` sector semantics;
- program-only 1-to-0 bit transitions;
- 256-byte page-boundary restrictions;
- bounds checking;
- header-last Sidecar commit and outer payload CRC;
- injected Sidecar program cuts, Sidecar staging failures and Base erase
  failures;
- recovery from an orphan Base generation while preserving the last matching
  pair;
- Base + Sidecar payload roundtrip.

Latest focused CI result: **17 adapter/NOR checks, 0 failures** on the tested feature-branch revision. The independent pair transaction core suite reports 11 checks, 0 failures. These are host-model results only.

The test-only Sidecar offsets are not production Flash-map proposals.

## Explicitly not complete

- No physical Sidecar A/B backend is installed or enabled.
- No physical Flash sector is allocated or erased.
- No Base/Sidecar save or load callsite is switched to the adapter.
- No project/private parameter or Motion serialization is wired into runtime.
- No P_MC numeric Motion-ID whitelist has been frozen.
- No FM6 voice-bank staging reservation or autosave deferral is implemented.
- Archive V2 and pair-aware backup/restore remain separate work.
- Host simulated NOR is not a substitute for a physical power-loss test or a
  verified full 1 MiB device dump.

The adapter is therefore a production-facing software binding and host-tested
integration seam, not yet an enabled production persistence feature.
