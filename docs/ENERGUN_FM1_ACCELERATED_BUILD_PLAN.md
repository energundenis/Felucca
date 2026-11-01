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
