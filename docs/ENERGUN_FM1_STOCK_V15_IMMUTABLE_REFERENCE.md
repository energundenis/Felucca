# ENERGUN FM-1 CORE — immutable factory firmware reference V15

Date: 2026-11-03  
Repository: `energundenis/Felucca`  
Feature branch: `energun-development-orchestrator`  
Status: **BINARY IDENTITY VERIFIED; FORMAT NOT FULLY DECODED; READ-ONLY REFERENCE**

## 1. Purpose

This record freezes the original M-VAVE FM-1 factory firmware V15 as the comparison baseline for ENERGUN FM-1 CORE research.

The reference is identified by the complete-file SHA-256 digest and exact byte length. The binary itself is intentionally not committed to this repository.

## 2. Verified artifact

| Field | Verified value |
| --- | --- |
| Local filename | `FM-1.fwsc` |
| Size | `699956` bytes |
| SHA-256 | `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a` |
| Reference identity | M-VAVE FM-1 stock firmware V15 |
| Verification method | Full-file SHA-256 + exact byte count |
| Verification date | 2026-11-03 |

The uploaded artifact matched both the expected digest and size exactly. A matching SHA-256 is the decisive identity check for this frozen reference.

## 3. Binary inspection boundary

A read-only preliminary inspection found:

- The file is a binary blob, not a ZIP archive or plain-text document.
- The initial bytes do not expose a recognizable plain-text firmware header.
- A small number of printable internal strings/markers are present, including `UPDATE_JUMP` and `uboot`.
- These observations are not sufficient to infer the complete container layout, compression/encryption scheme, image boundaries, target memory map, or boot/update semantics.

Therefore this document does **not** claim that the firmware format has been decoded, that its internal payload has been independently validated, or that the image is safe to flash. Further format analysis must be grounded in the matching updater/bootloader source or a documented format specification.

## 4. Immutable handling rules

1. Treat this exact V15 digest as the immutable factory reference.
2. Never modify or overwrite the original artifact.
3. Do not silently substitute V16 or another V15-looking file.
4. Keep the binary outside Git unless a separate legal and project-storage decision explicitly authorizes otherwise.
5. Use the verification utility in `tests/verify_stock_v15.py` before any future analysis that depends on this reference.
6. Do not flash, install, patch, or write to the physical FM-1 as part of reference verification.
7. Any derived extraction or analysis must be stored separately and record the source digest.

## 5. Reproducible local verification

From the repository root, with the original file available locally:

```sh
python3 tests/verify_stock_v15.py /path/to/FM-1.fwsc
```

Expected result: exit code 0 and `STOCK_V15_REFERENCE=PASS`.

The utility checks the supplied file only. It does not parse, patch, install, or transmit firmware.

## 6. Explicitly out of scope

- Full reverse engineering or decryption of the `.fwsc` container.
- Validation of every embedded firmware section.
- Flash layout assignment or changes to Felucca storage.
- Firmware installation or physical-device access.
- Any change to the factory binary.

## 7. Acceptance record

- [x] Uploaded file received.
- [x] Exact byte length verified: 699956.
- [x] Full-file SHA-256 verified against the published V15 reference digest.
- [x] Immutable reference policy documented.
- [ ] Independent format/container specification audit.
- [ ] Source-backed mapping of image sections and update-loader behavior.

The V15 identity gate is complete. The structural-format audit remains open and must not be conflated with the successful identity check.
