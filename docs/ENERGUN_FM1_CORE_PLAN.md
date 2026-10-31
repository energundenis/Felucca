# ENERGUN FM-1 CORE — Implementation Plan

Status: planning baseline (2026-10-31)
Repository: `energundenis/Felucca`
Working branch: `energun-metal-core`
Target hardware: M-VAVE FM-1
Firmware foundation: Felucca
Sound-engine direction: ENERGUN METAL CORE V1

## Project boundaries and safety

- This is a standalone FM-1 firmware project.
- Do not mix this work with ENERGUN SYNTH LAB, browser-based Metal Core prototypes, or other ENERGUN projects.
- Do not modify `main` as part of this work.
- Preserve the factory firmware and the ability to return to it. No device flashing until the build, package, recovery path, and hardware test plan are verified.
- Keep each working milestone in a separate commit; do not overwrite known-good milestones.
- The selected direction is Option A: Felucca as the firmware foundation, FMini-1 as the interface reference, and ENERGUN METAL CORE V1 as the sound-engine design target.

## Verified repository state at plan creation

- The `energun-metal-core` branch exists.
- `firmware/src/eng_metal_core.c` is currently an isolated integration scaffold.
- Its source comment explicitly says it is not included by `engines.c`, exports no functions or symbols, and is not a playable registered engine.
- The scaffold records pending work: parameter map and signal-flow agreement; oscillator/FM/fold/LPG implementation; note lifecycle and block rendering; host regression tests and CPU baseline; append-only engine registration and UI pages.
- The scaffold specifies an initial monophonic voice mode, additive rendering into the host mix, host-provided stereo path, bounded fixed-point DSP, and per-part memory constraints.
- FOLLOW and METAL are independent ON/OFF controls in the OSCILLATORS / SYNTH VOICE block (not the FOLD page), per `docs/ENERGUN_METAL_CORE_SIGNAL_FLOW.md`. OFF bypasses only the corresponding processing contribution while retaining its associated values.
- No project-specific document at `docs/ENERGUN_FM1_CORE.md` was found on this branch at audit time.
- Do not treat DSP Stage 1/2/3 as integrated or complete based only on references to commits or other branch names. Their presence and ancestry must be verified before relying on them.
- No successful full firmware build or physical-device test of ENERGUN METAL CORE is established by this plan.

## Audit update — 2026-10-31

- The comparison baseline on `main` is `65b21d344fca68bd4bbc1eb24d7b90edfbe98200`. The working branch is 79 commits ahead of `main` and 0 behind it (rechecked against the current branch ref on 2026-10-31). The previously recorded `4ea3ed8` SHA was an earlier documentation commit, not the current branch HEAD.
- The separate DSP prototype branch is not merged. Its reports describe successful host-side subsystem tests and syntax-only checking of `engines.c`; GitHub Actions runs 41155060811, 41155691495, 41156380647, 41157732492, and 41158153324 were independently checked and all completed successfully. These checks do not establish a full JieLi target firmware build, target CPU/RAM measurements, or physical FM-1 validation.
- The integration audit records Felucca-Engines pin `d0260074ce100a47675814b7f38d61f11176c926` and felucca-bsp pin `5942e68b89ab9ccd23f2c541d712ca38fe635f11`; both commit objects exist in their respective upstream repositories. The actual local submodule checkout state still needs verification on the development Mac.
- The documented macOS prerequisites are Docker Desktop, Python 3 with Pillow/fontTools, JieLi pi32v2 toolchain, and AC79 SDK. Linux x86-64 runners can build without Docker. A dedicated GitHub Actions baseline workflow was added at `.github/workflows/energun-fm1-baseline.yml` in commit `fa0da918e1b13bb1034f85df65b6da18ae93ec9a`; it checks out pinned submodules recursively, installs the documented toolchain and SDK, builds Felucca, runs host/web tests, and stores build evidence as an artifact. Its first run has not yet been verified.
- The approved FOLLOW/METAL UI placement is OSCILLATORS / SYNTH VOICE. Any older FOLD-page wording in scaffold comments or this plan is stale and must not guide UI implementation.
- No firmware source has been changed during this audit. No cherry-pick, engine registration, firmware build, or device write has been performed.

## Implementation plan

### Phase 0 — Branch, source, and build audit
- [x] Record the exact HEAD SHA of `energun-metal-core`: `4ea3ed85877a8a7f241ecfdb5c9604686309a0c8` (2026-10-31 audit).
- [x] Compare ancestry and changed files with `main` and the separate DSP-stage branch; the working branch is 77 commits ahead of `main` and 0 behind it.
- [x] Inventory DSP code: the working branch contains only the unregistered scaffold. A separate branch, `metal-core-dsp-stage1-osc` (HEAD `cf22f01b23f618feda65ccde85b0503fccacc7b0`), contains OSC/FM/feedback prototypes, per-part state, an unregistered callback adapter, tests, and stage reports. It has diverged from `energun-metal-core` (86 commits on its side and 1 commit on the working-branch side after their merge base); none of that implementation is present in the working branch. No cherry-pick/port has been performed.
- [x] Verify the checked-out `firmware/engines` and `firmware/bsp` submodule SHAs against the parent repository's gitlink pins using the recursive CI checkout (run 41160651604; exact SHAs recorded below).
- [x] Build unmodified Felucca using the Linux x86-64 GitHub Actions workflow (run 41160651604; clean target build).
- [x] Record baseline test results, firmware/package sizes, and existing-engine CPU/audio-render budget from CI evidence. RAM headroom remains unmeasured and is explicitly open.
- Exit criteria: reproducible clean baseline build and an evidence-based branch/code inventory.

### Phase 1 — Sound-engine specification
- [ ] Write and approve the METAL CORE V1 signal-flow diagram.
- [ ] Define the eight engine edit parameters (P_E0..P_E7), ranges, units, defaults, and UI labels.
- [ ] Define how FOLLOW and METAL toggles retain their associated values without consuming ambiguous parameter slots.
- [ ] Define note/retrigger/legato behavior and the initial monophonic voice contract.
- [ ] Define fixed-point formats, headroom, clipping policy, and CPU/RAM budgets.
- Exit criteria: reviewed parameter map and signal-flow specification before DSP implementation.

### Phase 2 — DSP primitives in isolation
- [ ] Implement and test oscillator core and waveform generation.
- [ ] Implement FM operators/routing and modulation depth.
- [ ] Implement wavefolder and level/headroom management.
- [ ] Implement LPG response and decay behavior.
- [ ] Implement FOLLOW processing as a separately bypassable stage.
- [ ] Implement METAL processing as a separately bypassable stage.
- [ ] Implement feedback only after its routing, stability limits, delay memory, and CPU cost are specified.
- [ ] Add parameter smoothing and safe bounds where needed.
- [ ] Add deterministic host-side tests and reference renders for each primitive.
- Exit criteria: isolated DSP tests pass; no uncontrolled feedback, invalid arithmetic, or unexplained clipping; measured resource use is within budget.

### Phase 3 — Engine lifecycle and Felucca integration
- [ ] Implement the Felucca engine interface using its documented track/voice/modulation types and allocator.
- [ ] Implement note-on, note-off, retrigger, voice reset, and block rendering.
- [ ] Render additively into the host mix; do not overwrite it.
- [ ] Keep stereo in the host's existing part/mix path unless the host contract requires otherwise.
- [ ] Fit persistent per-part state within ENG_MEM_PART and use only permitted per-voice state.
- [ ] Add engine registration using a new append-only engine ID; do not renumber existing engines.
- [ ] Add regression tests for coexistence with existing engines and parts.
- Exit criteria: engine is registered, selectable, produces audio, and passes host regression tests.

### Phase 4 — FM-1 interface
- [ ] Implement the engine pages with FMini-1 as the interaction/layout reference.
- [ ] Map all eight edit parameters to the physical controls and display.
- [ ] Add independent FOLLOW and METAL switches in the OSCILLATORS / SYNTH VOICE block, retaining their associated values when OFF.
- [ ] Implement navigation, value display, and settings persistence.
- [ ] Test every control on the browser/emulator where applicable and on the physical FM-1.
- Exit criteria: all controls operate predictably and saved settings restore correctly.

### Phase 5 — Sequencer integration
- [ ] Port/adapt the agreed ENERGUN sequencer design to Felucca's timing and input architecture.
- [ ] Implement 16 steps and note data.
- [ ] Implement ACCENT and agreed step parameters.
- [ ] Implement constrained randomization, preserving step ON/OFF and ACCENT as specified.
- [ ] Implement FOLLOW behavior as defined for this project.
- [ ] Synchronize to internal and external MIDI clock.
- [ ] Test sequencing and live playing together without timing drift or audio dropouts.
- Exit criteria: repeatable 16-step playback and reliable clock synchronization on FM-1.

### Phase 6 — Optimization and regression
- [ ] Measure CPU at realistic voice/part counts and worst-case modulation.
- [ ] Measure RAM and per-part engine memory.
- [ ] Test clipping/headroom, denormals/undefined behavior, feedback stability, and parameter changes during playback.
- [ ] Run the full Felucca test suite and all new ENERGUN tests.
- [ ] Run long-duration playback and stress tests.
- Exit criteria: documented passing tests and resource budgets with no known critical audio faults.

### Phase 7 — Packaging and physical-device validation
- [ ] Produce a clearly labelled experimental build separate from factory firmware.
- [ ] Verify package format, target size, checksums, and loader compatibility.
- [ ] Document and verify the recovery/return-to-factory procedure before flashing.
- [ ] Install only after explicit go/no-go review.
- [ ] Test audio output, controls, display, MIDI, sequencer, save/load, reboot, and long-run stability.
- Exit criteria: documented successful physical FM-1 test and verified recovery route.

## Milestones and required evidence

| Milestone | Required evidence |
| --- | --- |
| CP0 — Baseline | branch SHA, dependency/toolchain inventory, clean build, baseline metrics |
| CP1 — DSP specification | approved signal flow and parameter map |
| CP2 — DSP primitives | host tests, reference renders, CPU/RAM measurements |
| CP3 — Integrated engine | engine registration, audio render, regression results |
| CP4 — Interface | FM-1 control map and device test results |
| CP5 — Sequencer | 16-step and MIDI-clock test results |
| CP6 — Release candidate | full test report, resource report, package and recovery verification |
| CP7 — Device validation | physical FM-1 test report |

## Working rule

Do not mark a checkbox complete based on an intended design, a source comment, or a test from another branch. Mark it complete only when the relevant code is present on `energun-metal-core` and the stated evidence is available. Update this document in the same milestone commit as the implementation it describes.


## Remote baseline CI — first execution (2026-10-31)

- Workflow: `.github/workflows/energun-fm1-baseline.yml`
- Run: [41160334654](https://github.com/energundenis/Felucca/actions/runs/41160334654), triggered by push on `energun-metal-core`, source commit `fa0da918e1b13bb1034f85df65b6da18ae93ec9a`.
- Verified from the run's job steps: recursive checkout, Python/Node setup, Pillow/fontTools installation, JieLi pi32v2 toolchain installation, AC79 SDK checkout, and `./build.sh` all completed successfully.
- The regression step `tests/run_tests.sh` was still in progress at the last check. The run is not yet a fully passing baseline; do not mark CP0.5 complete until the run concludes successfully.
- The parent tree gitlinks at the workflow source commit resolve exactly to `firmware/engines` = `d0260074ce100a47675814b7f38d61f11176c926` and `firmware/bsp` = `5942e68b89ab9ccd23f2c541d712ca38fe635f11`. Recursive checkout completed successfully, which confirms the workflow obtained the pinned submodules for this run.


## CP1.1 — Felucca audio integration audit (2026-10-31)

- Added source-backed audit: `docs/ENERGUN_METAL_CORE_CP1_1_AUDIO_INTEGRATION_AUDIT.md` (commit `9eadd809aae03a4ff9781404abf7fe5e5fac7fc5`).
- Confirmed host contract: shared voice allocator, mono initial Metal Core voice, `render()` adds to mix, 32-sample bounded blocks, host stereo/effects, per-part `engine_t.mem`, per-voice documented state slots.
- Confirmed V1 envelope starting point: `ownenv=0`; host ADSR is not curve-identical to v98. Keep voice ADSR, LPG decay, FM-index envelope and METAL decay independent.
- Confirmed `P_LEVEL` is final track mixer gain and `P_DETUNE` is host UNISON spread; neither replaces Metal Core internal controls.
- Parameter/storage gate: 35 independent V1 controls in current inventory; only 8 existing E values plus at most 16 additional common IDs, while current FUN10 has insufficient room for even one new complete common parameter. Sidecar and candidate flash sectors remain unapproved pending full user-device dump and storage/backup/restore tests.
- Engine registration remains pending; IDs are append-only and optional SLICE/ADPCM builds must be preserved.
- Corrected stale FOLLOW/METAL scaffold comment to OSCILLATORS / SYNTH VOICE (not FOLD), comment-only commit `d6452c1935c96e7c51048e7eb506898dd88277ce`.
- CP1.1 is an audit milestone only; no DSP algorithm, parameter ID, engine registration, storage format, runtime UI, or hardware state changed.


## CP1.2 — v98 reference/control reconciliation (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_2_REFERENCE_RECONCILIATION.md` (commit `d836cdcb8dfa4faf18d0636db783abda43e5cebe`).
- Confirmed distinct OSC1, OSC2 direct-audio, Complex FM/AM modulator, METAL FM modulator, METAL ring oscillator, and deferred RING2 oscillator roles.
- Confirmed independent MOD RATIO (3.07 default) and METAL RATIO (4.20 default), separate FM/AM controls, feedback, FM envelope, METAL trigger decay, two-stage fold, main filter and three-control LPG.
- Identified a control-inventory discrepancy: the reported 35 controls omit at least COMPLEX MODULE VOLUME and WEST COAST VOLUME. The v98 source also has conflicting output-volume assignments that require calibration. Therefore 35 is not a frozen parameter count.
- Detailed v98 signal path includes main filter, LPG filter/gain, voice VCA and METAL ring tap; the simplified top-level chain must not erase these tap points.
- CP1.2 is a reconciliation audit only. No DSP, IDs, engine registration, sidecar, package, or hardware writes.
- CP1 remains open until the full control inventory and V1 include/defer list are reconciled and approved.


## CP1.3 — complete v98 V1 parameter inventory (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_3_COMPLETE_PARAMETER_INVENTORY.md`; initial commit `5589fa69e686e17d4be08102afde90c30059d3de`, followed by correction `0df8566965e334149643326fe9fac1f1be6a1a3a`.
- Inspected the protected Library reference HTML directly, including its parameter object, UI selectors, and signal graph.
- Corrected the earlier 35-control estimate to **37 independent V1 sound controls**: 8 OSCILLATORS/SYNTH VOICE, 11 Complex Oscillator, 2 METAL-specific, 5 Wavefolder, 2 main filter, 3 LPG, 5 voice VCA/ADSR, and 1 final WEST COAST VOLUME.
- Added COMPLEX MODULE VOLUME and WEST COAST VOLUME, both absent from the former inventory. Flagged conflicting v98 output-gain assignments (VOLUME×12 vs VOLUME×5) for calibration.
- Identified a v98 inconsistency: the FM CHARACTER parameter object defaults to METALLIC (2), while the selector markup marks CLASSIC (1) selected. Effective runtime default must be verified before freezing.
- Deferred RING2, SPREAD pending routing proof, custom LFO/RANDOM/SLEW, full patch bay, and prototype DRONE/SEQ / WEST COAST ON controls (map to Felucca host lifecycle/enable).
- Host ADSR reuse remains conditional; P_LEVEL and P_DETUNE retain host semantics and are not substitutes for internal module controls.
- No IDs, editor pages, sidecar, flash addresses, firmware code, package, or hardware state changed. CP1 remains open pending inventory/scope approval, reference gain/default calibration, host ADSR parity, and storage design.


## CP1.4 — parameter access and persistence proposal (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_4_PARAMETER_ACCESS_STORAGE_PROPOSAL.md` (commit `3521cf69cc0308b61700e24d1a0f883660519928`).
- Audited the pinned `felucca_engine.h`: engine ABI exposes only `edit[8]` and one optional descriptor hook for those eight values; additional controls need common parameter pages or a deliberate private-value UI extension.
- Audited `params.c`: static four-slot pages resolve common IDs through TP[] and E0..E7 through engine descriptors. The modulation matrix exposes only E1..E8.
- Found a required motion change: common P_MC IDs inserted before E0 will not be covered by current `motion_param()`'s `id >= P_E0` rule. Add explicit P_MC range and tests if these are to support parameter locks.
- Proposed (not assigned) E0..E7 for FREQ, FM INDEX, METAL RATIO, WEST COAST DRIVE, FOLD, CUTOFF, RESONANCE, LPG DECAY; 16 common continuous P_MC controls; and a private sidecar bank of 9 or 13 values depending on host ADSR parity.
- Existing user-preset storage has 144 packed value bytes but retains the `<128` parameter-ID ceiling. FUN10 has insufficient free bytes for any new complete common per-track parameter. Existing backup object list has no Metal Core sidecar object.
- Identified the required private UI/API extension; do not put persistent controls in `engine_t.mem`, which is volatile DSP state.
- No IDs, source code, ABI, project format, backup protocol, sidecar, flash allocation or device state changed. CP1 remains open for sound-default calibration, ADSR parity, scope approval and sidecar/recovery design.


## CP1.5 — v98 behavior verification (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_5_V98_BEHAVIOR_VERIFICATION.md` (commit `c98ba7be8e4cacb4eda6893429d084e6ffca84ef`).
- Resolved WEST COAST VOLUME: steady-state reference is `P.wcVol × 5`; initialization and knob handlers briefly write ×12, but the recurring animation-frame loop overwrites this. V1 must have one writer and use the stable behavior as the reference target.
- Resolved FM CHARACTER: parameter object initializes METALLIC (2); static markup initially displays CLASSIC (1), then `wcComplexSync()` updates the UI to METALLIC during audio initialization. V1 canonical default = METALLIC.
- Found LPG MODULE VOLUME shares the same gain node as the per-trigger LPG envelope. It is a stored value but not an independent post-envelope output stage; preserve shared-node semantics or approve a deliberate redesign.
- Host ADSR reuse is not approved: v98 normal and METAL trigger schedules, VCA peak/velocity/accent scaling and sixteenth-note gate are distinct from the host voice-lifecycle ADSR. Keep v98 normal ADSR + VCA peak as internal controls for now; private bank remains 13 values.
- No parameter IDs, ABI, project/storage format, sidecar, flash, firmware package or device state changed. CP1 remains open pending scope approval, LPG behavior decision, signal map and safe sidecar/recovery design.


## CP1.6 — detailed v98 signal map (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_6_DETAILED_SIGNAL_MAP.md` (commit `33cca68deafdd57b94924600e7caa54e438dbff5`).
- Documented OSC1/Complex and OSC2 direct paths separately: OSC2 joins at the shared TIMBRE/folder input and bypasses COMPLEX MODULE VOLUME; both pass through both folder stages and WAVEFOLDER MODULE VOLUME.
- Confirmed detailed order: main LPF → LPG filter → shared LPG gain/envelope → internal voice VCA → post-VCA METAL ring AM → WEST COAST VOLUME (stable ×5) → EQ → host master/effects/stereo.
- Confirmed METAL ring is post-VCA amplitude modulation, not a pre-folder oscillator mix. METAL RATIO controls both METAL FM and ring oscillator frequencies.
- Confirmed deferred RING2 is a separate branch with a stage-1 tap and folder-output dry path, rejoining at main filter; it is not the METAL ring path.
- Added gain/headroom regression requirements for fixed-point port. No DSP code, parameter IDs, engine registration, storage, Flash or hardware changes.


## CP1.7 — DSP acceptance matrix (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_7_DSP_ACCEPTANCE_MATRIX.md` (commit `462d2a2e40a384820183ebbe989387cf8b15ec3b`).
- Defined layered evidence: static/contract, deterministic DSP unit, host engine integration, target build/disassembly cost, and physical FM-1 validation. These layers are not interchangeable.
- Added test IDs for oscillator pitch/FOLLOW, Complex FM/AM/feedback, METAL FM and post-VCA ring, two-stage fold and gain staging, main filter/LPG/VCA, host lifecycle, additive stereo rendering, numerical health, CPU and memory.
- Required deterministic fixture renders with source revision, sample rate, block size, event sequence, parameter snapshot, output hash, peak/RMS/DC and tolerances.
- Recorded current status honestly: existing scaffold test checks only isolation; ENERGUN DSP and host integration tests do not yet exist; no render symbol is present for target budget; physical tests not started.
- Flagged unresolved LPG MODULE VOLUME source-semantics conflict between CP1.5 and LPG design 02 as an explicit blocker for the LPG-03 expected result.
- No DSP, engine registration, parameter IDs, storage, Flash or device changes.


## Remote baseline CI — completed run (2026-10-31)

- Workflow run: [41160651604](https://github.com/energundenis/Felucca/actions/runs/41160651604), conclusion **success**.
- CI source commit: `d6452c1935c96e7c51048e7eb506898dd88277ce`. The current branch HEAD is `854815462ef420221f902fd0c73baf264fa5ec66`; comparison confirms all 15 commits after the CI source are documentation-only (no firmware, tests, workflow, submodule, or build-script changes). Therefore the passing baseline build covers the current firmware source tree.
- Recursive submodules checked out exactly:
  - `firmware/engines`: `d0260074ce100a47675814b7f38d61f11176c926`
  - `firmware/bsp`: `5942e68b89ab9ccd23f2c541d712ca38fe635f11`
- Environment setup passed: Python/Pillow/fontTools, Node, JieLi pi32v2 toolchain, and documented AC79 SDK.
- `./build.sh`: success. Produced:
  - `build/felucca.bin`: 513,532 bytes
  - `build/loader/ota.bin`: 7,130 bytes
  - `build/felucca.fwsc`: 610,286 bytes, package identity `FM-1_900`
- `tests/run_tests.sh`: **ALL HOST TESTS PASSED**, including web tests and web backup tests. The browser emulator was skipped because Emscripten is not installed on the runner; this is reported as a skip, not a pass.
- Target disassembly cost check passed under existing +10% budgets. Examples: `analog_render` cost 614 vs budget 585; `mix_part` 3649 vs 3549; both within configured tolerance. All listed target functions were reported ok. This is the Felucca baseline, not an ENERGUN render measurement (Metal Core has no render symbol yet).
- Artifact: [baseline build evidence](https://github.com/energundenis/Felucca/actions/runs/41160651604/artifacts/13027043947), artifact ID `13027043947`, 946,537 bytes, SHA-256 `fe3b1e45914aa1cead94e5449dc1864ec48f8e964ae6f80aa4a02a36f77d9355`, retention through 2026-11-14.
- RAM headroom is not reported by this workflow and remains an open measurement. No firmware was flashed; artifact is build evidence only.
- CP0 recursive submodule verification and clean baseline build/regression gate are complete. Physical-device validation and ENERGUN-specific DSP cost remain future work.


## CP1.8 — LPG MODULE VOLUME exact source trace (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_8_LPG_VOLUME_SOURCE_TRACE.md` (commit `aaa4f92e31c3e9a7235157658282591901a71789`).
- Materialized and inspected the protected v98 reference HTML itself, not just the previous notes.
- Resolved the apparent conflict: the MODULE VOLUME handler writes `wcLpgGain.gain`, but `wcTrig()` holds and overwrites that same AudioParam with `0.25 + RESPONSE × 0.75` and the decay envelope. Thus MODULE VOLUME is not an independent per-step gain in v98.
- Corrected `docs/ENERGUN_METAL_CORE_LPG_DESIGN_02_V98_FAITHFUL.md` (commit `239efdd810dfbf595d3b044c0118ca72abc7d1c5`) and updated CP1.7 matrix language (see current matrix revision).
- Documented three explicit V1 choices: faithful compatibility, separate post-envelope gain, or scaling the LPG envelope peak. Recommended the latter as a listening candidate, but no behavior was selected or implemented.
- No DSP, UI, IDs, ABI, storage, Flash, firmware package or hardware changes.


## CP1.9 — voice-state and feedback memory budget (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_9_VOICE_MEMORY_BUDGET.md` (initial commit `f732778e9395bb648a6d7fb991e714c7a3e45132`, refined in the current milestone commit).
- Rechecked pinned `voice_t` and engine API: three phase accumulators plus eight 32-bit state words per host voice; eight voice records per part; `engine_t.mem` provides a zeroed 12,888-byte per-part region by default (13,344 with ADPCM).
- v98 requires five independent phase sources: OSC1, OSC2, Complex modulator, METAL FM modulator and METAL ring oscillator. Three host phase words are insufficient.
- The 3 ms Complex feedback delay needs 133 int32 samples per voice at 44.1 kHz: 4,256 bytes for eight slots per part. At 96 kHz this grows to 9,216 bytes; at 192 kHz it alone requires 18,432 bytes and exceeds the current part pool.
- Direct use of `voice_t.s[8]` is too tight for five phases, two independent filter stages, envelope state and feedback cursor. Preferred proposal is a typed per-voice state record plus delay rings in `engine_t.mem`, with no shared `voice_t` ABI change. A conservative 64-byte record ×8 plus delay rings totals about 4,768 bytes at 44.1 kHz.
- This is a budget proposal only. Host-vs-engine VCA envelope ownership, exact filter topology, sample-rate support, and final state fields remain open. No code, memory request, ABI or target budget changed.


## CP1.10 — fixed-point oscillator primitive audit (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP1_10_OSCILLATOR_PRIMITIVE_AUDIT.md` (commit `42910fd3a674982d57e1a3f4193901bb9d97c55b`).
- Audited the pinned Felucca `dsp.c` and table generator: reusable `osc_sine`, `osc_tri`, polyBLEP `osc_saw`, `osc_pulse`, `pitch_inc` and `cents_inc` already exist as static fixed-point primitives.
- Mapped the five independent v98 oscillator phase sources and the waveform roles. No need to add a new public oscillator API or float math to the target.
- Flagged polyBLEP division cost and high-rate FM increment bounds for target worst-case measurement; triangle aliasing and sample-rate table consistency need explicit tests.
- Defined isolated oscillator test cases; no engine source, tests, IDs, UI, storage, firmware package, Flash or device state changed.


## CP2.1 — isolated oscillator primitive test harness (2026-10-31)

- Added `tests/metal_core_osc_test.c` (commit `784c38381fde90d5a4d4d47351ad56f39d146a00`, include correction `e9e7cb2a58402982d568c039130e2b870f17ab39`).
- Added it to `tests/run_tests.sh` (commit `cfdaec17a76b3ce1457dc928c8361825b342e5b8`).
- Host tests cover sine and triangle bounds/full scale/DC, polyBLEP saw and 50% pulse bounds/DC, A4 pitch increment, octave ratio and independent phase accumulators.
- This exercises existing Felucca fixed-point primitives only. It does not implement or register the Metal Core engine and does not claim target aliasing/audio quality.
- Host validation passed in GitHub Actions run `41203873611` at HEAD `f8511f49e558f1fb470e2cbde0c3b1ad30e039ba`: all oscillator harness checks passed, including the corrected half-scale 50% pulse expectation. This validates the host harness against the pinned Felucca primitives, not target aliasing or a Metal Core render.


## CP1.11 — LPG MODULE VOLUME behavior approved (2026-10-31)

- User explicitly approved Option C: per-trigger peak = `(0.25 + RESPONSE × 0.75) × MODULE VOLUME`.
- Preserves one LPG gain stage, RESPONSE attack contour, and DECAY tail; corrects the v98 shared-AudioParam collision without adding a post-envelope gain node.
- Added isolated bounded Q15 helper `firmware/src/eng_metal_core_math.h` and host test `tests/metal_core_lpg_test.c`; wired the test into `tests/run_tests.sh`.
- This is math-level implementation only. It is not connected to `eng_metal_core.c`, does not register the engine, and does not change IDs, UI, persistence, package or device state.
- CP1.11 decision is recorded; host-test and CI validation remain pending until the workflow completes.


## CP2.2 — LPG Option C peak math (2026-10-31)

- Added `eng_metal_lpg_peak_q15()` in `firmware/src/eng_metal_core_math.h` and host test `tests/metal_core_lpg_test.c`.
- Tests cover RESPONSE endpoints/midpoint, MODULE VOLUME mute/half/monotonicity, response contour ordering and input clamping.
- Integrated into `tests/run_tests.sh`. Host validation passed in run `41203873611` at HEAD `f8511f49e558f1fb470e2cbde0c3b1ad30e039ba`: all LPG peak-math tests passed. The helper remains isolated from the engine render path.
- Math helper is not connected to the engine render callback.


## CP2.3 — LPG timing and filter target math (2026-10-31)

- Added `eng_metal_lpg_timing_q15()` and bounded helper functions in `firmware/src/eng_metal_core_math.h`.
- Ported the v98 timing relationships into integer sample counts: LPG attack, gate-limited tail, gain decay time constant, transient filter cutoff, filter return time and filter return time constant.
- Added `tests/metal_core_lpg_timing_test.c` and integrated it into `tests/run_tests.sh`.
- The test is host-only and checks schedule math; it does not execute audio-rate envelopes or filter updates, produce reference renders, measure target CPU, or register the engine.
- Host validation passed in run `41203873611` at HEAD `f8511f49e558f1fb470e2cbde0c3b1ad30e039ba`: all LPG timing / filter-target math tests passed, including sample-rate conversion and bounds. The helper does not execute an audio-rate filter.


## CP2.4 — isolated sample-by-sample LPG gain envelope (2026-10-31)

- Added `firmware/src/eng_metal_core_lpg.h`, a fixed-point Q15 envelope state machine with a Q30 one-pole coefficient.
- Supports attack toward the approved Option C peak, gate-limited transition to the near-zero floor, exponential-style decay, idle/silence completion, and retrigger from the current held level.
- Coefficient division occurs only at trigger time; the per-sample routine performs no allocation, floating-point operation, table lookup, or division. It uses a 64-bit intermediate for the Q30 multiply; target CPU/instruction cost remains to be measured.
- Added `tests/metal_core_lpg_env_test.c` and wired it into `tests/run_tests.sh`. Tests cover attack monotonicity/time constant, gate event, decay, idle, retrigger hold, zero gate, peak clamp and null safety.
- This is a host-testable isolated DSP primitive. It does not implement the LPG filter, host note lifecycle, or complete engine render; it is not included in `engines.c` and does not change engine registration, parameter IDs, UI, storage, package or device state.
- Host validation passed in run `41203873611` at HEAD `f8511f49e558f1fb470e2cbde0c3b1ad30e039ba`: all sample-by-sample LPG envelope tests passed (attack, gate event, decay, idle, retrigger hold, zero gate, clamp and null safety). Target instruction/CPU cost of the 64-bit Q30 multiply remains open; CP2.4 is host-validated, not target-cost-cleared.


## CP2 validation update — successful host regression (2026-10-31)

- Current branch HEAD at validation: `f8511f49e558f1fb470e2cbde0c3b1ad30e039ba`.
- GitHub Actions run [41203873611](https://github.com/energundenis/Felucca/actions/runs/41203873611) completed with conclusion **success**. Target build, `tests/run_tests.sh`, web tests and backup tests all passed.
- ENERGUN oscillator tests: sine, triangle, polyBLEP saw, 50% pulse, A4 pitch increment, octave ratio and five independent phase accumulators all passed. The 50% pulse test now correctly expects approximately half-scale output from the pinned `osc_pulse()` implementation; no upstream DSP code was changed.
- ENERGUN LPG peak math, LPG timing/filter-target math, and sample-by-sample LPG envelope tests all passed with zero failures.
- Build outputs: `felucca.bin` 513,532 bytes; `loader/ota.bin` 7,130 bytes; `felucca.fwsc` 610,286 bytes (identity `FM-1_900`). Artifact ID `13043878389`, 946,537 bytes; [run artifact](https://github.com/energundenis/Felucca/actions/runs/41203873611/artifacts/13043878389).
- Browser emulator was skipped because Emscripten was not installed; this is a skip, not a pass.
- CP2.1–CP2.4 now have passing host-test evidence. They are not complete engine DSP: no ENERGUN render callback exists, the LPG filter is not implemented, and target CPU cost / RAM headroom / aliasing measurements remain open. Therefore the broad Phase 2 checkboxes remain open.
- No firmware was flashed and no physical device was changed.

## CP2.5 — FOLLOW source-provenance audit (2026-10-31)

- Added `docs/ENERGUN_METAL_CORE_CP2_5_FOLLOW_SOURCE_AUDIT.md`.
- Source inspection found that Felucca currently routes live keyboard/MIDI, arpeggiator and step-sequencer events through the same `trk_note_on()` API; engine callbacks receive no note-origin field.
- The resolved `vmod_t.inc` pitch also includes host pitch modulation, so simply ignoring it with FOLLOW OFF would incorrectly suppress live performance pitch and unrelated modulation.
- Proposed a host-owned per-voice provenance contract and deterministic FOLLOW-01..08 acceptance tests. Exact v98 base-frequency mapping and arpeggiator/preview policy remain to be verified.
- Documentation-only milestone. No host source, shared engine ABI, DSP code, engine registration, IDs, UI, storage, package or device state changed.


## CP2.5 reference-copy follow-up — FOLLOW pitch mapping (2026-10-31)

- Inspected the available Library file `energun_westcoast_synth_v99_metal_core_vcf_testA.html`, which identifies itself as an experimental copy based on v98 while preserving the original v98.
- The v99-derived step mapping is `FREQ × 2^(NOTE/12) × 2^(NOTE OCTAVE)`; ROOT uses `FREQ × 2^OCTAVE`. NOTE is a semitone offset from base FREQ, not an absolute MIDI note number.
- The v99 copy has two issues requiring explicit V1 handling: its trigger hard-codes Complex MOD ratio to 1.5 despite the independent MOD RATIO control; and FOLLOW OFF does not restore base FREQ, leaving the last followed frequency until another frequency update.
- The original v98 file was not present in the currently searchable Library scope, so these findings are labelled v99-derived and are not claimed as direct v98 verification.
- The CP2.5 audit remains a design gate; no pitch-selection code or host note-source metadata has been added.


## CP2.6 — isolated FOLLOW fixed-point pitch math (2026-10-31)

- Added `firmware/src/eng_metal_core_follow.h` with Q24 semitone/octave ratio tables, base-frequency clamping and Q0.32 phase-increment conversion.
- Added `tests/metal_core_follow_test.c` and wired it into `tests/run_tests.sh`.
- The source selector encodes the proposed behavior: FOLLOW OFF bypasses step-sequencer pitch to base FREQ; performance-origin pitch remains host-tracked.
- The helper accepts relative NOTE/OCTAVE values. It does not yet map Felucca's absolute MIDI step notes or receive real host note-source metadata.
- Direct v98 verification, target CPU cost and engine/render integration remain open. CI validation is pending.


## CP2.7 — host note-source provenance audit (2026-10-31)

- Audited Felucca `seq.c`, `voice.c`, `core.h`, and the pinned `Felucca-Engines` callback contract.
- Identified the four direct note-on ingress paths: sequencer step, step preview via sequencer, arpeggiator, and keyboard/MIDI input.
- Confirmed a transient track flag alone is insufficient: engine-switch notes are queued, MONO/LEGATO can change pitch without calling engine note_on, and held-note fallback can restore a note of a different origin.
- Proposed host-owned per-voice and per-held-note source metadata, with pending engine-switch source queued alongside note/velocity. Shared `voice_t` ABI remains unchanged.
- CP2.7 is a design audit only. CP2.6 CI is still pending; no FOLLOW integration is connected to the render callback.


## CP2.8 — host note-source provenance implementation (2026-10-31)

- Added host-owned `voice_source[NVOICE]` and paired `mono_source[]` metadata; shared `voice_t` ABI and Felucca-Engines submodule remain unchanged.
- Added source-aware `trk_note_on_source()` while retaining the original three-argument `trk_note_on()` as a performance-source compatibility wrapper for existing host/test call sites.
- Sequencer step playback and stopped-transport step preview are marked SEQUENCER; keyboard/MIDI and arpeggiator remain PERFORMANCE.
- Preserved source through fresh voice allocation, mono/legato pitch changes, held-note fallback, same-pitch source replacement, and the pending engine-switch queue.
- Added host tests for POLY, LEGATO fallback, same-pitch retrigger, real `seq_step()`, keyboard/MIDI input, stopped step preview, arpeggiator, and queued note cancellation.
- CI validation is pending; CP2.8 is not yet considered validated and does not connect FOLLOW to the render callback.

## CP2.9 — absolute MIDI to relative FOLLOW mapping (2026-10-31)

- Added `eng_metal_follow_from_midi()` to convert the host's final absolute MIDI note into the v99-derived NOTE 0..11 plus NOTE OCTAVE pair.
- The V1 integration convention uses Felucca's keyboard C4 anchor (MIDI 60): C4 maps to base FREQ, C5 to +1 octave, B3 to B at -1 octave. This is an explicit host adapter convention, not direct v98 source verification; Felucca's `P_ROOT` remains a scale setting and is not repurposed.
- Negative MIDI offsets use floor division; octave is clamped to the isolated helper's -2..+2 range. Added host tests for C4, C#4, B4, C5, B3, C3, both clamps and invalid inputs.
- Added `docs/ENERGUN_METAL_CORE_CP2_9_MIDI_FOLLOW_ADAPTER.md`.
- No engine render integration, engine registration, parameter ID, UI, storage format, firmware package, factory image or device state changed. Full CI validation pending.



## CP2.10 — host-to-engine FOLLOW pitch contract audit (2026-10-31)

- Inspected the real host render path in `voice.c` and `mod.c`. Engine render receives `track_t *t`, `voice_t *v`, and final `vmod_t *m` after glide, tune, pitch bend, LFO pitch, envelope pitch, fine pitch and modulation-matrix pitch.
- Host note provenance is available per voice in `t->voice_source[v - t->v]`; no shared `voice_t` ABI change is required.
- Critical design constraint: FOLLOW selection must not discard independent host pitch offsets. A simple choice between bare base-FREQ increment and final host `m.inc` is insufficient; the host pitch delta must be preserved and applied to the selected base/step pitch.
- Review found CP2.9's current -2..+2 octave clamp does not preserve MIDI notes outside C2..C6 relative to C4. Existing MIDI endpoint tests prove deterministic clamp behavior only, not full-range pitch tracking. A full-range table or explicit out-of-range fallback is required before render integration.
- Added `docs/ENERGUN_METAL_CORE_CP2_10_HOST_FOLLOW_PITCH_CONTRACT.md` and appended limitations to CP2.9 documentation.
- Documentation-only audit. No engine callback, DSP render, registration, UI, parameter IDs, storage, package or device state changed.
- CI for implementation HEAD `2b3d1484e7b69c7d0df81ff438e5a895cf01c91f` remains in progress; CP2.8/CP2.9 are not validated yet.


## CP2.11 — host pitch-delta fixed-point math (2026-10-31)

- Added `eng_metal_follow_inc_ratio_q24()` to derive a Q24 ratio from two same-rate Q0.32 host phase increments; this preserves all pitch effects already combined by Felucca rather than reimplementing pitch bend/tune/glide/modulation.
- Added `eng_metal_follow_host_phase_inc()`: PERFORMANCE and SEQUENCER+FOLLOW ON use final host pitch relative to C4; SEQUENCER+FOLLOW OFF uses final host pitch relative to the target step note, retaining glide and independent pitch effects while removing the absolute step note.
- Added Nyquist saturation at Q0.32 `0x80000000`, invalid-reference and zero-rate handling.
- Added isolated host tests for all three selection modes, +10% host pitch offset, invalid inputs and Nyquist clamp.
- Added `docs/ENERGUN_METAL_CORE_CP2_11_HOST_PITCH_DELTA_MATH.md`.
- CI validation pending. Helper is not connected to engine render; target pitch-table/sample-rate adapter and full MIDI range still need verification.


## CP2.8 / CP2.9 validation update (2026-10-31)

- CI run `41208854407`, source commit `2b3d1484e7b69c7d0df81ff438e5a895cf01c91f`: firmware build and complete host/web regression succeeded; log reports `ALL HOST TESTS PASSED`.
- CP2.8: all 11 note-source provenance cases passed. Host-side provenance implementation is validated; render integration remains pending.
- CP2.9: isolated FOLLOW and MIDI adapter tests passed with zero failures. Bounded arithmetic prototype is validated only. The adapter's -2..+2 octave clamp remains a known limitation, so full MIDI 0..127 mapping is not yet validated.
- CP2.11 host pitch-delta helper has a separate CI run `41210288637` in progress at commit `5cba63fd7b9877896daaa1f32a2a4b9010071dce`.
