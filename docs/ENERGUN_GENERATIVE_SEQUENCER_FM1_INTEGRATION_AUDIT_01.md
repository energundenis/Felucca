# ENERGUN GENERATIVE SEQUENCER — FM-1 integration audit 01

Date: 2026-10-31
Repository: energundenis/Felucca
Branch: energun-metal-core
Audited branch head: f9c1178ff80d70e607f9f81f9220ee60ce35e13e
Status: documentation-only audit; no firmware, SDK, archive, factory image, or device changed.

## 1. Approved generator reference

The accepted host prototype is ENERGUN GENERATIVE SEQUENCER V0.3. It is the reference behavior for integration:
- new 8 / 16 / 32-step sequences;
- ROOT NOTE and SCALE;
- rhythm density and syncopation;
- pitch range, octave minimum/maximum and octave bias;
- accent amount and gate length;
- independent RHYTHM / NOTES / ACCENTS / GATES generation;
- GENERATE ALL, EVOLVE, UNDO, PREVIEW and MIDI export.

The generator owns sequence data only. It must not alter the FM sound engine or Metal Core DSP.

## 2. Existing FM-1 sequencer model — source confirmed

From `firmware/src/core.h`, `seq.c`, `params.c`, `project.c`:
- Four independent tracks / synth parts (`NTRK=4`).
- 64 steps per track (`NSTEP=64`).
- Each step stores up to four MIDI notes, NOTE/TIE/REST, accent/slide/ratchet flags, velocity, drum hits/accents, chance/condition.
- Pattern length is `P_SLEN`, range 1..64, default 16.
- Root is `P_ROOT`, 0..11; scale is `P_SCALE`.
- The device already supports MIDI note numbers 0..127 in step data.
- The existing project format FUN10 stores the current step data and pattern parameters. A basic generator that only writes existing step fields and P_SLEN does not inherently require a new project format.

V0.3's 8 / 16 / 32 lengths fit directly.

## 3. Direct field mapping

| V0.3 feature | FM-1 representation |
|---|---|
| LENGTH | `P_SLEN` |
| ROOT NOTE | `P_ROOT` |
| SCALE | `P_SCALE` |
| Rhythm ON/OFF | `step_t.time` = NOTE / REST; `n` = 1 / 0 |
| Generated pitch + octave | `step_t.note[0]` as absolute MIDI note |
| ACCENT | `SF_ACCENT` |
| Density / syncopation / mode | Generator algorithm; no new playback engine needed |
| EVOLVE | Rewrites selected existing step fields |
| UNDO | Existing pattern undo snapshot can hold all 64 `step_t` records |

The device has more scale choices than V0.3; map the six V0.3 scales to the matching existing values (MIN, PHRY, HARM, DOR, MAJ, CHR). The generated absolute notes are already in the chosen scale. If the track uses `P_QUANT=SEQ`, the device will quantize them again using its current `P_ROOT/P_SCALE`; generation must therefore synchronize the chosen root/scale with those track settings or explicitly account for the existing quantizer. Do not silently change unrelated sound parameters.

## 4. GATE — important compatibility finding

V0.3 has an independent gate percentage per step. FM-1's `P_SGATE` is currently a track-level value (range 1..127), and `seq_step()` computes note duration from that value. There is no per-step gate byte in `step_t`.

However, the existing motion system already supports per-step Parameter Locks:
- locks are applied in `seq_play()` before `seq_step()` triggers the note;
- a lock remains for the full step and its ratchets;
- the next step restores the underlying automation/base value;
- the project already stores these motion events.

Current `motion_param()` deliberately excludes `P_SGATE`. A narrowly scoped sequencer-only extension could allow `P_SGATE` locks, then represent each generated step's gate as a lock on `P_SGATE`. This would avoid adding a field to every step or changing the project format. It is a proposal only, not implemented or approved.

Capacity constraint: the motion event pool is 128 records shared across all four tracks and other automation. A 32-step generated gate layer can consume up to 32 records on one track. Generation must count available capacity, preserve unrelated events, and fail atomically rather than leave a partial pattern. The 4-track worst case can exhaust all 128 records before any other automation.

## 5. Existing host-to-device route

The firmware already exposes a USB-MIDI SysEx editor protocol:
- framing: `F0 7D 46 4C <cmd> ... F7`;
- `INFO` (1) advertises firmware capabilities;
- `TRACK_STEP` (30) reads/writes a step on any track;
- `TRACK_PARAM` (31) reads/writes a track parameter;
- `MOTION` (64) has Parameter Lock operations (5..7).

This makes a host-side integration prototype possible without first adding a native generator UI: a computer-side generator can discover the connected device, read the selected track, generate V0.3-compatible data, then write the step pattern through the existing protocol. The P_SGATE lock whitelist issue remains for per-step gate. This route is a test/integration path, not a claim that a generator frontend already exists in this repository.

## 6. Native firmware execution boundary

The sequencer playback path in `seq.c` runs in the audio ISR. The generator must never run there.

The main loop calls `ui_input()` at approximately 60 UI frames per second; editor SysEx service also runs in the main loop. Generation of at most 32 steps is small enough to plan for main-loop execution. Build a complete candidate pattern in a temporary buffer, validate it, then publish it under the firmware's existing interrupt-guard discipline so the audio ISR cannot observe a half-written pattern.

Use existing undo and pattern signatures where appropriate. Do not write Flash during generation; the normal project save path remains separate and is only used when the user explicitly saves.

## 7. UI and persistence constraints

The native panel has a small fixed control surface (four knobs, key grid and buttons) and already has SEQ > STEP, DETAIL and AUTOMATION pages. V0.3 exposes more controls than can be presented simultaneously. A native UI layout must be deliberately designed around a small number of pages/actions; do not simply transplant the browser layout or add a large collection of new permanent controls.

Generator-only settings (pitch range, octave bounds/bias, density, syncopation and accent amount) do not all have existing track parameter IDs. The first embedded version should treat them as transient generator settings unless a separate persistence design is explicitly approved. Do not repurpose sound-engine parameters to store them.

MIDI file export is naturally a host-side feature. Device-side sequence generation does not require adding a MIDI file writer to the firmware.

## 8. Recommended staged approach

1. Keep the accepted V0.3 HTML and its algorithm unchanged as the behavioral reference.
2. Build a computer-side SysEx integration harness using INFO / TRACK / TRACK_STEP / TRACK_PARAM; first verify pitch, rhythm, accents and length against a test track.
3. Resolve GATE with a separately tested, narrowly scoped P_SGATE Parameter Lock extension, including capacity checks and tests for lock-before-note-on and restore-next-step.
4. Only after the host-side mapping is proven, port the same algorithm to C for optional native generation from the FM-1 panel.
5. Add host tests and target timing/memory checks before any firmware package is considered.
6. Keep all work on the development branch and separate from factory V1.5.

## 9. Safety boundary

This audit changes documentation only. No firmware source, SDK/submodule, build toolchain, archive, Flash layout, project format, factory V1.5 dump/image, updater, or physical FM-1 was modified or written. No build or flash operation was run.
