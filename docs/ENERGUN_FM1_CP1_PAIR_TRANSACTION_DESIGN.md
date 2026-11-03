# CP1 — Base + Sidecar pair transaction coordinator

Status: **transaction-core implementation added; production storage adapter and physical Sidecar map remain blocked**  
Date: 2026-11-03

## Safety and scope

This change adds a backend-independent C coordinator core and a host fault-injection suite. It does **not** assign Sidecar Flash sectors, call physical Flash, alter factory V15, or wire new persistence into user-facing save/load callsites. The coordinator must not be treated as production-enabled until a storage adapter is reviewed and qualified.

## Commit protocol

1. A read-only adapter plans the exact inactive Base copy, next sequence, length and payload CRC.
2. A Sidecar selector protects the currently matching Sidecar copy and chooses the opposite copy (or A for a new store).
3. The coordinator encodes and semantically validates the Sidecar V1 envelope against the predicted Base generation.
4. The adapter commits Sidecar first using its own header-last A/B commit and readback.
5. The adapter commits Base second using the existing Felucca header-last A/B commit and readback.
6. The adapter reads both committed objects and validates exact Base sequence/CRC binding before success is returned.

The ordering is intentional. If Sidecar staging fails, Base is untouched. If Base erase, payload or header programming fails, the previous Base remains valid and its matching Sidecar copy is preserved; the newly staged Sidecar is an orphan. Load/recovery must select only a complete matching pair, not simply the newest Sidecar record.

## Implementation

- `firmware/src/energun_pair_txn.h`: adapter contract, exact Base metadata, caller-owned workspace, result codes.
- `firmware/src/energun_pair_txn.c`: coordinator order and mandatory post-commit pair verification.
- `tests/pair_txn_test.c`: simulated A/B storage and fault injection.
- `tests/run_pair_txn_test.sh`: strict C99 host build (`-Wall -Wextra -Werror -pedantic`).

The 521-byte Sidecar record workspace is caller-owned to avoid an implicit large stack allocation in firmware code. Production must place it in a budgeted static/pool allocation and serialize access.

## Host acceptance coverage

- successful first save and update;
- Sidecar staging failure prevents Base commit;
- Base erase, payload and header failures preserve the previous matching pair;
- orphan Sidecar is not considered a committed pair;
- post-commit verification failure is not reported as success;
- invalid API arguments cause no writes.

These tests exercise the coordinator contract through a simulated backend. They do not prove the actual SPI NOR adapter or physical power-loss behavior.

## Remaining gates

1. Freeze and review the Sidecar physical A/B sector map and reserved-region constraints.
2. Implement production callbacks using explicit Sidecar copy read/write, existing Base metadata and `st_save_to`.
3. Prove the selected Sidecar target never overwrites the currently matching copy.
4. Add real `storage.c` adapter tests with simulated NOR and deterministic failures at every erase/program/header boundary.
5. Integrate the coordinator into project/autosave/preset save callsites only after the above gates.
6. Implement pair-aware load/recovery that scans Base A/B and Sidecar A/B combinations and chooses the newest complete matching pair.
7. Re-run target compiler and RAM/stack/pool budget qualification.

No production save/load callsite is changed by this patch.
