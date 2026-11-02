# ENERGUN FM-1 CORE — CP1.4 storage host-test status

Date: 2026-11-02
Branch: `energun-metal-core`
Evidence commit: `a648633b6976b6c63a5df232599dfd44470962cd`

## Changes

- Extended `tests/storage_test.c` with four executable acceptance checks:
  - invalid newest header CRC falls back to the older valid copy;
  - a committed header with an oversized payload length is rejected, with fallback to the older copy;
  - equal valid A/B sequence numbers deterministically select copy A;
  - a zero-length object can be committed and loaded.
- Added `tests/storage_test.c` and `tests/fm6bank_test.c` to `tests/run_metal_core_fast.sh`.
- The fast lane now compiles and executes 20 host tests, including the real storage implementation and FM6 voice-bank staging/recovery tests.
- Existing GitHub Actions push and pull-request triggers automatically launch the fast lane for changes on `energun-metal-core`; the workflow uploads test evidence as an artifact.

## Verified CI

- Workflow: [ENERGUN METAL CORE fast regression](https://github.com/energundenis/Felucca/actions/runs/41452325949)
- Result: **success**
- Exact tested commit: `a648633b6976b6c63a5df232599dfd44470962cd`
- Summary: `all 20 tests passed`
- Storage test: passed, including all four new checks.
- FM6 bank test: passed; its existing host test exercised 7,296 programmed-byte power-cut points, staging in the autosave older copy, autosave interleaving behavior, and boot recovery.
- Artifact: [focused host-test evidence](https://github.com/energundenis/Felucca/actions/runs/41452325949/artifacts/13145042748)

## Scope and limitations

This is host-only evidence using simulated NOR Flash and the real Felucca storage/FΜ6-bank C paths. No physical FM-1 was connected or written. No factory firmware, bootloader, OTA or physical Flash allocation was changed.

The Sidecar pair coordinator is not implemented yet. Therefore the following remain OPEN and must receive their own executable tests as the coordinator and Sidecar format are implemented:

- exact base-generation binding and orphan/stale Sidecar rejection;
- matching-copy selection independent of Sidecar-local sequence;
- pair commit ordering and power-loss injection across Sidecar-before-base phases;
- Sidecar CRC/semantic validation and full roundtrip;
- archive V2 pair-aware restore and destination-sequence rebinding;
- explicit FM6 voice-bank staging reservation/ownership coordination with Sidecar autosave writes.

The FM6 bank test proves the existing FM6 staging behavior under its current host model; it does not prove the future Sidecar coordinator's arbitration contract.
