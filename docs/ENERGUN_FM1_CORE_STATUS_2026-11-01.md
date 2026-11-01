# ENERGUN FM-1 CORE — consolidated project status and plan

Date: 2026-11-01
Repository: energundenis/Felucca
Coordination branch: energun-metal-core
Hardware: M-VAVE FM-1; factory firmware V15 baseline

## Current verified checkpoint

**CI: PASS**

- Full baseline workflow: https://github.com/energundenis/Felucca/actions/runs/41322047870 — success; all 16 principal steps completed.
- Exact tested source SHA: 542126c0498f177f38f6f14525ac3042806cfdb7, on DSP work branch energun/cp3-1-biquad-prototype-01 (PR #1 head), not coordination branch.
- Standard unmodified Felucca firmware build: PASS.
- Isolated ENERGUN VCA compile with pinned AC79 pi32v2 compiler: PASS.
- Host Regression and Web Regression: PASS.
- Build output/size recording and evidence upload: PASS.
- Focused Metal Core fast regression: https://github.com/energundenis/Felucca/actions/runs/41322047771 — PASS on same SHA.
- Biquad host regression + AC79 compile/disassembly probe: https://github.com/energundenis/Felucca/actions/runs/41322047859 — PASS on same SHA.
- AC79 biquad inspection is compile-only; no runtime cycle timing or full-engine CPU proof.

## Plan with disposition

### A. Foundation and build pipeline
- [x] Keep FM-1 CORE separate from other ENERGUN projects.
- [x] Preserve factory firmware; no flashing or device write without explicit approval.
- [x] Establish Felucca foundation and coordination branch.
- [x] Create isolated DSP, host-integration and platform-QA workstreams.
- [x] Establish pinned AC79 SDK/toolchain CI path.
- [x] Build unmodified Felucca firmware in CI.
- [x] Pass Host + Web Regression on the exact tested DSP SHA.
- [x] Record build outputs and upload experimental evidence.

### B. METAL CORE V1 specification and architecture
- [x] Reconcile v98 signal flow and V1 scope in documentation.
- [x] Audit Felucca audio, voice, memory, parameter and storage contracts.
- [x] Approve internal VCA contour ownership (ownenv=1).
- [x] Audit and implement host Accent provenance transport.
- [x] Define CP3 integration readiness sequence and safety gates.
- [ ] Freeze final parameter mapping (37-control inventory exists; assignments/persistence not complete).
- [ ] Approve parameter storage/sidecar and recovery format using exact user-device dump.

### C. DSP qualification
- [x] Implement isolated Q2.30 DF2T biquad candidate.
- [x] Add coefficient stability, impulse, sine, independent-state, DC and saturation tests.
- [x] Replace signed division path with symmetric rounded shifts and add oracle-equivalence tests.
- [x] Pass focused host regression on exact SHA 542126c.
- [x] Inspect AC79 target code generation for isolated biquad candidate (compile-only).
- [ ] Review DSP PR #1 and obtain approval before integration.
- [ ] Integrate qualified DSP into coordination branch behind safe boundaries.
- [ ] Measure target runtime cycles under realistic block/voice counts.
- [ ] Complete full-engine CPU/headroom/stability tests.

### D. Host engine integration and lifecycle
- [x] Define isolated VCA model and host integration contract.
- [x] Pass VCA-only harness for eight independent states, additive render, host amp ramp, Accent, block-boundary Gate-off, retrigger hold/release/idle/done (on its separately recorded harness SHA).
- [x] Exercise CP3.1 memory-accounting model and candidate filter state in focused workstream (not final full-engine accounting).
- [ ] Complete real allocator/dispatcher tests for voice allocation and stealing.
- [ ] Verify MONO, LEGATO, UNISON, fresh-slot reuse, released-slot retirement, retrigger and engine switching against actual host lifecycle.
- [ ] Verify shared eight-voice budget across four parts.
- [ ] Connect full METAL CORE render callbacks; engine is not production registered/playable.
- [ ] Register with append-only engine ID only after all gates and review.

### E. Platform and resource qualification
- [x] Verify pinned toolchain/SDK setup in CI.
- [x] Compile isolated VCA for AC79.
- [x] Compile/disassemble isolated biquad candidate for AC79.
- [ ] Freeze complete typed per-voice/per-part state and exact memory footprint.
- [ ] Include coefficients, smoothing, oscillator/folder scratch, feedback delay and alignment in memory accounting.
- [ ] Measure full-engine runtime CPU at realistic/worst-case loads.
- [ ] Preserve existing thresholds; do not manufacture a pass by lowering them.

### F. FM-1 interface and persistence
- [ ] Implement METAL CORE UI pages and physical control mapping.
- [ ] Implement independent FOLLOW and METAL switches with retained values.
- [ ] Implement parameter save/restore and version compatibility.
- [ ] Implement/test sidecar A/B transaction, CRC and backup/restore only after safe storage allocation is proven.
- [ ] Test settings/projects through reboot and recovery.

### G. Sequencer integration
- [x] Audit ENERGUN Generative Sequencer V0.3 against FM-1 pattern model.
- [ ] Implement generator bridge in firmware main loop, not audio ISR.
- [ ] Implement atomic pattern publish, preview, undo/evolve and persistence policy.
- [ ] Verify 16-step behavior, ACCENT, gate semantics and MIDI clock sync.
- [ ] Test sequencer playback with live playing under load.

### H. Release and physical-device validation
- [ ] Build experimental package distinct from factory firmware.
- [ ] Verify package format, size, checksums, loader compatibility and boot constraints.
- [ ] Source-verify and test factory recovery/rollback.
- [ ] Complete final go/no-go review and obtain explicit approval before any flash.
- [ ] After approval only: install on FM-1 and test audio, controls, display, MIDI, sequencer, save/load, reboot and long-run stability.

## Branch and safety status

- Coordination branch HEAD at checkpoint start: e6877d47f6b5684efab1e2cf759ed37b2ea76cd0.
- DSP PR #1: draft/open, unmerged.
- Host Integration PR #2: draft/open, unmerged.
- Platform QA remains separate.
- Main unchanged. No PR merged. No device write/flash. Factory firmware untouched.

## Immediate next sequence

1. Review exact DSP PR #1 diff and evidence; do not merge automatically.
2. Close actual host allocator/voice lifecycle matrix.
3. Complete full-engine typed state and per-part memory accounting.
4. Integrate qualified DSP only after review; rerun exact-HEAD full CI on the resulting coordination candidate.
5. Establish target runtime CPU measurements and full-engine regression.
6. Resolve parameter mapping and storage/recovery design before UI/persistence integration.
7. Integrate sequencer, validate package and recovery, then seek separate explicit approval for any physical-device flash.

**Release status: NOT INSTALLABLE.** Passing CI is an important engineering checkpoint, not permission to flash and not proof of hardware operation.
