# ENERGUN FM-1 CORE — CP1.4.6 Sidecar logical capacity audit

Date: 2026-11-02  
Repository: `energundenis/Felucca`  
Branch: `energun-metal-core`  
Status: **LOGICAL PAYLOAD SIZES VERIFIED; ENVELOPE ALLOWANCE PROVISIONAL; NO FLASH ALLOCATION**

## 1. Scope and limits

This audit calculates the logical payload sizes implied by the reconciled CP1.4.1 map and the CP1.4.5 Sidecar schema draft.

It is a logical storage-capacity calculation only. It does not freeze the Sidecar binary ABI, add storage object IDs, select physical Flash sectors, change `OBJ_COUNT`, change `storage.c`, or authorize a physical device write.

## 2. Source constraints

Verified Felucca storage limits:
- `ST_SECTOR = 4096` bytes.
- Existing A/B storage header is 32 bytes and is committed last.
- `ST_PAYLOAD_OFF = 256`; therefore the maximum object payload is `ST_PAYLOAD_MAX = 3840` bytes.
- Existing project FUN10 is exactly 3840 bytes and already consumes the full object payload.
- Existing user-preset record has 144 packed parameter bytes; `P_COUNT=127` fits, and the full bank object is 3080 bytes (8-byte bank header + 16 × 192-byte records).
- Existing archive staging scratch is sized from `proj_wire_u.raw`, with a compile-time assertion that it can stage project/preset patch objects; archive object IDs and browser manifests are currently fixed to the V1 object set.

Source files:
- `firmware/src/storage.c` — current A/B object capacity and header-last commit.
- `firmware/src/project.c` — fixed 3840-byte FUN10 layout.
- `firmware/src/upreset.c` — 144-byte preset parameter area, 192-byte record, 16 records per bank.
- `firmware/src/editor_backup.c` — archive staging and object size limits.

## 3. Project and live-autosave Sidecar payload

The reconciled CP1.4.1 allocation is 16 common P_MC values and 13 Private values per part. Four parts therefore require:

| Component | Calculation | Bytes |
|---|---:|---:|
| P_MC values | 4 × 16 × 1 byte | 64 |
| Private values | 4 × 13 × 1 byte | 52 |
| P_MC Motion event count | 1 byte | 1 |
| P_MC Motion events | 128 × 3 bytes | 384 |
| **Maximum logical payload** | 64 + 52 + 1 + 384 | **501** |

The 128-event limit is shared with the existing four-track motion pool. This is a maximum, not an additional independent pool. Runtime validation must enforce base FUN10 motion count + Sidecar P_MC motion count <= 128.

## 4. User-preset bank Sidecar payload

The two user-preset banks contain 16 records each. P_MC values remain in the base preset records; the Sidecar stores only the 13 Private values for each used slot.

| Component | Calculation | Bytes |
|---|---:|---:|
| Private values | 16 × 13 × 1 byte | 208 |
| Slot-presence bitmap | 16 slots ÷ 8 bits | 2 |
| **Maximum logical payload before envelope** | 208 + 2 | **210** |

The base preset bank already stores P_MC values within each record's 144-byte parameter area. The 127-value count fits that area. Empty/deleted slots must have their Sidecar Private data cleared or marked absent so stale values cannot attach to a reused slot.

## 5. Envelope allowance and object capacity

The current CP1.4.5 draft defines logical envelope fields but deliberately does not assign final byte widths or a binary ABI. For capacity planning only, reserve up to **64 bytes** for Sidecar identity, schema version, base-sequence binding, payload length, flags and future-compatible metadata. This 64-byte allowance is conservative planning headroom, not an approved field layout.

| Sidecar identity | Logical data | Provisional envelope budget | Planning total | Existing object payload limit |
|---|---:|---:|---:|---:|
| Project slot / live identity | 501 | 64 | 565 | 3840 |
| User-preset bank | 210 | 64 | 274 | 3840 |

Both logical records fit comfortably within one existing storage-object payload limit. Remaining margin under the provisional envelope budget:
- Project/live identity: 3840 − 565 = **3275 bytes**.
- User-preset bank: 3840 − 274 = **3566 bytes**.

This proves logical record size is not the limiting factor within the existing 3840-byte object payload. It does not prove physical Flash availability or authorize use of any sector.

## 6. Logical copy count

Seven logical identities remain in scope:
- four project slots;
- one live-autosave identity;
- two user-preset banks.

Each uses an A/B pair: **7 × 2 = 14 logical copies**.

If each copy were eventually allocated one 4 KiB sector, the arithmetic reservation would be 14 × 4096 = **57,344 bytes (56 KiB)**. This is only a capacity thought experiment. Existing storage map ranges are already assigned to other objects, and no Sidecar sector is selected by this audit.

## 7. Archive and RAM implications

- The logical Sidecar records are much smaller than the current 3840-byte archive staging limit, so payload size alone does not require enlarging that scratch buffer.
- Archive V1 does not enumerate Sidecar identities and its firmware/browser object-ID validation must be extended in a future archive V2 design. This is a protocol/transaction gap, not a capacity gap.
- The Sidecar P_MC Motion events must be validated against the shared 128-record limit and merged without creating a second independent runtime event pool.
- Exact RAM use for typed Private caches, transaction snapshots, and pair-restore staging remains a separate AC79 linker-map/stack audit. Do not infer runtime RAM fit from the Flash payload calculation.

## 8. Remaining gates

1. Freeze exact envelope field widths and reserved-field policy in CP1.4.5; replace the provisional 64-byte allowance with an exact encoded size.
2. Confirm all Private enum/boolean/continuous values fit the selected one-byte typed encoding.
3. Confirm the complete user-preset slot-presence and bank-generation representation.
4. Implement and test pair-aware A/B save/restore, archive V2 rebinding and power-loss behavior.
5. Measure runtime cache, snapshots, stack and full engine memory on AC79.
6. Perform the independent CP1.4.11 full Flash map / bootloader / OTA / recovery audit before any physical allocation.

## 9. Acceptance boundary

**CP1.4.6 logical payload arithmetic: VERIFIED.**  
**Exact envelope ABI: OPEN.**  
**Physical Flash capacity/allocation: NOT VERIFIED / NOT AUTHORIZED.**  
**No production code, storage IDs, sectors, factory firmware or physical FM-1 state changed.**
