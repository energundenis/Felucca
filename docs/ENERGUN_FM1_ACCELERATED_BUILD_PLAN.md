# ENERGUN FM-1 CORE — accelerated build execution plan

Date: 2026-11-01
Branch: `energun-metal-core`
Status: user-approved execution approach; schedule is a target, not a release guarantee.

## Objective

Shorten the path to a safely installable FM-1 test candidate by running independent workstreams in parallel and integrating proven components incrementally. Do not reduce safety, numerical, memory, lifecycle, or recovery gates to meet a date.

## Target milestones

| Milestone | Target from active implementation | Exit condition |
|---|---:|---|
| EARLY BUILD | 5–8 working days | Firmware build succeeds with an isolated Metal Core integration scaffold; no device write required |
| TEST BUILD | 8–12 working days | Core DSP components integrated behind explicit feature boundaries; host regression, target compile, memory and lifecycle checks pass |
| INSTALLABLE CANDIDATE | 12–18 working days | Full regression and package validation pass; installation and factory-recovery procedure is source-verified and documented |

These are planning targets, not guaranteed calendar dates. Hardware access, unknown boot/update constraints, AC79 resource limits, or defects may move them.

## Parallel workstreams

1. **Firmware integration scaffold** — inspect engine registration, callbacks, per-part memory ownership, parameter/preset boundaries and build configuration; prepare a non-registered or disabled-by-default integration path. Do not change the host ABI.
2. **DSP qualification** — continue CP3.1 fixed-point filter and feedback candidates; inspect target code generation, arithmetic helpers, block cost, numeric headroom and automation behavior.
3. **Platform/resource qualification** — confirm AC79 target build, I2S clock, memory-pool limits, linker/map output and per-part state size.
4. **Lifecycle and control integration** — test fresh note, sounding retrigger, released voice reuse, mono legato, voice steal, MIDI Gate-off, sequencer Gate-off, engine switch and completion/done semantics.
5. **Automated QA and release safety** — run focused tests per change and full regression at integration checkpoints; archive binaries/maps/disassembly; verify package format, boot path, backup and recovery before any device installation.

Independent work proceeds in parallel. A failed gate blocks only the affected integration or release candidate, not unrelated analysis and scaffold work.

## Incremental build policy

- Produce an EARLY BUILD as soon as the scaffold compiles; it is an engineering artifact, not an installable release.
- Keep unqualified DSP behind compile-time or runtime-disabled boundaries; do not silently substitute untested algorithms.
- Promote components only with host regression and pinned AC79 compiler evidence.
- Use fast regression after narrow changes; run full baseline regression at integration checkpoints and before release.
- Keep each iteration on a feature branch. Do not modify `main`, merge draft work, overwrite the protected factory firmware, or flash the device without explicit approval.
- Do not label a build installable until package validation and a safe, tested recovery route are established.

## Current source-backed constraints

- The engine registry in `firmware/src/engines.c` does not yet register METAL CORE.
- The isolated VCA model exists in `firmware/src/eng_metal_core_vca.h`; it is not connected to production render callbacks.
- CP2.19 is the approved VCA behavior: full internal envelope for live MIDI and sequencer, with `ownenv=1`; MIDI Note Off is currently block-boundary, not sample-accurate.
- CP2.20 Gate-off is an isolated helper/model, not proof of host event dispatch integration.
- CP3.1 filter and feedback prototypes are not production DSP. Final per-part memory and callback cost remain open.
- No physical FM-1 write or factory firmware modification is authorized by this plan.

## Documentation precedence

The older `ENERGUN_METAL_CORE_ADSR_COMPATIBILITY_AUDIT_01.md` contains a provisional V1 recommendation of `ownenv=0`. That recommendation is superseded by the later user-approved CP2.19 decision, which requires `ownenv=1` and a dedicated internal VCA contour. The older audit must be marked superseded before engine integration; do not implement its provisional recommendation.

## Reporting

For each workstream, report status as not started / in progress / checking / passed / blocked, with completed work, evidence, and remaining gates. Report only observed CI and build results. No claim of background engineering between active sessions.


## Execution kickoff — 2026-11-01

User approved the accelerated execution model. The following isolated work branches have been created from the same audited base, `77c704bd8ef58e2fa03070fd4fefe3bae59e6d3c`:

- `energun-fm1-cp3-dsp-filter` — host-only fixed-point filter prototype and numerical qualification. No production DSP promotion until reference-vector and stability tests pass.
- `energun-fm1-cp3-host-integration` — real Felucca callback/lifecycle adapter tests, including retrigger, fresh-slot reuse, Gate-off and voice retirement. No engine registration in this workstream.
- `energun-fm1-cp3-platform-qa` — AC79 compiler/code-generation inspection, state/memory accounting, build evidence and release-safety audit.

The integration branch `energun-metal-core` remains the coordination branch. No branch is merged automatically. Each workstream must return reviewable commits and test evidence before integration. The three branch refs start at the same base; work is not considered implemented merely because a branch exists.

The latest exact coordination HEAD at kickoff is `77c704bd8ef58e2fa03070fd4fefe3bae59e6d3c`. The last observed full baseline build/regression run was successful on `daae06e84c5710e0ed5703c5af38afde927b1ed8` (run 41254106977); it predates the kickoff HEAD and therefore is not evidence for the exact kickoff SHA. A fresh exact-HEAD CI run is required.


## Existing DSP lane reused — 2026-11-01

A pre-existing draft PR was found during kickoff and is the authoritative CP3.1 DSP/filter lane; avoid duplicating its implementation:

- PR [#1 — isolated Q2.30 DF2T biquad prototype](https://github.com/energundenis/Felucca/pull/1), head `65533d9780dedf81524a43a7fd669e286d82343e`.
- Focused host regression run [41255766930](https://github.com/energundenis/Felucca/actions/runs/41255766930): success.
- AC79 pi32v2 compile-only code-generation run [41255766916](https://github.com/energundenis/Felucca/actions/runs/41255766916): success.
- The PR reports a host double-precision oracle, 28 coefficient-stability cases, 28 impulse cases, 84 sine comparisons, independent filter histories, and explicit over-range saturation checks.
- This is still a draft candidate: the PR's local evidence and CI success do not authorize production DSP integration or prove runtime AC79 cycle cost. Review numeric error, signal headroom, target disassembly and the source-derived filter automation before promotion.

The newly created `energun-fm1-cp3-dsp-filter` branch is therefore redundant and remains an unused placeholder; use PR #1 as the active DSP lane unless review identifies a specific missing task.

## Host lane kickoff — 2026-11-01

- Draft PR [#2 — CP3 host lifecycle workstream kickoff](https://github.com/energundenis/Felucca/pull/2) records the host-integration scope.
- It currently adds documentation only. The real-track adapter source already exists on the base branch, but an exact-HEAD CI run for that adapter has not yet been observed. PR #2 has not produced an observed workflow run at this audit.
- Next code increment is the fresh-slot / retrigger / released-slot / mono-legato / voice-steal lifecycle matrix, still test-only.

## Platform lane kickoff — 2026-11-01

- Branch `energun-fm1-cp3-platform-qa` records the AC79/resource/recovery workstream scope.
- No new target benchmark or complete engine-memory proof has been claimed. The 112-byte per-voice / 896-byte per-part accounting and 3 ms feedback ring remain provisional candidate figures; complete coefficients, smoothing and scratch state are not yet budgeted.


## CI checkpoint — 2026-11-01

- Full baseline workflow: https://github.com/energundenis/Felucca/actions/runs/41322047870 — SUCCESS; all 16 principal steps completed.
- Exact tested source: 542126c0498f177f38f6f14525ac3042806cfdb7 on DSP work branch energun/cp3-1-biquad-prototype-01 (PR #1 head), not the coordination branch.
- Unmodified Felucca firmware build: PASS.
- Isolated ENERGUN VCA compile with pinned AC79 pi32v2 target compiler: PASS.
- Felucca Host Regression and Web Regression: PASS.
- Build output/size recording and experimental evidence upload: PASS.
- Companion fast regression: https://github.com/energundenis/Felucca/actions/runs/41322047771 — SUCCESS on the same SHA.
- Biquad candidate host regression + AC79 compile/disassembly probe: https://github.com/energundenis/Felucca/actions/runs/41322047859 — SUCCESS on the same SHA.
- AC79 biquad inspection is compile/code-generation evidence only, not runtime cycle timing or full-engine CPU proof.
- This verifies the tested DSP candidate commit only. It does not mean PR #1 is merged, METAL CORE is production-registered/playable, or the coordination branch has the same tested tree.
- Coordination branch HEAD at checkpoint start: e6877d47f6b5684efab1e2cf759ed37b2ea76cd0. Main remains untouched.
- No device write or firmware flash occurred; factory firmware remains unchanged.

Acceleration is operating as parallel CI and verification, not as an autonomous coding agent. No development occurs between active sessions. Next critical work: review DSP PR #1, close the host lifecycle matrix, then complete full-engine memory and runtime CPU budgets before registration or release work.
