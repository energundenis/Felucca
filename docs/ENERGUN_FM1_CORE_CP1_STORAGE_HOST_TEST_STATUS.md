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

The backend-independent pair transaction core and Felucca Base storage adapter now exist and pass simulated-NOR host tests. The production Sidecar Flash backend and runtime project/preset/autosave callsites are still not connected. The following remain OPEN:

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
- CI: [fast regression and CP1 queue 41460427671](https://github.com/energundenis/Felucca/actions/runs/41458503150), exact code HEAD 651da48f95a90743b77383e181907c9ac4fc9942, SUCCESS.
- Strict C11 compile with -Wall -Wextra -Werror: PASS. Sidecar V1 acceptance: **38 checks, 0 failures**.
- Sequential queue: Storage A/B PASS; FM6 staging/recovery PASS; Sidecar Format PASS; Base-generation binding BLOCKED pending the production storage metadata reader.
- This is format/validator host evidence only. No physical device, storage object ID, OBJ_COUNT, Flash sector, archive V1, bootloader, OTA or recovery path was changed.

## Base-generation binding acceptance — 2026-11-02

- Added internal st_meta_t / st_current_meta() in firmware/src/storage.c. It reports the exact copy, sequence, length and payload CRC selected by the real storage A/B reader after payload CRC validation. st_load() now consumes this helper.
- Added tests/sidecar_base_binding_test.c and tests/run_sidecar_base_binding_test.sh. This suite compiles the actual storage.c and Sidecar V1 C module together against simulated NOR.
- Verified CI: [41462258057](https://github.com/energundenis/Felucca/actions/runs/41462258057), code HEAD 95a95e383015d0fb584902770d3ea9125cd3074c, SUCCESS.
- Base-generation binding suite: **15 checks, 0 failures**. Sidecar V1 suite: **38 checks, 0 failures**. Existing focused regression: 21/21 passed.
- Verified stale Sidecar rejection after Base rewrite and acceptance against the old Base generation after a corrupted newer Base payload falls back to A.
- Next stage: Sidecar A/B matching-copy inspection and selection. It remains blocked until production dual-copy reader and tests are implemented.
- No physical Flash, object IDs, OBJ_COUNT, archive V1, bootloader, OTA or physical device changed.

## Sidecar A/B selection acceptance — 2026-11-02

- Added storage primitive `st_read_copy_meta()` for an explicit A/B copy read with header and outer payload CRC validation; it returns local storage sequence, length and CRC and copies bytes into caller-owned memory.
- Added Sidecar record inspection and A/B selector. Exact Base sequence + payload CRC determines pair membership; the newest fully valid local Sidecar sequence determines the next storage sequence even if it belongs to an orphan. The newest matching copy is protected from overwrite.
- Added `tests/sidecar_ab_selection_test.c` and strict runner `tests/run_sidecar_ab_selection_test.sh`.
- CI [41478848470](https://github.com/energundenis/Felucca/actions/runs/41478848470), exact code HEAD `277313eeda4bd3805ea0c910e3298633c041d03b`, **SUCCESS**.
- Results: Sidecar V1 **38 checks, 0 failures**; Base-generation binding **16 checks, 0 failures**; Sidecar A/B selection **17 checks, 0 failures**; existing focused regression **21/21**.
- Queue passes through Sidecar A/B selection and blocks at Pair Commit Ordering. Production pair transaction coordinator and fault injection remain unimplemented.
- Latest baseline workflow [41478848463](https://github.com/energundenis/Felucca/actions/runs/41478848463) was still running host/web regression at the time of this update; no final baseline result claimed.
- Simulated NOR only. No physical Flash, object IDs, `OBJ_COUNT`, archive V1, bootloader, OTA or device write.


## Pair transaction coordinator and exhaustive fault-injection update — 2026-11-03

- Added/verified `firmware/src/energun_pair_txn.c/.h`: backend-independent Sidecar-first transaction core. It plans the next Base generation, stages/validates Sidecar, commits Base only after Sidecar succeeds, then verifies the pair.
- Added/verified `firmware/src/energun_pair_storage_adapter.c/.h`: binds the coordinator to Felucca's real Base A/B storage primitives while injecting a separate Sidecar backend. No production Sidecar sector address is defined.
- Added/verified simulated-NOR adapter acceptance in `tests/pair_storage_adapter_test.c`; the suite uses test-only RAM offsets and the real included `storage.c` implementation.
- Expanded interruption coverage to **every programmed byte** in the current fixture: 169 Sidecar payload/header cut positions and 48 Base payload/header cut positions. All 217 interruption cases preserved the previous complete pair; 0 failures.
- Pair transaction core: **11 checks, 0 failures**. Adapter: **18 checks, 0 failures**.
- Exact tested HEAD: `97de9b7abf893c18064fcde942c46f637b2c3e9a`. CP1 worker run [41640259969](https://github.com/energundenis/Felucca/actions/runs/41640259969): SUCCESS. Focused fast regression [41640260005](https://github.com/energundenis/Felucca/actions/runs/41640260005): SUCCESS.
- The queue still blocks at pair-commit-order because the production Sidecar Flash backend, authoritative physical region exclusions and production callsite wiring are not qualified. No physical map, sector, bootloader, OTA, factory image or device state was changed.
