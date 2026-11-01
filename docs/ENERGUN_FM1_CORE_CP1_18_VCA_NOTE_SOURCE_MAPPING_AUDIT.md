# ENERGUN FM-1 CORE — CP1.18 VCA note-source and Gate mapping audit

Date: 2026-11-01
Branch: `energun-metal-core`
Status: source-backed reconciliation; no firmware source changed.

## 1. Decision already approved

CP2.19 is a user-approved product behavior decision:

- Every live/manual performance note and every integrated sequencer note uses the same internal METAL CORE VCA Attack → Decay → Sustain contour.
- Live MIDI Gate is controlled by MIDI key state; sequencer Gate is controlled by the sequencer step.
- Retrigger holds the instantaneous level and starts a new contour.
- Host ADSR is bypassed (`ownenv=1`); LFO-to-AMP and engine-switch fade remain separate host multipliers.
- Gate-off is applied at the next render-block boundary under the current host API; no sample-accurate sub-block timing is claimed.

This deliberately overrides the V98 manual-mode constant LEVEL behavior. The V98 contour and timing rules remain the source for the selected full-envelope behavior.

## 2. Source trace

### Live/manual performance notes

- `voice.c` has a backward-compatible `trk_note_on()` wrapper that calls `trk_note_on_source(..., NOTE_SRC_PERFORMANCE)`.
- `trk_note_on_source()` routes the note through the shared voice allocator and eventually `voice_start()`.
- `voice_start()` writes the supplied provenance into `track_t.voice_source[voice_index]` before calling the selected engine's `note_on()` callback.
- `trk_note_off()` clears `voice_t.gate` for the matching active voice.
- In `track_render()`, an `ownenv=1` engine bypasses host ADSR shaping and calls its own `done()` lifecycle hook. The host still applies the independent LFO-to-AMP multiplier and engine-switch fade.

### Sequencer notes

- `seq.c` calls `trk_note_on_source()` with `NOTE_SRC_SEQUENCER` and adds `NOTE_META_ACCENT` only when the sequencer step's Accent flag is set.
- Sequencer note-off is represented through the shared track/voice Gate lifecycle; the engine observes Gate state at render-block entry.
- The host preserves source and Accent metadata when a note is queued during an engine-switch fade, then restores that metadata when replaying the queued note after the switch.

### VCA adapter evidence

- The isolated VCA render harness uses `ownenv=1`, per-voice VCA state, Gate-off at render-block entry, retrigger hold, and `done()` after the internal envelope reaches idle.
- The CP3.0 real-track adapter compiles against actual Felucca `track_t`, `voice_t`, and `vmod_t` structures and reads explicit Accent metadata from `track_t.voice_source[]`.
- These are test-only descriptors. Neither is registered in `engines.c`; neither exercises the complete production note dispatcher plus sequencer runtime with METAL CORE.

## 3. Accent semantics

The source has an explicit sequencer Accent bit. It is not equivalent to expressive MIDI velocity.

- Sequencer Accent: source-backed and carried in `NOTE_META_ACCENT`.
- Live MIDI Accent: not defined by the approved CP2.19 decision. Do not infer it from velocity 127 or any other velocity threshold.
- The VCA target function supports an explicit Accent input, but the production mapping must remain source-aware.
- Until a separate product decision is made, the safe V1 behavior is: apply Accent only when the explicit sequencer Accent metadata is present; performance notes use no Accent unless an explicit performance control/message is later approved.

This last bullet is an implementation recommendation for the current unresolved mapping, not a new user-approved control behavior.

## 4. Timing and lifecycle limits

- MIDI Note Off and sequencer Gate-off currently become a changed `voice_t.gate` state observed by the engine at the next render-block entry.
- The current engine callback ABI has no dedicated note-off callback or sub-block event offset.
- The first prototype must document the block-boundary latency and test both MIDI and sequencer release paths at that boundary.
- Do not claim sample-accurate Gate timing.
- `done()` must remain false throughout Release and become true only after the internal VCA reaches its silence criterion.
- Voice stealing remains a separate host-owned lifecycle case; the existing shared-voice steal fade must not be mistaken for the normal VCA Release.

## 5. Remaining implementation tests

Before CP1/CP3 can close this mapping:
1. Exercise actual live MIDI dispatch through `trk_note_on_source()` and `trk_note_off()` with the test descriptor.
2. Exercise actual sequencer note/Accent/Gate dispatch through `seq.c` with the test descriptor.
3. Verify that Accent is present only for explicit sequencer Accent and survives engine-switch queuing.
4. Verify retrigger behavior for POLY, MONO, LEGATO and UNISON; distinguish a true retrigger from a legato pitch change that intentionally does not call `note_on()`.
5. Verify voice-steal fade and `done()` retirement against the host allocator, not only the isolated descriptor.
6. Keep the render-block Gate boundary visible in the test names and acceptance report.

## 6. Status and safety

- CP2.19 full-envelope behavior is accepted.
- The live-versus-sequencer envelope shape question is closed.
- Gate source mapping is source-traced; complete production-path regression remains open.
- Sequencer Accent provenance exists; live-MIDI Accent semantics remain open.
- No parameter/engine IDs, firmware source, registry, UI, persistence, Flash map, package, factory firmware or physical device changed.
- `main` remains untouched; no PR was merged and no device write was performed.
