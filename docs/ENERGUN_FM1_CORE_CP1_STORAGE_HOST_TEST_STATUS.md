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

## Sidecar V1 format acceptance — 2026-11-02

- Added standalone firmware-side energun_sidecar_v1.c/.h encoder and validator. The implementation is compiled directly by the host test; it is not connected to the storage driver or production save/load paths.
- Frozen the 20-byte little-endian V1 envelope with identity, exact Base sequence and Base payload CRC-32 fingerprint, payload length and reserved-field validation.
- Added project/live payload validation (P_MC + four Private banks + Motion extension) and preset-bank Private payload validation.
- Added tests/energun_sidecar_v1_test.c and tests/run_sidecar_v1_test.sh.
- CI: [fast regression and CP1 queue 41459497619](https://github.com/energundenis/Felucca/actions/runs/41458503150), exact code HEAD eecb9c37f2022640c9568985339d580ca21317a5, SUCCESS.
- Strict C11 compile with -Wall -Wextra -Werror: PASS. Sidecar V1 acceptance: **38 checks, 0 failures**.
- Sequential queue: Storage A/B PASS; FM6 staging/recovery PASS; Sidecar Format PASS; Base-generation binding BLOCKED pending the production storage metadata reader.
- This is format/validator host evidence only. No physical device, storage object ID, OBJ_COUNT, Flash sector, archive V1, bootloader, OTA or recovery path was changed.
