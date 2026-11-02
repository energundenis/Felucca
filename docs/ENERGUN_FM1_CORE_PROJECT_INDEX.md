# ENERGUN FM-1 CORE — project documentation index

**Canonical working branch:** `energun-metal-core`  
**Last consolidated:** 2026-11-02

Start here:
0. [Master status and completion percentages](ENERGUN_FM1_CORE_MASTER_STATUS_2026-11-02.md)
1. [Cross-branch canonical project base and decision register](ENERGUN_FM1_CORE_CROSS_BRANCH_CONSOLIDATION_2026-11-02.md)
2. [CP1.4.1 frozen logical parameter map — corrected](ENERGUN_METAL_CORE_CP1_4_1_FROZEN_PARAMETER_MAP.md)
3. [CP1.4 parameter access and storage proposal](ENERGUN_METAL_CORE_CP1_4_PARAMETER_ACCESS_STORAGE_PROPOSAL.md)

## Workstream navigation

- **Parameter inventory / persistence:** CP1.3, CP1.4, CP1.4.1, CP1.4.2–4.11; source closure: `ENERGUN_METAL_CORE_CP1_4_1_FROZEN_PARAMETER_MAP.md`.
- **Parameter encoding / compatibility audit:** `ENERGUN_FM1_CORE_CP1_4_2_COMPATIBILITY_AUDIT.md` (V1 standard 7-bit encoding accepted; per-control curves/quantization, base-project migration and user-preset extension scope remain open).
- **Per-control conversion table:** `ENERGUN_FM1_CORE_CP1_4_2_PARAMETER_CONVERSION_TABLE_DRAFT.md` (30-control host reference; 9,434 source-step boundaries; 100,001-point nearest-code bound sweep per control; endpoint clamping; corrected signed bipolar mapping; 18/18 focused tests pass). Exact-head full baseline CI also passed on code HEAD `89528e4`, including AC79 VCA compile probe and all host/web tests. Musical-resolution review and legacy project/preset compatibility fixtures remain open.
- **Private access / Motion Lock:** `ENERGUN_FM1_CORE_CP1_4_3_4_PRIVATE_ACCESS_MOTION_AUDIT.md` (source audit) and `ENERGUN_FM1_CORE_CP1_4_3_4_PRIVATE_MOTION_IMPLEMENTATION_DESIGN_DRAFT.md` (typed per-part cache, editor capability protocol, explicit P_MC whitelist and test matrix; design draft only, implementation open).
- **Host VCA / sequencer lifecycle:** CP1.17–CP1.20 records, including final CI evidence in `ENERGUN_FM1_CORE_CP1_20_DEVELOPMENT_CHECKPOINT.md`.
- **V98 behavior and V1 decisions:** CP1.5, CP1.8, CP1.12–CP1.16. Their source records currently remain in `energun-fm1-cp3-host-integration`; the canonical decisions are summarized in the cross-branch register.
- **DSP / engine:** DSP Stage 1–7 and FOLLOW/MOD RATIO decision, source branch `metal-core-dsp-stage1-osc`.
- **VCA / lifecycle / memory:** CP2.23 and CP3.0/3.1, source branch `energun-fm1-cp3-host-integration`.
- **Filter qualification:** `energun-fm1-cp3-dsp-filter`.
- **AC79 / platform QA:** `energun-fm1-cp3-platform-qa`.
- **Sequencer bridge:** `fm1-dryrun-pattern-20261031`.
- **Factory firmware baseline / recovery checkpoint:** exact factory image is user-confirmed as 100% identical to the user's FM-1; the separate two-read backup workflow is reference-only, not an active stream. Verify image integrity and restore route before first physical write.

## Governance

The cross-branch register is a status/decision index, not permission to merge source branches. Keep source branches intact until a separate integration review. No physical device writes or factory firmware changes without verifying the accepted exact factory image, a practical restore path, an audited write scope and explicit authorization.

- **Sidecar design:** `ENERGUN_FM1_CORE_CP1_4_5_SIDECAR_SCHEMA_DRAFT.md` (seven logical identities, project/preset payload capacity estimates, base-sequence binding and sidecar-first A/B pair-commit model; logical draft only, no binary ABI or Flash allocation).

- Latest fast lane [41417141686](https://github.com/energundenis/Felucca/actions/runs/41417141686) passed all 18 focused test programs on exact branch HEAD `7ec54a1`; it adds a CP1.4.1 logical inventory contract test. Exact-head full baseline [41417141732](https://github.com/energundenis/Felucca/actions/runs/41417141732) was in progress at the latest check. The prior full baseline [41410200166](https://github.com/energundenis/Felucca/actions/runs/41410200166) passed on code HEAD `33327eb`.

- **CP3.1 VCA real-pool test:** fast regression [41410200181](https://github.com/energundenis/Felucca/actions/runs/41410200181) PASS on code HEAD `33327eb`; 17/17 test programs passed. VCA state is exercised in real `ENG_MEM(p)` with first-claim zeroing, same-owner retention and owner-change clearing checks. Full METAL CORE memory/CPU qualification remains open. This result remains valid for that exact code HEAD; later CP1.4.1 test commits do not themselves re-qualify CP3.

- **Exact-head full baseline update (2026-11-02):** [41410200166](https://github.com/energundenis/Felucca/actions/runs/41410200166) completed SUCCESS on code HEAD `33327eb312a36b83b3751560d899c577147125e4`, including unmodified Felucca build, AC79 VCA compile-only probe, host/web regression and artifact upload. [41410200181](https://github.com/energundenis/Felucca/actions/runs/41410200181) also passed 17/17 focused tests. This does not mean the full METAL CORE engine is registered or hardware-qualified.

- **CP1.4.9 backup/restore audit:** `ENERGUN_FM1_CORE_CP1_4_9_BACKUP_RESTORE_AUDIT.md` — current archive V1 omits Sidecar; V2 requires pair-aware restore and destination base-sequence rebinding. Source audit complete; no implementation or Flash changes.

- **CP1.4.1 E0–E7/P_MC reconciliation:** `ENERGUN_FM1_CORE_CP1_4_1_E0_E7_RECONCILIATION.md` preserves the user-approved 2026-10-30 E0–E7 order and derives 16 unique P_MC controls without duplicates.
- **CP1.4.2 legacy compatibility fixtures:** exact-head full baseline [41410200166](https://github.com/energundenis/Felucca/actions/runs/41410200166) passed the valid-checksum FUN10 `np=127` rejection fixture and the expanded 127-value user-preset map-by-count fixture (preserves E0–E7 while skipping 16 inserted common values). New-writer, Sidecar and archive V2 behavior remain open.

- **CP1.4.6 Sidecar capacity:** `ENERGUN_FM1_CORE_CP1_4_6_SIDECAR_CAPACITY_AUDIT.md` — logical payload maximum 501 bytes per project/live identity and 210 bytes per user-preset bank before envelope; provisional envelope budget is not an ABI or Flash allocation.


- **CP1.4.1 inventory contract (2026-11-02):** logical map is frozen at 8 Engine EDIT + 16 P_MC + 13 Private per part (52 Private values across four parts). New host test `tests/metal_core_parameter_inventory_test.c` guards counts, E0–E7 order, uniqueness, no E1/E2 duplication, FM INDEX/LPG DECAY placement, and independent FOLLOW/METAL booleans. Fast run [41417141686](https://github.com/energundenis/Felucca/actions/runs/41417141686) passed 18/18 on exact HEAD `7ec54a1`. This freezes logical membership only—not IDs, descriptors, Private API, Motion Lock, storage ABI or Flash layout. Full baseline [41417141732](https://github.com/energundenis/Felucca/actions/runs/41417141732) was still running at the latest check.


## Status authority — 2026-11-02

Use `ENERGUN_FM1_CORE_MASTER_STATUS_2026-11-02.md` for current CP0–CP7 percentages, CP1.4.1–CP1.4.11 progress, CI exact-HEAD boundaries and execution order. Older status paragraphs in this index and earlier checkpoint snapshots are historical evidence; where they conflict with the Master Status, the newer Master Status governs. The original implementation-plan checkbox list is a historical task breakdown, not a current progress report.
