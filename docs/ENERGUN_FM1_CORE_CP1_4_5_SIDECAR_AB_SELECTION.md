# ENERGUN FM-1 CORE — CP1.4.5 Sidecar A/B selection

Date: 2026-11-02  
Branch: `energun-metal-core`

## Scope

This change adds the first executable Sidecar A/B selection layer. It does not allocate a Sidecar storage object ID, define a physical Flash sector, connect Sidecar persistence to project/preset/autosave call sites, or write to an FM-1.

## Storage primitive

`st_read_copy_meta(obj, copy, dst, max, meta)` reads one explicitly requested existing Felucca A/B object copy:

1. validates the commit header with `st_head()`;
2. validates payload CRC with `st_body()`;
3. copies the payload out of shared `st_buf` into caller-owned memory;
4. returns that copy's local storage sequence, payload length and CRC.

It does not choose a copy, alter the storage map, or assign a new object ID. Caller-owned buffers are required because `st_buf` is shared scratch storage.

## Sidecar selector contract

`energun_sc_v1_inspect()` validates a record against its own embedded Base fingerprint. It verifies the envelope, identity and semantic payload while allowing the selector to distinguish current-generation records from valid orphan/stale records.

`energun_sc_v1_select_ab()` accepts two candidates. A candidate is eligible only when its enclosing Felucca storage header and payload CRC have already passed (`storage_valid`) and its Sidecar envelope/semantics pass inspection.

The selector returns:

- `matching_copy`: newest fully valid Sidecar bound to the exact current Base sequence **and** Base payload CRC; this is the protected old-pair copy.
- `newest_valid_copy`: newest fully valid Sidecar object by its enclosing storage sequence, even if it is an orphan for another Base generation.
- `write_copy`: opposite the protected matching copy; if there is no matching copy, opposite the newest valid Sidecar; if neither copy is valid, A.
- `next_storage_sequence`: newest fully valid Sidecar local storage sequence + 1, including when that newest record is an orphan; starts at 1 when no valid record exists.

Sequence ordering uses the same unsigned half-range rule as Felucca storage: candidate A is newer than B iff the values differ and `A - B < 0x80000000`. Exact ties deterministically select A.

## Protected-pair invariant

A valid Sidecar matching the currently selected Base is never chosen as the write target while it is the only/newest matching copy. A newer orphan cannot displace that protected copy. Local Sidecar sequence ordering and pair membership are separate decisions: local sequence chooses the next record number; exact Base sequence + CRC chooses which record belongs to the current pair.

If neither copy matches the current Base, there is no valid current pair to protect; the selector preserves the newest valid orphan and chooses the other copy as the write target.

## Automated acceptance

Test file: `tests/sidecar_ab_selection_test.c`  
Runner: `tests/run_sidecar_ab_selection_test.sh`

The test compiles the real `storage.c` and Sidecar V1 module against simulated NOR. It covers:

- actual explicit A/B storage reads and outer CRC validation;
- older matching pair protected against a newer orphan;
- no matching candidate, preserving newest orphan;
- matching B protected when orphan A has a higher local sequence;
- both copies matching, newest matching protected;
- sequence wrap and deterministic tie handling;
- outer-storage-invalid and malformed Sidecar candidates ignored;
- no-valid-copy initialization;
- null-argument rejection;
- unchanged ordinary Base load behavior.

CI: [ENERGUN METAL CORE fast regression 41478848470](https://github.com/energundenis/Felucca/actions/runs/41478848470), exact tested code HEAD `277313eeda4bd3805ea0c910e3298633c041d03b`, **SUCCESS**.

Results on that HEAD:
- existing focused regression: 21/21 PASS;
- Sidecar V1 format: 38 checks, 0 failures;
- Base-generation binding: 16 checks, 0 failures;
- Sidecar A/B selection: 17 checks, 0 failures;
- sequential CP1 queue: Storage A/B PASS → FM6 staging/recovery PASS → Sidecar Format PASS → Base-generation binding PASS → Sidecar A/B selection PASS → BLOCKED at Pair Commit Ordering.

The queue block is expected: the production Base + Sidecar pair transaction coordinator is the next unimplemented stage. CI workflow success means all executable tests passed and the queue correctly reported the next implementation blocker; it does not mean the whole CP1 queue is complete.

## Safety boundary

- Host simulated NOR only.
- No Sidecar object ID or `OBJ_COUNT` change.
- No physical Flash-sector allocation or map change.
- No archive V1/V2, bootloader, OTA or recovery changes.
- No physical FM-1 access or write.
