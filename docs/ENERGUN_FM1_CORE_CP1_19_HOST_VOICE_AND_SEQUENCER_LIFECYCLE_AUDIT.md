# ENERGUN FM-1 CORE — CP1.19 host voice-mode and sequencer lifecycle audit

Date: 2026-11-01
Branch: `energun-metal-core`
Status: source-backed behavior matrix; no firmware source changed.

## Scope

This audit follows the actual Felucca note-dispatch and sequencer source paths after CP1.18. It distinguishes verified host behavior from the remaining METAL CORE integration tests. The CP3.0 real-track adapter remains a test descriptor, not a registered engine and not a full dispatcher test.

## 1. Voice-mode behavior confirmed in `firmware/src/voice.c`

### POLY
- `trk_note_on_source()` allocates one host voice and calls `voice_start()`.
- `voice_start()` stores note source / Accent metadata in `track_t.voice_source[voice_index]`, then calls engine `note_on()`.
- A new note on a currently sounding allocated voice is a real retrigger; Felucca preserves oscillator/filter state according to the engine's `keep` mask and the engine's own note-on callback decides how its VCA retriggers.
- Voice allocation first uses inactive slots, then prioritizes released non-fading voices, then may steal an older held voice while protecting the lowest held note.

### MONO
- A new note always reaches `mono_play(... retrig=1 ...)`; the active voice is restarted through `voice_start()`.
- This is a true VCA retrigger and should use cancel-and-hold, not reset-to-zero.

### LEGATO
- First note starts the voice.
- A subsequent held note that wins the selected priority changes pitch/source on the active voice without calling `voice_start()` or engine `note_on()`.
- Therefore it is not a VCA retrigger; the current VCA contour continues while pitch glides/changes.
- If a note-off reveals another held note, the host calls `mono_play(... retrig=0 ...)`, preserving the existing contour.

### UNISON
- `mono_play()` operates on the configured number of host voices (up to the engine's voice cap).
- Each voice has its own engine VCA state indexed by host voice index.
- A true retrigger starts each eligible unison voice; legato fallback changes pitch without restarting the contour.
- Memory and state isolation must be checked across every voice actually allocated, including configurations below and at the eight-voice cap.

## 2. Note-off and sequencer Gate mapping confirmed

### Live MIDI
- `trk_note_off()` clears `voice_t.gate` and marks the host voice stage as release.
- In MONO/LEGATO/UNISON it first removes the released note from the mono stack. If another held note wins, the host performs a legato fallback; otherwise it clears Gate on the active voice(s).
- The METAL CORE own-envelope adapter observes Gate low at render-block entry and calls explicit VCA Gate-off. This is block-boundary behavior.

### Sequencer
- `seq_step()` sends notes through `trk_note_on_source()` with `NOTE_SRC_SEQUENCER` and adds `NOTE_META_ACCENT` only from the explicit step Accent flag.
- A slide/tie carries notes between steps; notes not held over are released through `trk_note_off()`.
- Normal step Gate duration is scheduled in `t->seq_off`; ratchets calculate a shorter per-hit Gate and replay the step through `seq_ratchet()`.
- Rests and missed steps call `seq_release()`.
- This is the same host Gate lifecycle as MIDI, but driven by sequencer timing events. The engine observes Gate at render-block entry, so actual VCA release remains quantized to that boundary.

## 3. Important distinctions for the VCA tests

| Case | Engine `note_on()` expected? | VCA behavior to test |
|---|---:|---|
| POLY new note in free slot | Yes | New contour |
| POLY allocator reuses sounding voice | Yes | Retrigger from held level |
| MONO new note | Yes | Retrigger from held level |
| LEGATO held-note pitch change | No | Continue current contour |
| LEGATO fallback after Note Off | No | Continue contour; pitch changes |
| UNISON new note | Yes for each eligible voice | Independent contours per voice |
| Sequencer ratchet | Yes for each retrigger event | Retrigger from held level |
| Sequencer slide/tie | Only for genuinely new/retriggered notes | Preserve contour for held notes |
| Sequencer Gate expiry | No | Gate-off then Release |
| MIDI Note Off | No | Gate-off then Release |
| Engine-switch queued note | On replay after switch | Preserve source/Accent metadata |

## 4. Gaps that remain open

The source trace is not itself a regression test. Required next tests must execute the production functions, not copy their behavior into a hand-written model:

1. A host test translation unit that includes or links the real note dispatcher and verifies the engine callback trace for POLY/MONO/LEGATO/UNISON.
2. Real `trk_note_off()` tests for mono-stack fallback, all-notes-off and released voice state.
3. Sequencer tests that execute the real step/Gate path for normal Gate, ratchet, rest, slide and tie.
4. Accent propagation tests from sequencer step metadata through the actual host voice to the engine callback; verify MIDI velocity alone never asserts Accent.
5. Engine-switch queue test for source and Accent preservation using the real dispatcher.
6. Integration tests for voice stealing: host stage-4 fade must not be mistaken for normal VCA Release, and `done()` must not retire a voice before host fade semantics are satisfied.
7. Test the VCA's one-shot Gate-off observation per voice/block and ensure repeated low-Gate blocks do not restart Release.

## 5. Acceptance boundary

- Source-backed host behavior for the four modes is now documented.
- The CP3.0 adapter proves only real structure layout, per-voice mapping, Accent-bit reading and a synthetic callback path.
- It does not prove production note dispatch, sequencer runtime, actual voice allocation/stealing or full METAL CORE audio.
- No claim of CP1 or CP3 closure is made.
- No production source, engine registry, parameter IDs, UI, persistence, Flash layout, firmware image, factory firmware or physical device was changed.
- `main` remains untouched; no PR merged and no device flashing/writes performed.
