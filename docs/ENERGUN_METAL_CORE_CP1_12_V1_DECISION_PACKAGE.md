# ENERGUN METAL CORE — CP1.12 V1 decision package

Date: 2026-10-31
Branch: energun-metal-core
Status: engineering recommendation for user review; **not approved and not implemented**.

## 1. Decisions already established by source audit or explicit user approval

- V1 inventory: 37 independent sound controls, including separate FOLLOW and METAL switches. The count is source-backed by CP1.3.
- LPG MODULE VOLUME: user-approved Option C. Per-trigger peak is `(0.25 + RESPONSE × 0.75) × MODULE VOLUME`; one LPG gain stage, RESPONSE attack and DECAY tail retained.
- FM CHARACTER canonical default: METALLIC (2).
- WEST COAST VOLUME reference steady-state scale: ×5, with one deterministic writer.
- FOLLOW and METAL controls belong in OSCILLATORS / SYNTH VOICE and retain their values while OFF.
- RING2, custom LFO, RANDOM and SLEW remain proposed deferred features for initial V1; this is a scope recommendation pending explicit confirmation.
- Host P_LEVEL and P_DETUNE retain their Felucca meanings and are not aliases for internal Metal Core controls.
- No flash address is approved. A full user FM-1 V1.5 dump must be audited before any candidate sector is selected.

## 2. Recommended V1 control-access allocation

Retain all 37 controls and use the following three-tier proposal:

- 8 engine edit values E0..E7: FREQ, FM INDEX, METAL RATIO, WEST COAST DRIVE, FOLD, CUTOFF, RESONANCE, LPG DECAY.
- 16 common P_MC values: OSC2 LEVEL, OSC2 DETUNE, TIMBRE, MOD RATIO, AM DEPTH, FM FEEDBACK, FM ENV AMT, FM ENV ATTACK, FM ENV DECAY, METAL DECAY, FOLD SYMMETRY, WAVEFOLDER DRIVE, LPG RESPONSE, VOICE VCA LEVEL, WAVEFOLDER MODULE VOLUME, WEST COAST VOLUME.
- 13 private sidecar values: WAVE, FOLLOW, METAL, MODULATION mode, MOD OSC WAVE, FM CHARACTER, COMPLEX MODULE VOLUME, FOLD MODE, LPG MODULE VOLUME, ATTACK, DECAY, SUSTAIN, RELEASE.

This allocation uses the full available 16 common-ID slots below E0 (P_COUNT 111 → 127 in the current audited layout). It leaves no spare common IDs in the current 7-bit parameter-ID space. Therefore it must not be implemented until project/preset migration, motion whitelist, editor pages and backup/restore tests are in place. Discrete switches/enums remain outside continuous motion locks by default.

## 3. VCA and host lifecycle — explicit approval required

Source-backed constraints:
- Felucca host owns voice allocation and note lifetime.
- Default `ownenv=0` keeps host ADSR/velocity behavior.
- v98 has a separate internal triggered VCA contour, plus separate LPG and Complex FM envelopes and METAL trigger behavior.
- Host ADSR and v98 VCA curves/retrigger/legato behavior are not proven equivalent.

Recommendation for the first V1 implementation:
- Keep `ownenv=0`; do not transfer voice lifetime or voice-ending responsibility to Metal Core.
- Preserve the internal V1 VCA peak and normal ADSR controls as independent sound-shaping state, alongside the host voice lifecycle envelope.
- Keep LPG, Complex FM-index and METAL decay envelopes independent.
- Treat the resulting host × internal amplitude contours as an intentional layered architecture, not as exact v98 parity; verify with deterministic reference renders and listening before integration.
- Do not reuse P_ATK/P_DEC/P_SUS/P_REL as aliases, and do not remove the internal VCA controls without a separate user decision.

This is a recommendation, not a frozen decision. If layered amplitude shaping is not desired, stop before VCA DSP implementation and compare a host-envelope-only alternative.

## 4. Recommended initial target and memory envelope

- Start target validation at the firmware's default 44.1 kHz and 32-sample CTL block.
- Keep 48/96 kHz support unclaimed until pitch tables, filter behavior, feedback memory and target CPU are measured at those rates.
- Do not claim 192 kHz support for V1 with the current 3 ms per-voice feedback-ring proposal.
- At 44.1 kHz, 3 ms feedback uses 133 int32 samples × 8 voice slots = 4,256 bytes per part. A provisional 8 × 64-byte voice-state array adds 512 bytes: subtotal 4,768 bytes of the default 12,888-byte ENG_MEM_PART, leaving 8,120 bytes before alignment/other state.
- At 96 kHz, the feedback rings alone use 9,216 bytes per part; with 512 bytes of state, 3,160 bytes remain. This is a tighter conditional target, not a validated budget.
- Final struct sizes, memory assertions and CPU cost must be measured with the target compiler after a real render function exists. Host tests cannot close this gate.

## 5. Sidecar and recovery — design boundary

Recommended architecture, with no physical address assigned:
- A versioned per-project/per-preset extension record stores the 13 private controls for each track/part.
- Record identity must include project/preset association and format version; integrity and commit validity must be checked before loading.
- Use two-generation/A-B transactional writes so an interrupted update leaves one valid prior generation.
- Define deterministic defaults for old projects, missing/corrupt records and unknown future versions.
- Extend archive manifest and backup/restore so the sidecar is always backed up and restored with its owning project/preset.
- Add migration, CRC, generation mismatch, interrupted-write, corrupt-record and restore round-trip tests before any storage implementation.
- The actual XDATA sector and loader compatibility remain blocked on the user's full FM-1 V1.5 dump. No flash write is authorized.

## 6. Remaining CP1 gates

1. User approves or changes the 37-control V1 scope and deferred list.
2. User approves or changes the 8/16/13 access allocation.
3. User approves or changes the layered VCA recommendation in Section 3.
4. Signal map and gain staging are frozen against the approved choices.
5. Sidecar byte format, project/preset association and backup/recovery protocol are specified and tested in a non-device environment.
6. Full device dump is audited before selecting any flash sector.
7. Append-only engine ID behavior is verified across optional SLICE/ADPCM build configurations.
8. Target memory/CPU limits are confirmed after a render implementation exists; until then, the estimates above are planning bounds, not measured proof.

## 7. Safety and implementation status

This document records recommendations only. It does not assign parameter IDs, change engine ABI, implement UI, allocate Flash, modify the firmware render path, create a package, or change physical device state. CP1 remains open until the required user approvals and evidence are recorded. `main` remains untouched.
