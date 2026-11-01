# ENERGUN FM-1 CORE — project checkpoint

Date: 2026-10-31
Branch: `energun-metal-core`
Status: checkpoint / roadmap baseline

## Project boundary

ENERGUN FM-1 CORE is the firmware project for the physical M-VAVE FM-1. It is separate from ENERGUN SYNTH LAB, which contains browser-based synth projects. Do not use Synth Lab project documentation as evidence for FM-1 firmware state.

## Frozen project direction

- Firmware foundation: Felucca.
- FM-Mini-1: reference for UI/navigation ideas, not a replacement firmware foundation or audio engine.
- Target custom audio engine: ENERGUN METAL CORE, based on the project's approved METAL CORE V1 architecture.
- Sequencer extension: ENERGUN Generative Sequencer V0.3, accepted as the generator to integrate with the FM-1's existing sequencer.

## METAL CORE V1 scope

The approved architecture covers oscillators, Complex Oscillator/FM, METAL, FOLLOW, Wavefolder, main filter, LPG, and voice-level/envelope behavior.

Architecture and source audits exist for signal flow, Felucca parameter semantics, engine memory, host ADSR/lifecycle, parameter mapping, storage constraints, and sequencer/editor integration.

## Implementation status — distinguish design from code

- Architecture and integration audits: completed as documented.
- A Metal Core source scaffold is present in the branch.
- The scaffold is not yet a complete DSP implementation or a registered, working Felucca engine.
- DSP implementation, engine registration/UI integration, firmware build validation, and physical-device testing remain future work.
- Sequencer V0.3 bridge export and offline dry-run tooling are prepared. This does not mean the generator is already integrated into the device firmware.
- Gate behavior and MCX automation/Parameter Lock remain subject to their documented unresolved decisions and approval gates.

## Safety boundary

- Factory M-VAVE firmware V1.5 remains the working baseline.
- No factory firmware modification, device flashing, erase, or physical-device write is authorized by this checkpoint.
- Do not claim hardware compatibility or successful hardware testing without direct evidence.
- Preserve existing working versions and use separate, versioned changes.

## Next development stage

Continue with the actual METAL CORE DSP implementation in Felucca, starting from the approved signal-flow architecture and existing scaffold. Keep sequencer integration as a separate workstream. Before any hardware write or firmware flash, obtain explicit user approval.

## Project separation rule

Do not bring browser Synth Lab versions such as METAL CORE V105, VOICE 2, MOOG CORE, or K-2 CORE into the FM-1 CORE implementation plan unless the user explicitly requests it and the relevant source is independently verified.


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

The implementation boundary is unchanged: DSP candidate PR #1 remains unmerged; METAL CORE is not thereby registered or playable; physical-device validation, full-engine CPU/RAM, UI, persistence, sequencer, package and recovery remain open. See ENERGUN_FM1_CORE_STATUS_2026-11-01.md for the consolidated plan.
