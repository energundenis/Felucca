# ENERGUN FM-1 CORE — CP1.6 / CP1.7 runtime integration progress

Date: 2026-11-03  
Branch: `energun-metal-core`  
Code/test HEAD verified by CI: `dbed170650bddab3b41d769a9531f3080b9b8c8b`

## 1. Work completed in this milestone

### CP1.4.11 — source Flash/recovery audit

Added `docs/ENERGUN_FM1_CORE_CP1_4_11_FLASH_AND_RECOVERY_GATE.md`.

The audit reconciles the Felucca storage object map, update-loader scan window, stock update-record sector, FM6/autosave staging ownership, current storage allowlist and Sidecar's full 14-sector logical A/B requirement (four projects + live autosave + two preset banks).

Result: source audit complete; physical sector allocation remains blocked until the user's own full 1 MiB V1.5 dump is obtained, hashed, preserved and compared with the loader/SPL map. No physical allocation or write is authorized.

### CP1.6 — host-only pair transaction acceptance harness

Added:
- `tests/sidecar_pair_transaction_test.c`
- `tests/run_sidecar_pair_transaction_test.sh`
- A separate `pair-transaction-harness` stage in `tests/cp1_test_queue.json`.

The test includes the real `firmware/src/storage.c` and `firmware/src/energun_sidecar_v1.c`. Base project A/B operations run against the same simulated NOR hooks used by the real storage tests. Sidecar copies are held in a separate test-only A/B backend; no production address, object ID or hardware backend is defined.

Covered cases:
- first Base + Sidecar commit and exact sequence/CRC binding;
- second commit and exact-pair load;
- torn Sidecar payload/header byte sweep;
- torn Base payload/header byte sweep after the future-bound Sidecar is committed;
- failed Base commit leaving the previous Base + matching Sidecar selected;
- retry protecting the old matching Sidecar even when a newer local-sequence orphan exists;
- corrupted newest Base falling back to the previous Base and its matching Sidecar.

## 2. CI evidence

Fast regression:
- Run: [41673679602](https://github.com/energundenis/Felucca/actions/runs/41673679602)
- Exact code/test HEAD: `dbed170650bddab3b41d769a9531f3080b9b8c8b`
- Conclusion: **SUCCESS**
- Focused regression: **22 tests passed**.
- CP1 queue: Storage foundation PASS; FM6 staging baseline PASS; Sidecar format PASS; Base-generation binding PASS; A/B selection PASS; pair-transaction harness PASS.

The queue then correctly reports **BLOCKED** at production Pair Commit Ordering. The workflow succeeds because it successfully runs the executable tests and reports the intentionally blocked production stage; it does not mean the production coordinator exists.

## 3. Current closure status

| Work item | Status | Remaining gate |
|---|---|---|
| CP1.17 logical Sidecar schema | Closed | Runtime integration is separate |
| CP2/CP3 inventory, logical IDs, ownership, persistence boundary | Closed | Production descriptor/engine/Sequencer integration remains separate |
| CP1.4.11 source map and loader/recovery audit | Source audit complete | User's own full-device dump and recovery verification |
| CP1.6 project runtime | Not integrated | Production pair coordinator, approved Sidecar backend, save/load/rename/erase/autosave call sites, archive compatibility |
| CP1.7 preset runtime | Not integrated | Bank Sidecar transaction, 32-slot preservation/erase/reuse, load/save/rename/delete call sites |
| Archive V2 | Not implemented | Pair-aware validation and restore |

## 4. Next engineering order

1. Extend the host pair harness to all five project identities and both preset-bank identities, including independent A/B histories and bank slot-preservation semantics.
2. Add explicit FM6 staging reservation tests and define a shared main-context storage ownership gate. Do not treat `autosave_hold()` as a lock.
3. Complete project lifecycle tests for Save As, Rename, Erase, Load, live Autosave boot and autosave writes. Preserve the legacy FUN10 base serializer and default extension state when Sidecar is absent.
4. Add preset bank tests for all 32 slots, bank mutation, Private Sidecar defaults, erase/reuse and the independent FM6 patch object.
5. Keep archive V1 byte-for-byte compatible; design archive V2 only after pair commit/recovery semantics are stable.
6. After the user's own dump and recovery gate are closed and the exact sector map is approved, implement the target Sidecar backend and production coordinator; only then wire CP1.6 and CP1.7 runtime call sites.

## 5. Safety

No production firmware source was changed by this milestone. No Sidecar object IDs, `OBJ_COUNT`, storage whitelist, Flash address, sector, archive V1, bootloader, OTA, factory image or physical device state was changed. No PR was merged. `main` remains untouched.

## 6. Follow-up host coverage — seven logical identities

Added:
- `tests/sidecar_identity_matrix_test.c`
- `tests/run_sidecar_identity_matrix_test.sh`
- A strict C11/Werror invocation in `tests/run_metal_core_fast.sh`.
- A `sidecar-identity-matrix` stage in `tests/cp1_test_queue.json`, which must pass before the production pair-order stage can be considered.

The matrix exercises all seven logical identities independently: Project 0–3, Live Autosave, Preset Bank 0–1. It checks each identity's own encoded record, rejects all cross-identity pairings, and checks that preset slot writes/erases preserve unrelated slots and clear the erased slot's values.

Scope limit: this is a serializer/identity/slot-semantics host test. It does not yet exercise seven independent Base object histories through the pair coordinator, does not implement FM6 staging ownership, and is not production runtime integration.

Verification status at this commit: **CI pending**. Do not report the new matrix or the updated 23-test fast regression as passed until GitHub Actions logs show the exact updated branch head executing them.
