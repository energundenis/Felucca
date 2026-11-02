# ENERGUN FM-1 CORE — Master Status
Date: 2026-11-02
Canonical repository: `energundenis/Felucca`
Canonical working branch: `energun-metal-core`
Documentation synchronization base HEAD: `069b43f1aac4a08aaf927fac92c2852526a03cf5`
Latest code/test HEAD with confirmed fast-lane result: `651da48f95a90743b77383e181907c9ac4fc9942`

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
| CP1.4.5 | Sidecar schema and pair transaction | **50%** | V1 binary envelope/payload frozen and implemented as a storage-independent C encoder/validator; strict host suite passes 35 checks. No storage reader, A/B selector or pair coordinator yet. |
| CP1.4.6 | Sidecar logical capacity | **100%** | Capacity arithmetic verified: 501-byte project/live payload and 210-byte preset-bank payload before provisional envelope; no physical sector allocation. |
| CP1.4.7 | CRC/integrity | **35%** | Existing outer CRC reuse policy is implemented in the format contract; standalone validator checks semantic schema and exact Base sequence + payload-CRC binding. No storage-integrated Sidecar reader, A/B fallback selection or pair fault suite yet. [Integrity audit](ENERGUN_FM1_CORE_CP1_4_7_SIDECAR_INTEGRITY_AUDIT.md). |
| CP1.4.8 | Automated storage/fault regression | **40%** | New strict Sidecar V1 host suite passes 38 checks and is wired into the sequential CI queue. Pair-commit, storage-backed A/B, power-loss and complete round-trip matrix remains open. |
| CP1.4.9 | Backup/Restore audit | **100% audit** | Source audit complete: archive V1 omits Sidecar; archive V2 needs pair-aware restore and destination-sequence rebinding. Implementation remains 0%. |
| CP1.4.10 | Integration | **20%** | Integration dependencies and boundaries are documented; production Private/P_MC/Sidecar integration is not implemented. |
| CP1.4.11 | Flash map, bootloader and recovery audit | **0%** | Physical allocation remains blocked pending a complete source/image map and recovery qualification. No sectors are selected or authorized. |

CP1.4 percentages distinguish completion of an audit/design deliverable from implementation. In particular, CP1.4.6 and the CP1.4.9 source audit are complete only within their stated scopes; neither authorizes Flash allocation or archive V2 implementation.

## Current CI evidence

- PR #1 branch head `304085f743c748fa657cf7199a6d79de803c0d73`: fast regression [41426208152](https://github.com/energundenis/Felucca/actions/runs/41426208152), biquad candidate/target probe [41426208192](https://github.com/energundenis/Felucca/actions/runs/41426208192), and baseline build [41426208286](https://github.com/energundenis/Felucca/actions/runs/41426208286) all completed SUCCESS. The baseline workflow built unmodified Felucca, compiled the VCA probe with AC79, and completed host/web regression; this is not a METAL CORE full-engine target build.
- PR #2 branch head `1090091999c87f94da7068b5879dec39bc235820`: focused fast regression [41426956119](https://github.com/energundenis/Felucca/actions/runs/41426956119) and baseline [41426956021](https://github.com/energundenis/Felucca/actions/runs/41426956021) both completed SUCCESS. The baseline includes the unmodified Felucca build, AC79 VCA compile-only probe and host/web regression.
- Isolated DSP branch documentation-resync commit `7b47c18556e94b60d66c40f2676fac4d91c4d2af`: branch `metal-core-dsp-stage1-osc` is ahead 89 / behind 0 against `energun-metal-core`. Latest stage workflow [41430073973](https://github.com/energundenis/Felucca/actions/runs/41430073973) completed SUCCESS, including the real `engines.c` syntax-only check and OSC/FM/feedback host tests at 44.1 and 96 kHz. The branch remains isolated and unregistered.

- Fast regression [41460427671](https://github.com/energundenis/Felucca/actions/runs/41460427671): **SUCCESS**, 21/21 existing focused regression programs/suites plus the Sidecar V1 strict C11 suite (**38 checks, 0 failures**); sequential CP1 queue passes stages 1–4 and stops at stage 5 (`base-generation-binding`) because the production sequence-aware storage reader is not implemented. Exact tested code HEAD `651da48f95a90743b77383e181907c9ac4fc9942`.
- Full baseline [41417141732](https://github.com/energundenis/Felucca/actions/runs/41417141732): **IN PROGRESS at last inspection** on the same code HEAD. Build and AC79 VCA compile-only probe had passed; Felucca host/web regression was still running. Do not report this run as successful until GitHub records completed SUCCESS.
- Previous full baseline [41410200166](https://github.com/energundenis/Felucca/actions/runs/41410200166): SUCCESS on older code HEAD `33327eb312a36b83b3751560d899c577147125e4`; useful historical evidence, not a replacement for exact-head CI.
- CP3.1 VCA real-pool run [41410200181](https://github.com/energundenis/Felucca/actions/runs/41410200181): SUCCESS, 17/17 on its exact older code HEAD. It remains a scoped historical result, not full engine qualification.

Documentation-only commits after the tested code HEAD do not change the tested firmware source, but a new code/test change requires its own relevant CI run. Do not transfer a PASS result to a different code HEAD without a tree comparison or exact-head run.

## Parallel workstreams and non-integration rule

- DSP Stage 1–7, VCA and Sequencer V0.3 remain separately developed artifacts. They are not one registered production METAL CORE engine.
- CP2.23 is a VCA-only real-track integration harness, not full METAL CORE integration.
- CP3 lifecycle tests cover real Felucca host paths within their stated scope; they do not establish production engine registration or full DSP graph.
- Source branches are preserved. No source branch merge is implied by this status document.

## Next execution order

1. Finish CP1.4.2: review audible/musical 7-bit resolution, lock per-control mapping decisions, close remaining legacy project/preset compatibility evidence.
2. Complete CP1.4.3 and CP1.4.4 implementation design review, then implement typed Private access and explicit P_MC Motion Lock numeric-ID whitelist behind tests.
3. Continue CP1.4.5 with a sequence-and-payload-CRC-aware Base metadata reader, then Sidecar A/B matching-copy selection and pair coordinator; retain the protected-old-pair invariant.
4. Extend CP1.4.7/4.8 with real storage-backed Sidecar reads, A/B fallback, power-loss injection and complete round-trip regression.
5. Implement CP1.4.9 archive V2 only after pair semantics are stable; preserve archive V1.
6. Complete CP1.4.10 integration and CP1.4.11 independent Flash/bootloader/recovery audit before any physical allocation.
7. Continue CP3 production engine integration in parallel only where its dependencies are already fixed; keep production registration gated.
8. Start CP4/CP5 production UI and sequencer integration after stable parameter and engine contracts exist.
9. Close CP6 target resource/stress qualification and CP7 package/recovery/device validation only after prior gates pass.

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
- Exact tested code HEAD: `651da48f95a90743b77383e181907c9ac4fc9942`. The standalone module passes an AC79 pi32v2 compile-only probe in baseline run [41460427579](https://github.com/energundenis/Felucca/actions/runs/41460427579); the overall baseline host/web regression was still running at last inspection. The module is not linked into firmware.
- Updated CP1.4.5 to 50%, CP1.4.7 to 35%, CP1.4.8 to 40%. CP1 overall remains 70%; equal-weight CP0–CP7 overall remains 40%.
