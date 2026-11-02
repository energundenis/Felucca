# ENERGUN FM-1 CORE — CP1.4.7 exact-generation reader implementation audit

Date: 2026-11-02  
Branch: `energun/cp1-4-5-pair-coordinator`  
Status: reader implementation candidate; exact-head host CI PASS (run 41511905591, tested HEAD `a82512e02c782145cdef43572ed1aaa5118b86ba`).

## Scope

This work separates three integrity layers:

1. **Storage-object integrity:** Felucca's existing A/B header and payload CRC-32. A candidate is storage-valid only after both header validation and complete payload CRC pass.
2. **Sidecar envelope integrity:** magic, format/schema, identity, Base sequence + payload CRC fingerprint, payload length and reserved-field checks.
3. **Logical semantic integrity:** P_MC/Private domains, preset bitmap, Motion event keys/values and shared 128-event-pool capacity.

The outer storage CRC covers the complete Sidecar record, including its envelope. V1 therefore does not duplicate an inner CRC. CRC-32 detects accidental corruption; it is not authentication.

## Exact-generation reader

Files:

- `firmware/src/energun_sidecar_v1.h`
- `firmware/src/energun_sidecar_v1.c`
- `tests/sidecar_reader_test.c`
- `tests/run_sidecar_reader_test.sh`

New API: `energun_sc_v1_read_matching()`.

Inputs include both A/B candidate records, their storage-valid flags and local sequences, the selected Base sequence + payload CRC, identity and current Base Motion-pool occupancy.

Rules:

- Ignore a candidate unless the storage backend has already validated its outer header and payload CRC.
- Validate the Sidecar envelope and semantic payload against the exact selected Base generation.
- Never return stale or orphan data as active logical values, even when its local A/B sequence is higher.
- If both copies match and validate, select the newer local sequence using unsigned half-range ordering; exact ties select A.
- Return `ENERGUN_SC_ERR_NO_MATCH` if neither copy is an exact semantic match; selected copy remains -1.

The API is backend-neutral. It assigns no production object ID, Flash sector or physical address and does not call the physical storage driver.

## Focused acceptance cases

The strict host test covers:

1. Newest valid exact-generation copy wins.
2. Higher-sequence orphan cannot override an exact Base match.
3. Candidate rejected by outer storage CRC is excluded.
4. Newer semantic-invalid record is rejected.
5. Equal local sequence deterministically selects A.
6. Unsigned sequence wrap selects newer B.
7. No exact-generation record returns NO_MATCH and clears selection.
8. Motion pool occupancy outside 0..128 is rejected.
9. Invalid identity is rejected before selection.

The CP1 queue stage `sidecar-read-validation` runs this suite after pair power-loss tests.

## Coordinator regression discovered and corrected

The coordinator's post-commit verification previously passed the **old** Base Motion-pool occupancy after the new Base had committed. This can reject a valid new pair when the Base pool occupancy decreases and the Sidecar uses the newly freed capacity.

The coordinator now validates the final pair using `request->base_motion_count`; pre-write selection continues to use `request->current_base_motion_count`. The regression fixture models old Base occupancy 1 and new Base occupancy 0, allowing the new Sidecar to use all 128 event slots. Exact-head CI must confirm this correction.

## Remaining CP1.4.7 gates

This work does **not** close CP1.4.7. Remaining items:

- Freeze V1 envelope/payload ABI after CP1.4.4 assigns the P_MC numeric-ID whitelist and all value domains are reconciled.
- Bind the reader to a production storage backend only after CP1.4.11 Flash map / bootloader / OTA / recovery audit authorizes an allocation.
- Verify the real backend's outer CRC-validity contract and explicit A/B reads.
- Add end-to-end corrupt-header, corrupt-payload, semantic-invalid, stale-generation, fallback and no-valid-copy recovery tests through the production adapter.
- Add project/live-autosave/preset-bank roundtrip and power-loss recovery coverage.
- Audit AC79 RAM/stack placement for coordinator and reader workspaces.
- Preserve Archive V1 legacy behavior; Archive V2 remains a separate pair-aware restore gate.

## Evidence boundary

Evidence at this stage is strict host C11 execution and simulated-NOR tests only. It is not target compilation, target runtime or physical FM-1 validation. No factory firmware, physical FM-1, production object IDs or Flash sectors were modified or assigned.


## Exact-head CI result

Run [41511905591](https://github.com/energundenis/Felucca/actions/runs/41511905591) completed successfully against `a82512e02c782145cdef43572ed1aaa5118b86ba`. The new exact-generation reader suite passed 10/10. The pair coordinator ordering suite passed 6/6; power-loss suite passed 19/19. The dependency-aware queue correctly stops at `sidecar-integrity`, because ABI freeze and production storage-backend integration remain open. Evidence is host C11 / simulated NOR only.
