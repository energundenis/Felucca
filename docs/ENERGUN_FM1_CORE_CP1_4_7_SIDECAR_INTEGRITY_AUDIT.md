# ENERGUN FM-1 CORE — CP1.4.7 Sidecar CRC and integrity audit

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **SOURCE AUDIT COMPLETE; INTEGRITY POLICY DRAFT; NO SIDECAR IMPLEMENTATION OR ABI APPROVED**

## 1. Scope

This audit examines the existing Felucca storage integrity primitives and defines a conservative integrity contract for the proposed CP1.4.5 Sidecar. It does not add a storage object, change the storage header, assign an ABI, allocate Flash sectors, or modify production code.

## 2. Existing integrity primitives

### 2.1 Storage A/B object layer

Source: `firmware/src/storage.c`.

The current storage object header is a fixed 32-byte `st_hdr_t`:

- magic, object type and copy slot;
- per-object sequence and payload length;
- payload CRC-32;
- two reserved 32-bit words;
- header CRC-32.

The implementation uses the reflected zlib CRC-32 polynomial (`0xEDB88320`), with the standard initial/final XOR. The existing host test verifies the canonical check vector:

- `CRC32("123456789") = 0xCBF43926`.

Integrity coverage is split:

- `hcrc` covers the first 28 bytes of the storage header (everything except `hcrc`);
- `crc` covers the complete object payload, exactly `len` bytes.

The save path erases the inactive copy, writes payload pages, then writes the commit header last. It reads the new header and payload back before reporting success. The loader validates the header and payload CRC and can fall back to the older A/B copy when the newest payload is corrupt.

### 2.2 Existing regression evidence

`tests/storage_test.c` already covers:

- the standard CRC-32 check vector;
- A/B save and load;
- a torn payload/header write preserving the previous copy;
- corruption of both payload copies;
- corruption of both headers;
- fallback to the older copy when the newer payload is corrupt;
- every-byte payload/header power-cut injection;
- erase failure and write protection;
- sequence wrap;
- invalid object IDs and payload-size bounds.

This is strong evidence for the generic storage layer. It is not yet Sidecar-specific acceptance evidence.

### 2.3 Other integrity boundaries

- Project FUN formats have their own format-level checksum/hash validation in `project.c`; that is distinct from the outer storage-object CRC.
- User-preset banks are stored through the same A/B storage layer; the storage payload CRC protects the complete bank object.
- Backup LIST/PUT uses `st_crc32` for transport object checks. PUT validates the complete staged object CRC before committing any object. This is a transport integrity boundary, not a Sidecar pair transaction.
- None of these existing checks authenticates data cryptographically. CRC-32 detects accidental corruption; it is not a tamper-proof signature.

## 3. Recommended Sidecar integrity policy

### 3.1 Do not add a second payload CRC by default

The Sidecar is planned as one ordinary Felucca storage object. The existing outer payload CRC already covers every byte of its Sidecar envelope and logical payload. The outer header CRC separately protects the commit metadata.

Therefore the recommended policy is:

1. Reuse the existing storage-layer CRC-32 unchanged.
2. Do not duplicate the same CRC inside the Sidecar payload unless a future transport/export requirement proves that an independently portable Sidecar record needs its own checksum.
3. Keep Sidecar semantic validation separate from CRC validation.

This avoids two competing checksums, duplicate versioning rules and unnecessary bytes. The CP1.4.5 envelope draft already says not to duplicate the outer commit record.

### 3.2 Semantic validation after CRC

A matching Sidecar candidate must pass all of the following before any values are applied:

- exact Sidecar magic;
- supported format and logical schema versions;
- expected identity kind and identity index;
- exact base-object sequence binding;
- payload length within the identity-specific maximum and exactly consistent with the declared logical counts;
- valid slot-presence bitmap and no data inherited by absent slots;
- every parameter key belongs to the frozen logical whitelist;
- every value is valid for that key's declared type and encoding;
- P_MC Motion Lock records use valid P_MC IDs and values;
- no duplicate (part/track, step, parameter) event keys;
- base motion count plus Sidecar P_MC motion count never exceeds the shared 128-event pool;
- reserved bits/fields have their specified values;
- no unrecognized trailing bytes unless a future schema explicitly permits them.

CRC-valid but semantically invalid data is rejected as a whole. Do not partially apply a Sidecar.

### 3.3 A/B selection must include base-sequence binding

The current `st_current()` helper selects the newest valid storage copy based on its local sequence and payload CRC. That is not sufficient for Sidecar selection: a newer Sidecar can be CRC-valid but bound to a different base-object sequence.

The future Sidecar reader must inspect both copies independently:

1. Validate each storage header and payload CRC.
2. Validate the Sidecar envelope and identity.
3. Reject a candidate whose bound base sequence does not exactly equal the currently selected base object's sequence.
4. Among the remaining matching candidates, choose the newest Sidecar-local sequence using the existing wrap-safe ordering; use a deterministic A-copy tie rule.
5. If no candidate matches, report the extension as missing and load the base object with deterministic extension defaults.

A stale but CRC-valid Sidecar must never be attached to a newer base object. A corrupt newest copy must not prevent selection of an older CRC-valid copy that matches the current base sequence.

The matching reader must not rely on `st_current()` alone, because that helper returns one local-sequence winner before the caller can apply the base-sequence predicate.

### 3.4 Failure behavior

- Invalid Sidecar CRC: reject that copy and inspect the other copy.
- Invalid header: reject that copy and inspect the other copy.
- Valid CRC but unsupported schema, wrong identity, malformed values or stale base sequence: reject that candidate.
- No valid matching copy: preserve and load the valid base object; initialize extension values to documented defaults; do not erase or rewrite either Sidecar copy during load.
- Sidecar failure must not make an otherwise valid legacy-compatible base project or preset bank appear empty.
- Diagnostics should distinguish missing, stale, CRC-invalid and schema-invalid extension states, without turning a recoverable extension failure into a destructive recovery action.

## 4. Pair-transaction integrity implications

The CP1.4.5 ordering remains necessary:

1. Snapshot and validate the complete extension.
2. Determine the next base-object sequence.
3. Commit the Sidecar copy tagged with that next sequence.
4. Commit the base object.
5. Read back and verify both the base sequence and matching Sidecar binding.

The outer CRCs establish byte integrity for each committed object. The base-sequence binding establishes pair identity. Semantic validation establishes that the extension is safe to apply. These are three different checks and none substitutes for the others.

For a power cut before the new base commit, the old base continues to select its old matching Sidecar. A future-bound Sidecar is ignored. After the new base commits, it selects the new matching Sidecar. A restore operation must rebind to the destination base sequence; copying a valid CRC-protected Sidecar byte-for-byte is not sufficient.

## 5. Sidecar-specific acceptance tests still required

The existing generic storage tests do not close CP1.4.7. Add tests against the eventual production Sidecar reader/coordinator for:

1. One corrupt Sidecar payload copy and one valid matching copy: valid copy selected.
2. Newer local sequence with bad payload CRC: older matching copy selected.
3. Both Sidecar copies CRC-invalid: base loads; extension defaults; no write/erase occurs.
4. Valid CRC with wrong base sequence: candidate rejected as stale.
5. Newer CRC-valid but stale copy plus older CRC-valid matching copy: older matching copy selected.
6. Both CRC-valid copies bound to another base sequence: extension treated as missing.
7. Valid CRC with malformed magic/version/identity/length/count/reserved fields: whole candidate rejected.
8. Valid CRC with an unknown parameter ID, out-of-range value, duplicate motion key or motion-pool overflow: whole candidate rejected.
9. Power cut at every Sidecar payload/header write boundary: previous base/Sidecar pair remains loadable.
10. Power cut between Sidecar commit and base commit: old base remains paired with its old Sidecar.
11. Base commit completed: new base selects only its matching Sidecar.
12. Archive V2 restore: the Sidecar is validated and rebound to the destination base sequence before it becomes active.

These tests must use the production storage and Sidecar reader code, not a separately reimplemented CRC model.

## 6. Remaining dependencies and gates

- CP1.4.5 must freeze the logical envelope and its exact field widths before binary validation code is written.
- CP1.4.2 must complete value-encoding and compatibility acceptance.
- CP1.4.3/4.4 must freeze Private value types and the P_MC Motion whitelist.
- CP1.4.8 must add the A/B, pair-commit and fault-injection matrix.
- CP1.4.9 archive V2 must preserve integrity checks and destination-sequence rebinding.
- CP1.4.11 must independently audit physical Flash and recovery. This CRC audit does not select or authorize sectors.

## 7. Acceptance boundary

**Completed:** source audit of the existing CRC-32, header/payload validation, A/B fallback, backup transport CRC and generic storage fault tests.

**Draft recommendation:** reuse the existing outer CRC-32; do not add an inner Sidecar CRC by default; add Sidecar-specific envelope, identity, base-sequence and semantic validation; select matching A/B candidates rather than relying on `st_current()`.

**Not completed:** no Sidecar CRC/reader/transaction code, no binary ABI, no Sidecar-specific production test, no Flash allocation, and no hardware operation.

**CP1.4.7 remains OPEN.**
