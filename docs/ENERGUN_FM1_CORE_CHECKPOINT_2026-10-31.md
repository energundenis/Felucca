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
