# ENERGUN METAL CORE — DSP stage 6: monophonic per-part state prototype

Date: 2026-10-31
Branch: `metal-core-dsp-stage1-osc`
Status: isolated per-part state machine implemented and host-tested. Not yet wired to `engine_t.mem`, `engine_t.block`, or `engine_t.post`.

## 1. Why a per-part state is required

The v98 Web Audio reference keeps OSC1, OSC2, wcMod, and the 3 ms feedback DelayNode running across note triggers and silent gaps. Felucca's voice phases do not persist after a voice fully releases and becomes inactive. The approved V1 engine is monophonic, so one persistent phase set and one delay ring per part are sufficient.

## 2. New V1 state

File: `firmware/src/eng_metal_core.c`

Added `metal_core_part_state_t` with:
- three continuous phases: OSC1, OSC2, wcMod;
- one 3 ms feedback ring and write position;
- smoothed feedback amount;
- last oscillator increments, panel-FREQ reference, ratio, waveform selections, FM index, drive, OSC2 level, and feedback target;
- a per-block advancement guard.

Calculated structure size:
- 44.1 kHz: 596 bytes (133 int32 delay samples);
- 96 kHz: 1,216 bytes (288 int32 delay samples).

Both fit comfortably inside Felucca's 12,888-byte per-part `ENG_MEM_PART` budget. The earlier NVOICE-array feedback state remains only as a multi-voice regression harness; it is not the V1 allocation.

## 3. Shared active and silent sample state machine

The active renderer and silent advancement both call `metal_core_part_tick()`. There is no separate approximate idle oscillator path.

Intended host callback sequence:
1. `engine_t.block`: `metal_core_part_begin_block()` clears the per-block guard.
2. If the active voice renders, `metal_core_part_render()` advances the state and emits the pre-fold audio, then marks the block consumed.
3. `engine_t.post`: `metal_core_part_advance_silent()` advances the exact same state machine only if the renderer did not run. This also covers the host's silent-sustain optimization where a voice remains active but its render callback is skipped.
4. A second advancement attempt in the same block is ignored.

This callback wiring is a design contract for the next integration step; the current source only contains the tested state-machine primitives.

## 4. Host regression results

Test file: `tests/metal_complex_fm_test.c`

New checks:
- two per-part states produce identical active output and state;
- silent advancement matches the active sample path exactly;
- duplicate advancement in one block is prevented;
- reactivation after silence resumes with identical phase and feedback history;
- legato/retrigger pitch changes advance existing phases and preserve delay position;
- the V1 part-state object is under 2,000 bytes and below the host per-part budget.

GitHub Actions run `41158153324` completed successfully, including the full fixed-point OSC/FM/feedback suite at 44.1 kHz and the Complex FM/per-part lifecycle suite at 96 kHz:
https://github.com/energundenis/Felucca/actions/runs/41158153324

## 5. Not yet done

- No `ENG_MEM_FITS` / `engine_t.mem` allocation has been committed.
- No callbacks have been wired into a registered `engine_t`.
- The active voice render adapter and note-on parameter update path are not implemented.
- No full firmware build, target CPU measurement, device query, SysEx, or hardware test has occurred.
- No engine IDs, parameter IDs, UI pages, or persistence allocation have been changed.
- Factory M-VAVE FM-1 V1.5 remains untouched.

## 6. Next gate

Wire the state into a non-registered engine adapter in a separate testable step:
- allocate exactly `sizeof(metal_core_part_state_t)` through `engine_t.mem`;
- use `block` to reset the block guard and `post` for silent advancement;
- use the voice render callback to configure and render the same state;
- verify the callback ordering against Felucca's real `track_render()`;
- compile the actual `engines.c` translation unit before any engine-table registration.

Only after that should DSP modules beyond Complex FM be layered onto this shared continuous state.
