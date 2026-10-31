# ENERGUN METAL CORE — CP2.21 Accent provenance transport audit

Date: 2026-10-31
Branch: `energun-metal-core`
Status: source audit and proposed metadata encoding; no implementation change.

## Findings from pinned Felucca source

- The sequencer already stores an explicit synth-step Accent in `step_t.flags & SF_ACCENT`; it is not necessary to infer it from velocity.
- Drum-lane Accent is stored separately in `step_t.acc` as one bit per lane.
- `seq_step()` currently converts synth-step Accent into velocity 127 and calls `trk_note_on_source(..., NOTE_SRC_SEQUENCER)`. This loses the distinction between an accented sequencer step and an ordinary performance note with velocity 127.
- The host already tracks note provenance separately from `voice_t`: `track_t.voice_source[NVOICE]` and `track_t.mono_source[8]`. `voice_start()` stores the supplied source byte in `voice_source`; mono/legato fallback carries it through `mono_source`.
- During engine switching, pending note-ons are queued in `xp_note[4]` and `xp_vel[4]`; `xp_note` uses bit 7 for the one-bit source, while velocity is constrained to 0..127.
- The source byte is currently interpreted through `source & 1u` when packed into `xp_note`; preserving this low bit keeps existing source semantics compatible.
- `voice_t` is defined by the pinned Felucca-Engines ABI and should not be enlarged or repurposed.

## Proposed low-risk metadata representation (not yet implemented)

Use the existing source byte as note metadata without changing `voice_t` or adding bytes to `track_t`:

- bit 0: `NOTE_SRC_PERFORMANCE` / `NOTE_SRC_SEQUENCER` (existing meaning)
- bit 1: explicit Accent flag (`NOTE_META_ACCENT`)
- bits 2..7: reserved, zero for current callers

Propagation proposal:

1. `seq_step()` constructs a parallel per-note Accent bit from `SF_ACCENT` for synth notes and `step_t.acc` for lane hits if they are routed through a pitched engine; each call passes source | Accent metadata.
2. `trk_note_on_source()` keeps the complete metadata byte in `mono_source[]` and `voice_source[]`; existing low-bit source logic continues to use `source & 1u`.
3. While an engine switch is pending, preserve Accent in the unused high bit of `xp_vel[]` (velocity is 0..127), while `xp_note[7]` continues to store only the source bit. On replay, reconstruct source | Accent and mask velocity back to 7 bits.
4. On mono/legato fallback, `mono_source_for()` already returns the stored metadata byte; Accent follows the selected held note rather than the last physical key pressed.
5. METAL CORE reads Accent from `track_t.voice_source[voice_index] & NOTE_META_ACCENT` in `note_on()` and latches it into the per-voice VCA trigger. Do not infer Accent from `voice_t.vel`.
6. Ordinary MIDI/performance callers continue passing `NOTE_SRC_PERFORMANCE` with Accent bit clear, even when velocity is 127.

## Risks and required tests before accepting the representation

- Engine-switch queue: Accent survives pending note-on, Note Off removal, and replay; velocity remains 0..127.
- Poly: each note has its own Accent bit; no cross-voice leakage.
- Mono/legato/unison: Accent follows the note selected by priority/fallback and survives same-pitch source ownership updates.
- Retrigger: new note's Accent replaces the previous note's metadata without corrupting the VCA's cancel-and-hold level behavior.
- Sequencer: SF_ACCENT is explicit; unaccented velocity 127 from live MIDI remains unaccented; ratchets repeat the step's Accent; ties/slide preserve the intended held-note metadata.
- Preview/audition and recording playback use the same path as normal sequencer steps.
- Arpeggiator and all non-sequencer callers remain unaccented unless an explicit product rule is later approved.
- Drum lane `acc` semantics must not accidentally apply to ordinary synth-note list entries.
- Confirm `sizeof(track_t)` and 16-byte alignment remain unchanged; no Flash/project/preset file format changes.
- Run full host/web regression and target-budget checks; do not relax any thresholds.

## Decision boundary

This is an audit/proposal, not a code change or user-approved implementation. Do not assume the encoding is accepted until the first patch and tests prove every propagation path. No engine callback, render path, engine registration, parameter/UI/storage/Flash/package/device state changed. No device flashing; `main` and factory firmware untouched.