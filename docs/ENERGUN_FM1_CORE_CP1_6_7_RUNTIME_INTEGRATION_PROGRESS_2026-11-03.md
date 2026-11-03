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

## 7. FM6 staging ownership gate and seven-identity pair-history follow-up

Branch follow-up on 2026-11-03:

- Added a main-context storage reservation for the inactive `OBJ_AUTOSAVE` A/B copy. `st_save_to()` rejects writes targeting the reserved copy before erase; FM6 Voice Bank owns the reservation while staging.
- FM6 releases the reservation after the staged bank has been copied to `FVB_HOME` and verified. A committed staged copy remains reserved if home recovery fails, so an ordinary autosave cannot erase the recovery source. Volatile ownership is reset by reboot; `fvb_boot()` can then recover the committed stage.
- `autosave_poll()` explicitly defers while `OBJ_AUTOSAVE` is reserved. `autosave_hold()` remains only a delay policy, not a lock.
- Added a host acceptance assertion that an autosave write is rejected before erase during FM6 staging, the bank transfer completes, and autosave writes resume after verified release.
- Extended `tests/sidecar_pair_transaction_test.c` to exercise independent Base A/B histories for all seven logical identities: Project 0–3, Live Autosave and Preset Bank 0–1. Each identity commits two generations and reads back its exact matching Base + Sidecar pair; cross-identity loads are rejected. Sidecar sectors remain isolated in a test-only backend.
- Updated the CP1 queue description to reflect the expanded seven-identity pair-history coverage.

### Verification boundary

The FM6 staging reservation acceptance passed in the Fast Regression workflow before this seven-identity harness extension. On that same verified head, the target baseline build compiled the unmodified Felucca firmware, ENERGUN VCA and Sidecar V1 with the AC79 target compiler; its full host/web regression step was still running at the last check. The newly extended seven-identity harness and its queue/documentation updates are committed after that verified head, so their CI result is **pending** and must not be inferred from the earlier run.

The reservation changes are production firmware source changes in `storage.c`, `fm6_vbank.c` and `project.c`, but are not physical-device validation. No Flash sector map, whitelist, Sidecar target address, loader, OTA or factory image was changed. No device was flashed. The production Base + Sidecar coordinator and target Sidecar backend remain unimplemented; CP1.6/CP1.7 lifecycle integration is not closed.

## 8. Backend-neutral pair coordinator implementation

New follow-up on 2026-11-03:

- Added `firmware/src/energun_pair_v1.h` and `firmware/src/energun_pair_v1.c`.
- The coordinator accepts explicit Base and Sidecar backend callbacks; it defines no Flash addresses, object IDs, sectors or target whitelist.
- Commit sequence is now implemented in reusable code: read selected Base metadata; inspect both Sidecar A/B candidates; protect the matching generation and choose the safe Sidecar target; encode the future Base sequence/CRC; commit Sidecar; read back and semantically validate that exact future-bound Sidecar; only then commit the inactive Base copy; finally verify selected Base copy, sequence, length and CRC.
- A failed Base commit leaves the future-bound Sidecar as an orphan. On retry, the selector protects the old matching Sidecar and reuses the orphan side rather than overwriting the only valid pair.
- Pair load selects the exact current Base metadata, requires a matching identity + sequence + payload CRC Sidecar, validates its semantic payload, then loads and rechecks the exact Base copy before returning both payloads.
- Extended `tests/sidecar_pair_transaction_test.c` so seven-identity history, pair load, Sidecar torn-write sweep, Base torn-write sweep, failed Base commit/retry and corrupted-newest-Base fallback run through the new coordinator. Its adapters use actual `storage.c` with simulated NOR and a separate in-memory Sidecar A/B backend.
- CP1 queue now marks the coordinator power-loss harness executable. Production pair commit ordering remains blocked until a target backend and runtime call sites are approved and integrated.

### Scope and verification

This is a backend-neutral transaction coordinator implementation and host acceptance, **not** production FM-1 runtime integration. It is not included in the Felucca main translation unit; the target baseline workflow now includes a pi32v2 compile-only probe. No Sidecar Flash sectors or object IDs have been assigned, no storage whitelist or loader/OTA/factory code has changed, and no device was flashed.

The new coordinator and expanded transaction/fault tests are committed after the previously verified HEAD `7c0a884ee46c1c9b93064d701a3574c6c27db773`. Their new CI run must complete on the exact new HEAD before these changes are marked verified.

Coordinator follow-up: added `energun_pair_v1_erase()`. It commits a zero-length Base tombstone to the inactive Base copy and verifies the new generation. Existing Sidecar records are left physically untouched but become non-matching orphans; the next save creates a fresh generation-bound Sidecar. Host tests cover failed erase preserving the old pair, successful erase hiding stale Sidecar data, and subsequent slot reuse. This avoids an unsafe in-place Sidecar erase and does not require any new physical Flash allocation.



## 9. Pair coordinator verification and lifecycle call-site mapping

Follow-up on 2026-11-03:

- Fixed the coordinator CRC32 lookup table to exactly match Felucca `storage.c` (the prior table had two incorrect nibble entries).
- Fast Regression on commit `a595435632498455cdd478bbeb5b75c4983c55cd` completed **SUCCESS** (Run `41694376373`). The focused host regression and dependency-aware CP1 queue steps both completed successfully. The coordinator AC79 compile probe is separately tracked by the baseline workflow.
- Baseline Build on the same commit `a595435632498455cdd478bbeb5b75c4983c55cd` compiled the unmodified Felucca firmware, ENERGUN VCA, Sidecar V1 and the pair coordinator with the AC79 target compiler. The full host/web regression step was still running at the last check; do not claim the complete baseline workflow is green until the final job conclusion is verified.
- Extended `tests/sidecar_pair_transaction_test.c` with lifecycle-semantic coverage: Rename changes the Base generation while preserving the exact Sidecar logical payload; Save As commits a separate destination identity and leaves the source pair intact.
- Added `docs/ENERGUN_FM1_CORE_CP1_6_7_LIFECYCLE_CALLSITE_MAP_2026-11-03.md`, mapping project Save/Save As/Rename/Erase/Load, boot scan, Live Autosave and user-preset bank boot/save/overwrite/rename/delete/load to their current Base-only call sites and required pair wrappers.
- Updated the CP1 queue description and pair coordinator stage to include lifecycle-semantic coverage.

The Rename/Save As tests are coordinator-level host semantics, not tests of production `project.c` or `upreset.c` call sites. Runtime state codecs, Base legacy fallback, production target Sidecar backend and actual lifecycle integration remain open. The new lifecycle-test and documentation commits were created after the verified `a595435` run; their CI result is pending.

No changes to `main`, V15, factory firmware, physical Flash allocation, storage whitelist, loader or OTA. No device was flashed.


## 10. Typed Sidecar payload codec layer

Follow-up on 2026-11-03:

- Added `energun_sidecar_payload_v1.h/.c` with explicit typed project/live and preset-bank payload structures and byte codecs.
- Project/live codec explicitly serializes four parts × 16 P_MC bytes, four parts × 13 Private bytes, a Motion count and 0–128 three-byte Motion events. Maximum encoded payload is exactly 501 bytes.
- Preset-bank codec explicitly serializes the 2-byte slot bitmap plus 16 × 13 Private bytes. Encoded payload is exactly 210 bytes.
- Encoding/decoding uses explicit field copies; it does not serialize C struct padding or assume adjacent struct members are byte-packed. Decode rejects malformed lengths/counts and clears unused Motion tail bytes.
- Semantic P_MC, Private, Motion and slot-bitmap validation remains centralized in `energun_sidecar_v1.c`; the typed codec does not weaken or duplicate those rules.
- Added strict host acceptance `tests/energun_sidecar_payload_v1_test.c` and runner `tests/run_sidecar_payload_v1_test.sh`. The dependency queue now runs this stage after Sidecar format validation and before the pair transaction harness.
- Fast regression count increased from 23 to 24; exact-HEAD CI is pending for the latest codec and lifecycle commits.

This is a serialization building block, not a runtime snapshot adapter. It does not yet capture live engine state, bind FUN10 E0–E7 migration, implement legacy Base fallback, or connect project/autosave/preset production call sites. No target Flash backend or physical address was introduced.


## 11. Latest integration acceleration — 2026-11-03

- CP1 queue now executes the coordinator power-loss harness only once; the previous `pair-power-loss` stage reran the identical `run_sidecar_pair_transaction_test.sh` command and added no coverage.
- CP1 queue blockers distinguish completed host components from missing production integration: Sidecar envelope/semantic validation and backend-neutral pair recovery are implemented; production Sidecar A/B backend, runtime pair restore, legacy defaults/fallback, and Sidecar-aware FM6 autosave coordination remain open.
- Implemented the frozen Sidecar Motion target whitelist in `energun_sidecar_v1.c`: Sidecar project/live Motion events accept only P_MC IDs 103–118. The Motion Lock bit is masked for whitelist comparison and preserved in the record. E0–E7 (119–126) remain in the Base FUN10 Motion stream; Private, MCX and ID 127 are rejected.
- Added strict host cases for P_MC IDs 103 and 118, locked ID 103, below-range ID 102, E0 ID 119 and unallocated ID 127. These tests are pending exact-head CI.
- The previous baseline run was cancelled when a newer commit arrived after all AC79 compile probes succeeded. Do not treat it as a complete baseline pass; the new head must complete both workflows.

Runtime integration remains deliberately gated: the agreed Private controls have no production per-part cache/owner in the current engine runtime, and the Sidecar target backend has no approved physical sector map. Do not invent an adapter that persists transient DSP state or silently changes FUN10 compatibility.
