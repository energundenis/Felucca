# ENERGUN FM-1 CORE — CP1 / CP2 / CP3 status snapshot

Date: 2026-11-04  
Repository: energundenis/Felucca  
Work branch: energun-metal-core  
Initial verified code/test HEAD: 78cc46c39b5b7599c894a2a144cea0f649105beb  
Latest verified code/test HEAD: 404a51ab03171fe893721ce17665ff461625734f  
Latest docs checkpoint before this status update: f943824758187a839ccdbe12ce1f081098c9f517 (documentation-only after code/test HEAD 404a51ab03171fe893721ce17665ff461625734f)

## Overall status

| Workstream | Completion estimate | Meaning |
|---|---:|---|
| CP1 — persistence / Sidecar / recovery | **76%** | Logical schema, backend-neutral coordinator and extensive simulated-NOR acceptance are largely implemented; target backend and production lifecycle integration remain open |
| CP2 — DSP / FOLLOW / VCA | **72%** | FOLLOW and VCA contracts plus the VCA-only host integration harness are accepted; the complete METAL CORE DSP chain is not integrated |
| CP3 — voice / sequencer / engine integration | **63%** | Real Felucca host dispatcher/sequencer and isolated VCA callback tests exist; full DSP state, production registration and target real-time qualification remain open |

Percentages are engineering estimates across design, implementation, host tests and production gates. They are not test pass rates or a claim of hardware readiness. Host/compile-only evidence is explicitly separated from production firmware and device validation.

## CP1 — persistence and recovery (76%)

| Item | Progress | Evidence / remaining work |
|---|---:|---|
| CP1.4.1 parameter / storage inventory | 100% | Logical ownership and inventory recorded |
| CP1.4.2 compatibility / legacy Base | 85% | Compatibility audit exists; production FUN10 E0–E7 fallback/default application still needs call-site proof |
| CP1.4.3–1.4.4 Private runtime / Motion whitelist | 90% | Typed Private cache, payload bridge and frozen Motion whitelist are host-tested; production project/preset state restore is open |
| CP1.4.5 pair coordinator and transaction harness | 95% | Sidecar-first/Base-second coordinator, readback, generation binding, tombstone, retry and seven-identity simulated-NOR tests pass |
| CP1.4.6 capacity | 100% | Logical payload capacity and codecs checked |
| CP1.4.7 integrity | 95% | Envelope, CRC, semantic validation and identity rules host-tested; target storage integration remains separate |
| CP1.4.8 fault injection | 100% host | Base and Sidecar torn-write sweeps pass; added Sidecar record/header byte-cut sweep passes on exact HEAD 78cc46c |
| CP1.4.9 Archive V2 | 100% offline validation / 0% pair restore | Browser validator passes; pair-aware firmware capture/restore is not implemented |
| CP1.4.10 production runtime integration | 20% | Adapters and compile probes exist; project.c, autosave_poll() and upreset.c are not wired to the production pair backend |
| CP1.4.11 Flash / recovery gate | 70% source audit | Source/ownership audit recorded; address-level physical allocation and recovery qualification remain blocked |
| CP1.6 project/live lifecycle | 35% | Host adapter covers pair Save/Load/Rename/Save As/Erase semantics; real project, boot and autosave call sites remain open |
| CP1.7 preset-bank lifecycle | 35% | Host bank/slot adapter covers Private isolation and preservation; real upreset.c hooks remain open |

### CP1 queue evidence

Fast Regression on exact HEAD 78cc46c39b5b7599c894a2a144cea0f649105beb:

- Run 41734515051 — SUCCESS, #445: https://github.com/energundenis/Felucca/actions/runs/41734515051
- All 24 focused test programs/suites passed.
- The CP1 queue passed its storage, FM6, Private, Sidecar format, Base binding, A/B selection, payload codec/bridge, pair transaction, legacy fallback, identity, integrity, Archive V2 offline, reservation and project/preset host-adapter stages.
- Queue result remains BLOCKED at production pair ordering and runtime recovery. The queue's successful workflow conclusion means the tests/report ran; it does not mean those production gates passed.

The queue wording was then corrected in test/queue commit 8b333643e78c34e4bd050337da9d2f43f06ff31b. Fast Regression [run 41741370066](https://github.com/energundenis/Felucca/actions/runs/41741370066) also completed **SUCCESS** on that exact SHA: all 24 focused tests passed and the queue still reported BLOCKED only at the explicit production gates. The companion Full Baseline [run 41741369998](https://github.com/energundenis/Felucca/actions/runs/41741369998) was still in progress at the time of this status refresh.

Full Baseline on the same exact HEAD:

- Run 41734515025 — SUCCESS, #572: https://github.com/energundenis/Felucca/actions/runs/41734515025
- Unmodified Felucca firmware build and full host/web regression passed.
- AC79 pi32v2 compile-only probes passed for the isolated ENERGUN VCA, Sidecar V1, typed payload codecs, Private runtime/bridge, pair coordinator and project/preset lifecycle adapters.
- No target runtime measurement or physical FM-1 test was performed.

### CP1 blockers

1. No approved target Sidecar A/B backend or physical allocation. Do not invent addresses, sectors, object IDs or alter the hardware Flash map.
2. Real project.c Save/Load/Rename/Erase/boot and autosave_poll()/autosave_boot() pair call sites remain unwired.
3. Real upreset.c bank/slot save, load, rename and erase integration remains unwired.
4. Runtime Private/P_MC/Motion snapshot and restore plus legacy FUN10 E0–E7 canonical-default handling need production-path tests.
5. Archive V2 pair-aware staging/commit/restore is not implemented.
6. The physical rescue/recovery path and target ownership review remain separate safety gates.

## CP2 — FOLLOW and VCA (72%)

| Item | Progress | Evidence / remaining work |
|---|---:|---|
| FOLLOW / Note Source / pitch contracts | 100% host contract | Source, pitch-delta and adapter contracts are recorded and tested |
| VCA arithmetic / ownenv lifecycle | 100% host contract | Fixed-point VCA, ownenv behavior, retrigger and lifecycle contracts are tested |
| Explicit Gate Off | 100% host contract | Gate-off state transition and release behavior are implemented/tested |
| Accent provenance | 100% host contract | Explicit note metadata is propagated; velocity 127 alone does not infer Accent |
| CP2.23 VCA-only integration harness | 100% — closed | Eight independent voice states, additive render, host Q15 ramp, Gate, retrigger, Release and done lifecycle accepted; AC79 compile probe passed |
| Complete OSC/FM/METAL/FOLD/filter/LPG/VCA production chain | 15% | Individual primitives exist, but the complete production callback chain is not connected |
| Full-engine target CPU / cycle budget | 10% | Compile/disassembly evidence exists; no cycle-accurate target measurement of the complete chain |

### CP2 blockers

- Integrate the complete fixed-point signal chain in isolated steps without changing the production engine registry prematurely.
- Freeze and test all filter/FM/FOLD/LPG state and retrigger ownership.
- Measure the complete worst-case callback on AC79; the existing Felucca render-loop budget does not include the full METAL CORE chain.
- Complete the approved parameter/storage map before exposing controls in production.

## CP3 — host voice / sequencer / engine integration (63%)

| Item | Progress | Evidence / remaining work |
|---|---:|---|
| CP3.0 host callback / voice contract | 100% audit | Real Felucca voice.c / seq.c behavior and block-boundary Gate semantics documented |
| Real dispatcher / sequencer lifecycle test | 100% host | Test executes Felucca hostsim production sources for POLY, MONO, LEGATO, UNISON, note-off, step dispatch and Accent provenance |
| Test-only METAL VCA callback dispatch | 100% host | Private test descriptor exercises real host track_render() / engine callback path without allocating a production engine ID |
| CP3.1 state / memory accounting | 55% | VCA/LPG and provisional filter/FM/delay state subtotals recorded; real ENG_MEM backing/claim contract is host-tested across all four parts for the 288-byte VCA test state; final complete state structure not frozen |
| CP3.1 fixed-point biquad | 45% | Isolated candidate and host/reference tests exist in Draft PR #1; no production filter integration or frozen state ABI |
| CP3.2 complete DSP chain integration | 15% | VCA-only harness exists; complete oscillator/FM/FOLD/main-filter/LPG chain remains unconnected |
| CP3.3 parameter map / stable engine ID / production registry | 0% | 37-control V1 include/defer map and optional SLICE/ADPCM-safe append-only engine ID remain unresolved |
| CP3.4 full target acceptance | 10% | Compile-only evidence only; complete target cycle measurement and device audio validation not performed |

### CP3 blockers

- Freeze complete per-part/per-voice state and retrigger reset/keep semantics; prove fit for both ADPCM build configurations.
- Finalize the 37-control V1 parameter include/defer map and storage/editor/preset implications.
- Reserve a stable append-only engine ID that cannot collide across optional SLICE/ADPCM builds.
- The test-only METAL VCA descriptor already passes hostsim cross-part eight-voice budget, stage-4 steal fade, done() retirement and stale-state reset checks. Remaining: repeat this with the eventual production-registered complete DSP engine and qualify its target timing.
- Complete full-chain target code-generation and cycle measurements; no hardware audio claim is made.

## Open pull requests at inspection

| PR | State | Scope | Exact head / CI evidence |
|---|---|---|---|
| #1 — https://github.com/energundenis/Felucca/pull/1 | Draft, open | CP3.1 isolated Q2.30 DF2T biquad | Head bf5af2e; run 41485732114 SUCCESS. Candidate only; not production integration |
| #2 — https://github.com/energundenis/Felucca/pull/2 | Draft, open | CP3 host lifecycle workstream | Head 820bc84; fast run 41430946644 and baseline run 41430946663 SUCCESS |
| #4 — https://github.com/energundenis/Felucca/pull/4 | Draft, open | CP8 parallel queue/orchestrator foundation | Head 07f9385; run 41669946022 SUCCESS. Separate orchestration workstream |
| #5 — https://github.com/energundenis/Felucca/pull/5 | Open, ready for review | CP1 Sidecar torn-write recovery sweep | Head 78cc46c; Fast Regression and Full Baseline above both SUCCESS. Base is isolated validation branch validation/cp1-pair-recovery-20261104; not merged |

## Safety and release boundary

- main and Factory V15 were not changed.
- No physical Flash address/sector was allocated or written.
- No firmware was flashed and no physical FM-1 runtime/audio test was performed.
- AC79 compile-only checks are not runtime/cycle validation.
- No PR was merged. Production engine registration, full DSP chain, target Sidecar backend and runtime persistence call sites remain gated.


## Latest CP1 / CP3 verification checkpoint — 2026-11-04

Exact code/test HEAD: 404a51ab03171fe893721ce17665ff461625734f.

- Fast Regression [run 41796239006](https://github.com/energundenis/Felucca/actions/runs/41796239006): **SUCCESS**. All 24 focused test programs/suites passed.
- The CP3 real VCA dispatch test now checks all four ENG_MEM part regions: mapping, pairwise separation, non-aliasing writes, first-claim zeroing, same-owner retention and bounded owner-change clearing. All six aggregate checks passed.
- The dependency-aware CP1 queue ran its executable host stages, including Private cache/bridge, Sidecar validation, generation binding, A/B selection, pair coordinator fault injection, project/live lifecycle adapter and preset-bank lifecycle adapter. The queue result is **BLOCKED** at production pair-commit ordering / runtime integration, as intended. The host suite's SUCCESS does not close the target-storage or physical Flash gates.
- Full Baseline [run 41796239193](https://github.com/energundenis/Felucca/actions/runs/41796239193) was **IN PROGRESS** at this status update. Unmodified Felucca firmware build and AC79 compile-only probes for VCA, Sidecar, payload codecs, Private cache/bridge, pair coordinator and lifecycle adapters had passed; final host/web regression was not yet confirmed.

### Current independent pull requests observed 2026-11-04

| PR | Scope | Head | Latest evidence at inspection |
|---|---|---|---|
| [#6](https://github.com/energundenis/Felucca/pull/6) | CP3.1 shared Felucca engine-memory budget constants | aaf6faaee310053509add3c205f34458aaee5172 | Fast run 41795538110 SUCCESS; Full Baseline 41795538116 still running at inspection |
| [#7](https://github.com/energundenis/Felucca/pull/7) | CP2 LPG cutoff input bounds | 4a433b973778409bfb4fdf2a88cbc0654bdd819b | Fast run 41795684551 SUCCESS; Full Baseline 41795684705 still running at inspection |

These PRs remain open and unmerged. The CP3.1 budget PR changes the shared accounting constants only; it does not freeze the full METAL CORE state layout. The CP2 LPG bounds PR is outside this CP1/CP3 increment.
