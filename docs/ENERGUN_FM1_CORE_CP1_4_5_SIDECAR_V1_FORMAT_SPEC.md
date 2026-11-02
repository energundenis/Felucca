# ENERGUN FM-1 CORE — Sidecar V1 binary format specification

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **V1 logical envelope and payload layout frozen for host-side implementation; no storage integration or physical allocation**

## 1. Scope and safety boundary

This specification freezes the byte representation consumed by the standalone module:

- `firmware/src/energun_sidecar_v1.h`
- `firmware/src/energun_sidecar_v1.c`

The module encodes and validates a Sidecar record in RAM. It is deliberately not connected to `storage.c`, `OBJ_COUNT`, project/preset call sites, archive V1, the target Flash driver or a physical FM-1.

No storage object IDs, A/B sectors, Flash addresses, bootloader/OTA behavior or factory-firmware bytes are changed. Existing Felucca storage remains responsible for the outer commit header, A/B copies, payload CRC-32, header CRC-32, header-last commit and read-back verification.

## 2. Wire envelope — 20 bytes, little-endian

All offsets are byte offsets from the beginning of the Sidecar payload. There is no native-struct serialization, compiler padding or alignment.

| Offset | Size | Field | V1 rule |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `EGSC` |
| 4 | 1 | Format version | `1` |
| 5 | 1 | Logical schema version | `1` |
| 6 | 1 | Identity kind | 1 project, 2 live autosave, 3 user-preset bank |
| 7 | 1 | Identity index | Project 0–3; live autosave exactly 0; preset bank 0–1 |
| 8 | 4 | Base sequence | Exact committed Felucca base-object sequence, little-endian u32 |
| 12 | 4 | Base payload CRC-32 | Exact payload CRC stored in the selected Felucca base commit header, little-endian u32 |
| 16 | 2 | Logical payload length | Exact number of following bytes, little-endian u16 |
| 18 | 2 | Reserved | Must be zero in V1 |

The pair identity is **(identity kind, identity index, base sequence, base payload CRC-32)**. Requiring both sequence and payload CRC prevents an old Sidecar from attaching after theoretical sequence wrap/reuse where the sequence value alone repeats.

The Felucca storage object's own local A/B sequence and copy number remain in the existing outer storage header; they are not duplicated here.

## 3. Project and live-autosave payload

Project and live-autosave use the same payload shape. The payload begins with 116 fixed value bytes, followed by a one-byte Sidecar Motion count and zero to 128 three-byte events.

| Offset | Size | Content |
|---:|---:|---|
| 0 | 64 | Four parts × 16 P_MC raw parameter codes, part-major |
| 64 | 52 | Four parts × 13 Private values, part-major |
| 116 | 1 | Sidecar P_MC Motion event count, 0–128 |
| 117 | count × 3 | Motion events in the existing Felucca three-byte representation |

Maximum payload: 116 + 1 + (128 × 3) = **501 bytes**. Maximum complete V1 record: 20 + 501 = **521 bytes**.

### 3.1 P_MC value order and domains

Each part stores the reconciled CP1.4.1 P_MC order:

1. OSC2 DETUNE
2. FM INDEX
3. MOD RATIO
4. AM DEPTH
5. FM FEEDBACK
6. FM ENV AMT
7. FM ENV ATTACK
8. FM ENV DECAY
9. METAL DECAY
10. FOLD SYMMETRY
11. WAVEFOLDER DRIVE
12. LPG RESPONSE
13. LPG DECAY
14. VOICE VCA LEVEL
15. WAVEFOLDER MODULE VOLUME
16. WEST COAST VOLUME

All values except FOLD SYMMETRY are unsigned canonical 7-bit codes, 0–127. FOLD SYMMETRY uses the existing signed 7-bit descriptor domain -64–63, serialized as an 8-bit two's-complement byte. Physical/display conversion remains the single CP1.4.2 descriptor conversion; Sidecar does not introduce a second scale.

No numeric P_MC parameter IDs are assigned by this format. The Motion parameter whitelist and numeric-ID verification remain a separate CP1.4.4 implementation gate.

### 3.2 Private value order and types

Each part stores exactly 13 one-byte values in this order:

| Index | Key | Type / valid encoded values |
|---:|---|---|
| 0 | WAVE | Enum: 0 SINE, 1 TRI, 2 SAW |
| 1 | FOLLOW | Boolean: 0 OFF, 1 ON |
| 2 | METAL | Boolean: 0 OFF, 1 ON |
| 3 | MODULATION | Enum: 0 FM, 1 AM |
| 4 | MOD OSC WAVE | Enum: 0 SINE, 1 TRIANGLE, 2 SAW, 3 SQUARE |
| 5 | FM CHARACTER | Enum: 0 SMOOTH, 1 CLASSIC, 2 METALLIC |
| 6 | COMPLEX MODULE VOLUME | Unsigned 7-bit code: 0–127 |
| 7 | FOLD MODE | Enum: 0 SOFT, 1 CLASSIC, 2 HARD |
| 8 | LPG MODULE VOLUME | Unsigned 7-bit code: 0–127 |
| 9 | ATTACK | Unsigned 7-bit canonical time code: 0–127 |
| 10 | DECAY | Unsigned 7-bit canonical time code: 0–127 |
| 11 | SUSTAIN | Unsigned 7-bit canonical level code: 0–127 |
| 12 | RELEASE | Unsigned 7-bit canonical time code: 0–127 |

Continuous values use the accepted CP1.4.2 canonical encoding. Private values are stored independently for each of four parts. Runtime defaults are applied by the Private cache when an extension is missing; the Sidecar format does not synthesize defaults while decoding.

The v98 source object and HTML selector disagree on the initial FM CHARACTER selection (source object value 2 vs markup-selected value 1). V1 accepts the three valid enum ordinals and deliberately does not resolve that separate default/parity decision.

## 4. Motion event validation

Each three-byte event preserves the current Felucca representation:

- byte 0: `place = (track << 6) | step`;
- byte 1: 7-bit parameter ID plus the existing high-bit `MOTION_LOCK` flag;
- byte 2: signed value in the existing -64..127 container domain.

V1 validation enforces:

- Sidecar event count <= 128;
- base FUN10 motion count + Sidecar event count <= the one shared global 128-event pool;
- signed event value -64..127;
- no duplicate (place, parameter ID) pair, treating the lock flag as a kind bit rather than a different parameter.

Numeric P_MC IDs and the exact whitelist are intentionally not checked yet because stable IDs are not assigned. This is an explicit open CP1.4.4 gate, not an implicit approval of arbitrary motion targets.

## 5. User-preset bank payload

The user-preset Sidecar is bank-scoped and contains only Private values; P_MC values remain in the existing 144-byte base preset parameter record.

| Offset | Size | Content |
|---:|---:|---|
| 0 | 2 | Slot-presence bitmap; bit 0 is slot 0, least-significant bit first |
| 2 | 208 | 16 slots × 13 Private values, slot-major |

Exact payload length: **210 bytes**. Maximum complete V1 record for a preset bank: **230 bytes**.

For a slot marked absent, all 13 associated Private bytes must be zero. This prevents an erased slot from retaining or inheriting a prior preset's extension. Runtime defaults are applied when loading an absent slot.

## 6. Integrity and failure policy

- Do not add an inner Sidecar CRC-32. Felucca's existing outer storage payload CRC covers the complete Sidecar envelope and payload.
- The existing outer header CRC protects the storage commit metadata.
- The Sidecar validator checks magic, versions, identity, exact Base sequence + Base payload CRC, reserved fields, exact length, parameter domains, Private enum/Boolean domains, motion count/duplicates/ranges and preset slot-presence semantics.
- A record is rejected as a whole on any validation failure. No partial application is allowed.
- A missing/invalid/stale Sidecar must not make a valid legacy Base project or preset bank unloadable; the future load coordinator applies deterministic Private/P_MC extension defaults.
- CRC-32 detects accidental corruption; it is not cryptographic authentication.

## 7. Host implementation and acceptance

The standalone encoder/validator is compiled with strict C11 warnings-as-errors by `tests/run_sidecar_v1_test.sh`. The acceptance suite covers:

- project envelope roundtrip and exact Base sequence + CRC binding;
- project identity and live/preset identity bounds;
- all P_MC unipolar bounds and signed FOLD SYMMETRY endpoints;
- Private enum, Boolean and 7-bit value bounds;
- maximum Motion payload, shared-pool overflow, duplicate events and invalid signed values;
- preset-bank roundtrip, absent-slot stale data and invalid enum rejection;
- bad magic/version/schema/reserved bytes, truncated/trailing data;
- output-capacity and no-partial-write behavior.

The queue's stage `sidecar-format` runs this exact test command after the existing Felucca Storage A/B and FM6 baseline evidence. Later Base-binding reader, Sidecar A/B selection, pair coordinator, power-loss injection, archive V2 and FM6 ownership tests remain separately gated.

## 8. Explicit non-goals / remaining gates

This format specification does not implement:

- production storage object registration or A/B Sidecar copy selection;
- reading the selected Base commit header through a public storage API;
- Base + Sidecar pair commit coordinator;
- project, autosave, preset, archive V2 call-site integration;
- P_MC stable numeric IDs / Motion Lock whitelist;
- Private runtime cache, editor protocol or UI pages;
- physical Flash sector allocation, AC79 target runtime qualification or FM-1 writes.

**CP1.4.5 format/schema implementation can pass its host acceptance stage independently. It does not close CP1.4.5 pair transactions or CP1.4.7 storage integrity integration.**
