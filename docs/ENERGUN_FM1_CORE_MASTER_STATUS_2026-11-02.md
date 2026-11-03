# ENERGUN FM-1 CORE — Master Status
Date: 2026-11-02
Canonical repository: `energundenis/Felucca`
Canonical working branch: `energun-metal-core`
Documentation synchronization base HEAD: `35d1c2feb5df95e3b6ffeaffbff9ee03a0765a10`
Latest code/test HEAD with confirmed fast-lane result: `35d1c2feb5df95e3b6ffeaffbff9ee03a0765a10`

## Authority and percentage method
This is the current status entry point. It supersedes stale checkbox/status wording in the original planning baseline and older checkpoint snapshots. The detailed source documents remain authoritative for technical scope and acceptance.

Percentages are planning estimates based on evidence against each phase's acceptance criteria, not code-volume estimates. Work on a separate prototype branch counts only as isolated/prototype progress and does not count as production firmware integration. A host test, compile-only target probe, or design document cannot close a target/runtime/device acceptance gate.

## Project plan — CP0 to CP7

| Checkpoint | Scope | Completion | Evidence / remaining acceptance |
|---|---|---:|---|
| CP0 | Repository, source, dependencies, baseline build and CI audit | **100%** | Pinned submodules, unmodified Felucca build and baseline host/web regression are established. ENERGUN-specific target runtime/resource qualification remains outside CP0. |
| CP1 | METAL CORE specification, parameter model, access and persistence architecture | **70%** | Signal map, 37-control inventory, 8/16/13 logical map and multiple source audits are documented. Stable IDs, conversion acceptance, Private API, Sidecar ABI/transactions, CRC, archive V2 and Flash/recovery gates remain open. |
| CP2 | Isolated DSP primitives | **65%** | Oscillator/LPG math and envelope host tests exist; DSP Stage 1–7 artifacts exist on a separate branch. They are not a complete production engine and do not establish AC79 runtime cost/audio qualification. |
| CP3 | Felucca lifecycle and METAL CORE engine integration | **40%** | Real host dispatcher/sequencer and VCA-only real-track integration tests exist. Production callback/registration, complete DSP graph, full resource accounting and full integration remain open. |
| CP4 | FM-1 UI, controls, display and settings | **0%** | No production METAL CORE UI pages/control mapping have passed the checkpoint acceptance criteria. |
| CP5 | Sequencer integration | **15%** | Sequencer V0.3 JSON/offline bridge and dry-run work exist on a separate branch. Firmware/live FM-1 integration, accent mapping, per-step gate and clock qualification remain open. |
| CP6 | Optimization, regression and release qualification | **30%** | Automated host regression and baseline build workflows exist. METAL CORE target CPU/RAM, full graph stress, long-run audio and release-level qualification remain open. |
| CP7 | Packaging and physical FM-1 validation | **0%** | No installable METAL CORE release candidate or physical-device validation is established. No device write is authorized. |

Indicative overall planning progress using equal weighting for the eight CP phases: **40%**. This is a coarse project-management indicator, not a claim that 40% of firmware code is implemented.

## CP1.4 — parameter access and persistence sub-checkpoints

| Item | Scope | Completion | Current evidence / gate |
|---|---|---:|---|
| CP1.4.1 | Logical parameter inventory | **100%** | Frozen logical map: 8 Engine EDIT + 16 P_MC + 13 Private per part; four independent Private banks. Host inventory contract test passes in the 18-program fast lane. Numeric IDs and firmware descriptors are deliberately not assigned. |
| CP1.4.2 | 7-bit conversion and compatibility | **80%** | Standard Felucca 7-bit V1 encoding accepted. 30-control reference, 9,434 source-step boundaries, 100,001-point nearest-code sweeps, endpoint clamping and corrected bipolar mapping are documented/tested. Remaining: musical-resolution review, final per-control acceptance, base-compatible FUN10/preset strategy and exact-head compatibility evidence. |
| CP1.4.3 | Private parameter access/runtime model | **45%** | Typed four-part cache, lifecycle and capability-probed editor protocol are designed. No production Private API/cache/editor implementation. |
| CP1.4.4 | P_MC Motion Lock policy | **45%** | Explicit whitelist and shared 128-event-pool rules are designed. No production whitelist/merge/split implementation. |
| CP1.4.5 | Sidecar schema and pair transaction | **70%** | V1 envelope/semantic validator, exact selected-Base metadata, explicit validated A/B copy reader and orphan-aware matching-copy selector are implemented and host-tested. Pair commit coordinator and power-loss transaction tests remain open. |
| CP1.4.6 | Sidecar logical capacity | **100%** | Capacity arithmetic verified: 501-byte project/live payload and 210-byte preset-bank payload before provisional envelope; no physical sector allocation. |
| CP1.4.7 | CRC/integrity | **45%** | Outer storage CRC is validated independently for both A/B candidates; semantic inspection and exact Base sequence+payload-CRC matching determine membership. Pair recovery and power-loss integrity remain open. [Integrity audit](ENERGUN_FM1_CORE_CP1_4_7_SIDECAR_INTEGRITY_AUDIT.md). |
| CP1.4.8 | Automated storage/fault regression | **50%** | Sidecar V1 format **38/38**, Base-generation binding **16/16**, Sidecar A/B selection **17/17** pass under strict host compilation and simulated NOR. Pair-commit, power-loss and full round-trip matrix remains open. |
| CP1.4.9 | Backup/Restore audit | **100% audit** | Source audit complete: archive V1 omits Sidecar; archive V2 needs pair-aware restore and destination-sequence rebinding. Implementation remains 0%. |
| CP1.4.10 | Integration | **20%** | Integration dependencies and boundaries are documented; production Private/P_MC/Sidecar integration is not implemented. |
| CP1.4.11 | Flash map, bootloader and recovery audit | **70% (source audit)** | V15 accepted as authoritative; source-derived ownership map, loader scan windows, OTA/FM6 staging and current allowlist audited. Read-only rescue/recovery procedure, final sector policy and physical safety qualification remain open; no sectors are selected or authorized. |

CP1.4 percentages distinguish completion of an audit/design deliverable from implementation. In particular, CP1.4.6 and the CP1.4.9 source audit are complete only within their stated scopes; neither authorizes Flash allocation or archive V2 implementation.

## Current CI evidence

- PR #1 branch head `304085f743c748fa657cf7199a6d79de803c0d73`: fast regression [41426208152](https://github.com/energundenis/Felucca/actions/runs/41426208152), biquad candidate/target probe [41426208192](https://github.com/energundenis/Felucca/actions/runs/41426208192), and baseline build [41426208286](https://github.com/energundenis/Felucca/actions/runs/41426208286) all completed SUCCESS. The baseline workflow built unmodified Felucca, compiled the VCA probe with AC79, and completed host/web regression; this is not a METAL CORE full-engine target build.
- PR #2 branch head `1090091999c87f94da7068b5879dec39bc235820`: focused fast regression [41426956119](https://github.com/energundenis/Felucca/actions/runs/41426956119) and baseline [41426956021](https://github.com/energundenis/Felucca/actions/runs/41426956021) both completed SUCCESS. The baseline includes the unmodified Felucca build, AC79 VCA compile-only probe and host/web regression.
- Isolated DSP branch documentation-resync commit `7b47c18556e94b60d66c40f2676fac4d91c4d2af`: branch `metal-core-dsp-stage1-osc` is ahead 89 / behind 0 against `energun-metal-core`. Latest stage workflow [41430073973](https://github.com/energundenis/Felucca/actions/runs/41430073973) completed SUCCESS, including the real `engines.c` syntax-only check and OSC/FM/feedback host tests at 44.1 and 96 kHz. The branch remains isolated and unregistered.

- Fast regression [41478848470](https://github.com/energundenis/Felucca/actions/runs/41478848470): **SUCCESS**, 21/21 existing focused regression programs/suites; Sidecar V1 format **38/38**, Base-generation binding **16/16**, Sidecar A/B selection **17/17**. Sequential CP1 queue passes through Sidecar A/B selection and stops at Pair Commit Ordering because the production pair coordinator is not implemented. Exact tested code HEAD `277313eeda4bd3805ea0c910e3298633c041d03b`.
- Fast regression [41597864607](https://github.com/energundenis/Felucca/actions/runs/41597864607): **SUCCESS** on exact code/test HEAD `35d1c2feb5df95e3b6ffeaffbff9ee03a0765a10`. All **21 focused tests** passed; CP1 scheduler unit tests **5/5** passed; Sidecar V1 **38/38**, Base-generation binding **16/16**, A/B selection **17/17**. The queue correctly reports **BLOCKED** at Pair Commit Ordering because the production pair coordinator is deliberately not implemented.
- Full baseline [41417141732](https://github.com/energundenis/Felucca/actions/runs/41417141732): **SUCCESS** on code HEAD `7ec54a19bb1e18992c1dbc3603deba60015d411f`. This is historical evidence on a different code HEAD, not a substitute for the later exact-head run below.
- Full baseline [41481399187](https://github.com/energundenis/Felucca/actions/runs/41481399187): **SUCCESS** on exact code HEAD `ae7d01399396c4e40b3eae24d2e0ebb9aebbcd9a`. The unmodified Felucca firmware build, AC79 VCA/Sidecar compile-only probes and complete host/web regression all passed. This does not link/register the METAL CORE engine or prove target runtime/audio behavior.
- Previous full baseline [41410200166](https://github.com/energundenis/Felucca/actions/runs/41410200166): SUCCESS on older code HEAD `33327eb312a36b83b3751560d899c577147125e4`; useful historical evidence, not a replacement for exact-head CI.
- CP3.1 VCA real-pool run [41410200181](https://github.com/energundenis/Felucca/actions/runs/41410200181): SUCCESS, 17/17 on its exact older code HEAD. It remains a scoped historical result, not full engine qualification.

Documentation-only commits after the tested code HEAD do not change the tested firmware source, but a new code/test change requires its own relevant CI run. Do not transfer a PASS result to a different code HEAD without a tree comparison or exact-head run.

- Baseline build [41597864611](https://github.com/energundenis/Felucca/actions/runs/41597864611): **IN PROGRESS** on exact code/test HEAD `35d1c2feb5df95e3b6ffeaffbff9ee03a0765a10`. The unmodified Felucca firmware build and AC79 VCA/OSC/Sidecar compile-only probes have passed; full host/web regression was still running at this status update. Do not claim the full baseline as PASS until the run completes.

## Parallel workstreams and non-integration rule

- DSP Stage 1–7, VCA and Sequencer V0.3 remain separately developed artifacts. They are not one registered production METAL CORE engine.
- CP2.23 is a VCA-only real-track integration harness, not full METAL CORE integration.
- CP3 lifecycle tests cover real Felucca host paths within their stated scope; they do not establish production engine registration or full DSP graph.
- Source branches are preserved. No source branch merge is implied by this status document.

## Accepted CP1 execution order — 2026-11-03

The following six-step order is accepted as the active CP1 execution plan:

1. **CP1.4.2–CP1.4.4:** close 7-bit compatibility, Private runtime model/API, and P_MC Motion whitelist.
2. **CP1.4.5–CP1.4.8:** complete Sidecar pair transactions, integrity handling, storage-backed A/B behavior, and fault-injection regression.
3. **CP1.6:** integrate Sidecar with project Save/Load/Save As/Rename/Erase and Live Autosave.
4. **CP1.7:** integrate Sidecar with preset Save/Overwrite/Rename/Delete/Load and coordinate bank operations with FM6 staging.
5. **CP1.4.9:** implement Archive V2 after Base+Sidecar pair semantics and runtime integration are stable; preserve Archive V1 compatibility.
6. **CP1.4.10–CP1.4.11:** complete end-to-end integration and finalize the physically safe storage/recovery design. V15 is the accepted authoritative reference; physical allocation/write remains separately gated by address-level ownership, loader/OTA/FM6 collision analysis, and a verified recovery route.

Do not reorder these dependencies merely to increase parallelism. Independent host-only tests and audits may run in parallel where they do not violate the dependency order. Keep physical Flash writes, bootloader/OTA changes, device flashing, and production sector allocation unauthorized until their explicit safety gates are satisfied.

After CP1, continue CP3 production engine integration and CP4/CP5 UI/sequencer integration against the stable parameter and persistence contracts, then CP6 target qualification and CP7 release/device validation.

## CP1.4.5 coordinator design update — 2026-11-02

- Added [Sidecar Pair Transaction Coordinator design](ENERGUN_FM1_CORE_CP1_4_5_PAIR_TRANSACTION_COORDINATOR_DESIGN.md).
- Defines a protected matching-copy rule, staged future-bound Sidecar, base commit, and final pair verification; includes load selection, sequence-wrap rules, re-entrancy/serialization constraints and a host fault-injection matrix.
- The design explicitly uses the real `storage.c` behavior and extends the existing NOR simulation in `tests/storage_test.c`; it does not propose a copied test model.
- A follow-up source audit identified that `fm6_vbank.c` temporarily stages FM6 voice-bank transfers in the autosave A/B sectors. The coordinator design now requires an explicit staging reservation and live-autosave deferral; the existing `autosave_hold()` timer alone is not a hard lock.
- This is a logical design deliverable only. No firmware source, object IDs, `OBJ_COUNT`, Sidecar ABI, Flash sectors, archive V1/V2 behavior, factory image or physical device changed.
- At the time of the coordinator design commit, CP1.4.5 remained **40%** with no implementation. This historical status is superseded by the Sidecar V1 implementation and CI evidence recorded below.

## CP1.4.7 integrity audit update — 2026-11-02

- Added [CP1.4.7 Sidecar CRC and integrity audit](ENERGUN_FM1_CORE_CP1_4_7_SIDECAR_INTEGRITY_AUDIT.md).
- Confirmed the existing storage layer already provides zlib-compatible CRC-32 over the complete object payload, a separate CRC-32 over the 32-byte commit header (excluding its own CRC field), header-last A/B commits, read-back verification and fallback to an older valid copy.
- Recommended reusing the outer storage CRC without duplicating an inner Sidecar CRC by default. Sidecar must add semantic validation and a reader that evaluates both copies against the exact base-object sequence; `st_current()` alone cannot safely select a base-bound Sidecar.
- At the time of the integrity-audit commit, CP1.4.7 was **30%** for source audit and policy draft only. This historical status is superseded by the Sidecar V1 semantic validator and host tests recorded below; storage integration and A/B reader remain open.

## Safety invariants

- Keep `main` unchanged; use only `energun-metal-core` for canonical project documentation and approved work.
- Preserve the factory firmware and accepted exact factory image unchanged.
- No physical device read/write, Flash allocation, bootloader operation, PR merge, stable ID assignment or production engine registration is authorized by this status update.
- No flashing until package, exact write scope, recovery route and explicit user go/no-go approval are all verified.

## CP1.4.5 Sidecar V1 format implementation — 2026-11-02

- Added `firmware/src/energun_sidecar_v1.h` and `.c`: 20-byte little-endian `EGSC` envelope, separate format/schema versions, logical identity, exact Base sequence + Base payload CRC-32 binding, payload length and zeroed reserved field.
- Project/live payload: 64 P_MC bytes + 52 Private bytes + one Motion count + up to 128 existing three-byte events (501-byte maximum). Preset-bank payload: 2-byte presence bitmap + 16 × 13 Private bytes (210 bytes). Maximum complete project record is 521 bytes; preset-bank record is 230 bytes.
- Validator checks identity, exact Base fingerprint, lengths, P_MC domains (including signed FOLD SYMMETRY -64..63), Private Boolean/enum/7-bit domains, shared 128-event capacity, duplicate Motion keys and empty preset-slot zeroing. P_MC numeric-ID whitelist remains open until CP1.4.4 assigns IDs.
- Reuses Felucca's existing outer storage CRC-32; no duplicate inner CRC. This module is not connected to storage.c and defines no object IDs or physical sectors.
- Added `tests/energun_sidecar_v1_test.c` and `tests/run_sidecar_v1_test.sh`; the queue stage `sidecar-format` is now enabled and executes the strict C11 `-Wall -Wextra -Werror` suite.
- CI [41459497619](https://github.com/energundenis/Felucca/actions/runs/41459497619): SUCCESS. Sidecar V1: **35 checks, 0 failures**. Queue: Storage A/B PASS, FM6 staging baseline PASS, Sidecar Format PASS; next stage BLOCKED at Base-generation binding because the production storage metadata reader is not implemented.
- Exact tested code HEAD: `95a95e383015d0fb584902770d3ea9125cd3074c`. A separate AC79 pi32v2 compile-only probe passed for Sidecar V1 on code HEAD `651da48f95a90743b77383e181907c9ac4fc9942`; the latest baseline workflow is queued/running for the Base metadata helper. The Sidecar module is not linked into firmware.
- CP1.4.5/4.7/4.8 updated after A/B selector acceptance to 70%/45%/50%. CP1 overall remains 70%; equal-weight CP0–CP7 overall remains 40%.

## CP1.4.5 Base-generation binding implementation — 2026-11-02

- Added internal `st_meta_t` and `st_current_meta()` in `firmware/src/storage.c`. It returns the copy, sequence, payload length and payload CRC from the exact copy selected by the existing `st_current()` logic after the selected payload CRC has been validated.
- Refactored `st_load()` to consume the same metadata helper without changing its fallback or payload behavior.
- Added `tests/sidecar_base_binding_test.c`, which compiles the real `storage.c` and Sidecar V1 source against simulated NOR. It verifies A then B metadata, exact sequence+CRC Sidecar binding, rejection of a stale Sidecar after Base rewrite, fallback to A when newer B payload is corrupted, and that metadata for invalid objects performs no Flash reads.
- Added `tests/run_sidecar_base_binding_test.sh` and enabled queue stage `base-generation-binding`.
- CI [41462258057](https://github.com/energundenis/Felucca/actions/runs/41462258057): SUCCESS. Historical Base-generation binding stage acceptance: Sidecar V1 **38/38**; Base binding **15/15**.
- Exact tested code HEAD: `95a95e383015d0fb584902770d3ea9125cd3074c`. No Flash layout, object IDs, `OBJ_COUNT`, project/preset call sites, physical device or factory firmware changed.
- Updated CP1.4.5 to **60%**, CP1.4.7 to **40%**, CP1.4.8 to **45%**. CP1 remains 70%; equal-weight CP0–CP7 remains 40%.

## CP1.4.5 Sidecar A/B selection implementation — 2026-11-02

- Added `st_read_copy_meta()` to `firmware/src/storage.c`: explicit A/B copy read, header validation, outer payload CRC validation, caller-owned payload copy and exact local storage metadata. It assigns no object ID and makes no Flash-map change.
- Added `energun_sc_v1_inspect()` and `energun_sc_v1_select_ab()` to the Sidecar V1 module. Matching-copy protection uses exact Base sequence + payload CRC; local storage sequence is separately selected from the newest fully valid record, including an orphan. A newer orphan cannot cause the only current matching copy to be overwritten.
- Sequence ordering follows Felucca's unsigned half-range rule; ties deterministically choose A. When no matching copy exists, the newest valid orphan is preserved and the other copy is the write target. With no valid copies, A/sequence 1 is selected.
- Added `tests/sidecar_ab_selection_test.c` and `tests/run_sidecar_ab_selection_test.sh`; the suite compiles real storage and Sidecar source against simulated NOR.
- CI [41478848470](https://github.com/energundenis/Felucca/actions/runs/41478848470): **SUCCESS** on exact code HEAD `277313eeda4bd3805ea0c910e3298633c041d03b`. Existing fast regression **21/21**; Sidecar V1 **38/38**; Base binding **16/16**; A/B selection **17/17**. Queue passes first five stages and blocks at Pair Commit Ordering.
- An initial CI attempt exposed an unused static storage helper under `-Werror`; the existing Base-binding test was extended to exercise the new primitive. The subsequent run passed. A second compile-time warning about uninitialized fixture metadata was fixed before the successful run.
- Baseline workflow [41478848463](https://github.com/energundenis/Felucca/actions/runs/41478848463) on exact code HEAD `277313eeda4bd3805ea0c910e3298633c041d03b`: unmodified Felucca firmware build **PASS**, AC79 pi32v2 compile-only probes for VCA and Sidecar V1 **PASS**; full host/web regression was still running at this status update, so the complete baseline workflow is not yet marked PASS.
- Safety: no Sidecar storage object ID, `OBJ_COUNT`, physical sector allocation, archive V1/V2, bootloader, OTA, recovery or physical device changes.

## Parallel CP1 queue automation — 2026-11-02

- Replaced linear CP1 queue execution with dependency-aware DAG scheduler (`tests/run_cp1_test_queue.py`), configured for up to 3 concurrent workers.
- Sidecar format/semantic validation and Base-generation binding are now independent sibling stages after Storage A/B + FM6 staging evidence; Sidecar A/B selection joins both before proceeding.
- Added scheduler acceptance tests for actual overlap, dependency join, independent-branch continuation, failure propagation and cycle detection.
- Fast regression [41481399097](https://github.com/energundenis/Felucca/actions/runs/41481399097): **SUCCESS** on exact tested code HEAD `ae7d01399396c4e40b3eae24d2e0ebb9aebbcd9a`. All 21 focused tests passed; scheduler unit tests **5/5 PASS**, including a barrier-based proof that independent stages overlap; CP1 queue ran Sidecar format and Base binding concurrently, then A/B selection after both; Sidecar V1 **38/38**, Base binding **16/16**, A/B selection **17/17**. Queue correctly reports BLOCKED at Pair Commit Ordering because the production coordinator is not implemented.
- Baseline build [41481399187](https://github.com/energundenis/Felucca/actions/runs/41481399187) on the same code HEAD: **SUCCESS**. The unmodified Felucca build, AC79 VCA/Sidecar compile-only probes and full host/web regression completed successfully. The CP1 queue still correctly reports **BLOCKED** at Pair Commit Ordering; workflow SUCCESS means the queue detected the blocker as designed, not that the pair coordinator exists.
- This automation change does not change CP0–CP7 completion percentages. No physical FM-1, Flash layout, object IDs, bootloader, OTA or recovery changes.


## V15 reference authority — accepted 2026-11-03

- User decision: the supplied/identified FM-1 V15 reference is accepted as **100% authoritative truth** for the stock FM-1 firmware baseline. Do not treat V15 fidelity as an unresolved assumption or require a second confirmation of its correspondence to V15.
- Use V15 as the canonical reference when auditing stock firmware behavior, source structure, Flash ownership, boot/loader behavior, OTA, project/preset storage, autosave, FM6 staging, and recovery constraints.
- Preserve V15 and factory firmware unchanged. All ENERGUN work remains additive and must preserve the agreed compatibility and recovery boundaries.
- This acceptance removes the prior *provenance/fidelity uncertainty* about the V15 reference. It does not itself grant permission to write physical Flash, allocate sectors, alter the bootloader, or flash the device. Such changes still require a concrete address-level ownership map, collision analysis against all V15 users (including loader scan ranges and FM6 staging), and a verified recovery route.
- Continue the CP1.4.11 audit using V15 as the accepted source of truth. Do not report the absence of a separately uploaded 1 MiB dump as a blocker to source-level analysis. If a particular operation requires raw binary bytes or a reproducible image hash, identify that narrow requirement separately rather than questioning V15's accepted authority.
