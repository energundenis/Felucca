# ENERGUN METAL CORE — DSP stage 5: part-state and oscillator lifecycle audit

Date: 2026-10-31
Branch: `metal-core-dsp-stage1-osc`
Status: source audit complete; implementation plan only. No engine registration or memory allocation changed.

## 1. v98 reference behavior

The preserved v98 HTML:
- creates `wcOsc`, `wcOsc2`, `wcMod`, and the 3 ms feedback DelayNode once in `wcInit()`;
- starts the oscillators once;
- leaves them running continuously;
- changes oscillator frequencies from `wcTrig()` but does not restart oscillator phases or clear the feedback delay;
- does not clear feedback history on a sequencer note trigger.

This means the reference oscillator phases and feedback delay continue through note retriggers and silent gaps.

## 2. Felucca host lifecycle (pinned Felucca-Engines interface)

Source paths:
- `firmware/src/voice.c`
- `firmware/src/engines.c`
- pinned submodule `firmware/engines/src/felucca_engine.h` at `d0260074ce100a47675814b7f38d61f11176c926`

Observed host behavior:
- `track_render()` calls `engine_t.block` once per part per audio block, before iterating voices. It calls the hook even when no voice is active.
- `track_render()` calls `engine_t.post` after voice rendering, also when no voice is active.
- `voice_start()` calls `note_on`. On a retrigger of a still-sounding voice, the host restores `ph[0..2]` after `note_on`; once the voice has fully ended and is inactive, a new start does not restore the old phase.
- `engine_t.keep` controls which `s[]` state slots survive a sounding-voice retrigger; it does not preserve voice phases across a fully released/inactive voice.
- `engine_t.mem` is a per-part state region. `eng_mem_claim()` zeroes it when another memory-using engine takes ownership; switching back to the same engine retains its region. The region is zero-initialized at boot.

## 3. Consequence for METAL CORE

The current stage-4 DSP function is a host-testable arithmetic prototype. It stores oscillator phases in `voice_t.ph[0..2]` and delay history in a per-voice test state. That is **not yet the final v98-compatible lifecycle**: after a voice becomes inactive, its phases would not continue advancing.

V1 is monophonic, so the first integrated implementation should use one **per-part** state object in `engine_t.mem`:
- OSC1 phase;
- OSC2 phase;
- wcMod phase;
- 3 ms wcMod feedback ring and write index;
- smoothed feedback control;
- last base oscillator frequency / increment and other small persistent state required to advance during silence.

Use `engine_t.block` to advance the free-running oscillator and feedback state during blocks with no active voice. While a voice is active, the render path advances that same part state and emits audio; the block hook must not advance it a second time. The state is not cleared on note-on or legato changes. It is cleared only when the engine's part-state region is initialized or reassigned by the host.

The exact transition between active render and silent block advancement needs a dedicated test to prove there is neither a skipped block nor a double-advance.

## 4. Memory implications

The existing `ENG_MEM_PART` budget is 12,888 bytes (13,344 with ADPCM enabled).

For the approved monophonic V1:
- 44.1 kHz 3 ms ring: 133 int32 samples = 532 bytes;
- 96 kHz 3 ms ring: 288 int32 samples = 1,152 bytes.

A single per-part ring is therefore substantially smaller than the earlier generic eight-voice estimate. Do not reserve an eight-voice delay bank for the monophonic V1 unless the voice-mode scope changes.

The integrated state structure must use `ENG_MEM_FITS` and `engine_t.mem = sizeof(state)`; its complete size, including oscillator/filter scratch and alignment, must be checked against the per-part budget at both supported build rates.

## 5. Additional source discrepancy: FOLLOW / MOD RATIO

In v98:
- `wcInit()` and the MOD RATIO control set `wcMod.frequency` from `P.wcFreq * P.wcModRatio`;
- the FOLLOW branch in `wcTrig()` sets `wcMod.frequency` to `f * 1.5`.

This hard-coded 1.5 ratio overrides the panel MOD RATIO on a FOLLOW-triggered note. It is recorded as a reference-source behavior requiring an explicit V1 parameter-mapping decision; do not silently choose one interpretation during engine integration.

The feedback gain itself is scaled from panel `P.wcFreq`, not FOLLOW's tracked note frequency. Stage 4 now passes that reference separately.

## 6. Next implementation gate

Before engine registration:
1. define the complete per-part state structure and prove its size;
2. split the renderer into active-audio and silent-state-advance paths that share one oscillator/feedback state machine;
3. add tests for retrigger persistence, release-to-idle continuation, silent-to-active transition, legato pitch changes, and no double-advance at the block boundary;
4. resolve the v98 FOLLOW / MOD RATIO discrepancy in the parameter semantics decision;
5. only then connect the state to `engine_t.mem` and proceed to engine registration/UI.

No engine IDs, parameter IDs, persistence sectors, firmware image build, or hardware writes are authorized by this audit. Factory firmware V1.5 remains untouched.
