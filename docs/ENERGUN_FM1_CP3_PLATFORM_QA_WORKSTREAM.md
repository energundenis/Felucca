# ENERGUN FM-1 CORE — CP3 platform and QA workstream

Base: 77c704bd8ef58e2fa03070fd4fefe3bae59e6d3c
Status: launched; build/resource qualification only.

## Objective
Establish repeatable AC79 target evidence and a trustworthy resource budget before production integration.

## Work
1. Re-run the pinned JieLi pi32v2 compiler probes for VCA and feedback-delay arithmetic.
2. Inspect disassembly for helper calls, wide multiplies, saturation paths and bounded loops.
3. Keep compile/code-generation evidence distinct from cycle-accurate runtime timing.
4. Complete memory accounting for 44.117647 kHz FM-1 target; include coefficient caches, smoothing, oscillator/fold scratch, delay storage and alignment.
5. Assert the complete part-state size against both ADPCM-off and ADPCM-on ENG_MEM_PART configurations.
6. Archive firmware build, map/disassembly, package hashes and regression evidence.
7. Audit installer/recovery source paths; do not flash the physical device.

## Acceptance gates
- Exact-HEAD AC79 compile and full baseline regression pass.
- No target CPU threshold increase to hide new cost.
- Complete state fits with explicit headroom; no unknown state counted as zero.
- Package/recovery remain unapproved until source and backup/restore evidence is complete.

Do not modify factory firmware, device state or main branch.
