# ENERGUN FM-1 CORE — project documentation index

**Canonical working branch:** `energun-metal-core`  
**Last consolidated:** 2026-11-02

Start here:
1. [Cross-branch canonical project base and decision register](ENERGUN_FM1_CORE_CROSS_BRANCH_CONSOLIDATION_2026-11-02.md)
2. [CP1.4.1 frozen logical parameter map — corrected](ENERGUN_METAL_CORE_CP1_4_1_FROZEN_PARAMETER_MAP.md)
3. [CP1.4 parameter access and storage proposal](ENERGUN_METAL_CORE_CP1_4_PARAMETER_ACCESS_STORAGE_PROPOSAL.md)

## Workstream navigation

- **Parameter inventory / persistence:** CP1.3, CP1.4, CP1.4.1, CP1.4.2–4.11; source closure: `ENERGUN_METAL_CORE_CP1_4_1_FROZEN_PARAMETER_MAP.md`.
- **Parameter encoding / compatibility audit:** `ENERGUN_FM1_CORE_CP1_4_2_COMPATIBILITY_AUDIT.md` (V1 standard 7-bit encoding accepted; per-control curves/quantization, base-project migration and user-preset extension scope remain open).
- **Per-control conversion table:** `ENERGUN_FM1_CORE_CP1_4_2_PARAMETER_CONVERSION_TABLE_DRAFT.md` (30-control host reference; 9,434 source-step boundaries; 100,001-point nearest-code bound sweep per control; endpoint clamping; corrected signed bipolar mapping; 17/17 focused tests pass). Exact-head full baseline CI also passed on code HEAD `89528e4`, including AC79 VCA compile probe and all host/web tests. Musical-resolution review and legacy project/preset compatibility fixtures remain open.
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

- Latest fast lane [41408456648](https://github.com/energundenis/Felucca/actions/runs/41408456648) passed all 17 focused tests on code HEAD `992e459`; it includes the real OSC host lifecycle dispatch test. Full baseline for this exact HEAD is still running.
