# ENERGUN METAL CORE — CP2.7 host note-source provenance audit

Date: 2026-10-31
Branch: `energun-metal-core`
Status: source-backed design audit; no firmware source or ABI changed.

## 1. Why provenance is required

The CP2.6 helper deliberately distinguishes sequencer-origin pitch from performance-origin pitch. Felucca's current engine callback does not provide that distinction as an argument. The engine interface is:

```c
void (*note_on)(struct track *t, voice_t *v);
void (*render)(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
```

The host's `voice_t.note` is an absolute MIDI note (0..127), but the engine callback receives only the track and voice pointers. The host note event's origin is not represented in `voice_t` or `vmod_t`.

## 2. Confirmed note ingress paths

Source inspection of `firmware/src/seq.c` and `firmware/src/voice.c` found four direct `trk_note_on()` call sites in seq.c:

| Source location | Meaning | Classification |
|---|---|---|
| `seq.c` `seq_step()` | Pattern step note, including ratchet replays | SEQUENCER |
| `seq.c` `seq_audition()` → `seq_step()` | Stopped-transport step preview | SEQUENCER_PREVIEW (same FOLLOW policy as a step) |
| `seq.c` `arp_fire()` | Arpeggiator note derived from held input | PERFORMANCE |
| `seq.c` `input_on()` | FM-1 keyboard / incoming MIDI note path | PERFORMANCE |

The CHORD arpeggiator branch calls `arp_chord()`, which also produces notes from the held-note list; classify those as PERFORMANCE. The arpeggiator is not the pattern sequencer and must not be disabled by FOLLOW OFF.

The pattern sequencer's ratchets re-enter `seq_step()`, so they naturally retain SEQUENCER classification. Live recording's `input_on()` is a performance event even when it writes the currently playing step; the rskip mechanism prevents that recorded note from sounding a second time as a pattern event.

## 3. Host lifecycle constraints

- `trk_note_on()` may queue up to four note-ons during an engine crossfade in `xp_note[] / xp_vel[]`; those queued notes are replayed after the engine switch. Source must be queued alongside each note or it can be misclassified on replay.
- `voice_start()` calls engine `note_on()` for fresh starts and retriggers. A sounding MONO/LEGATO voice can instead take the legato branch in `mono_play()`: it updates `v->note`, `v->pitch16` and glide without calling engine `note_on()`. Source provenance therefore cannot live only in a transient flag read by note_on.
- MONO/LEGATO maintains `mono_stack[]`; when a note is released, `mono_play()` may fall back to another held note. A mixed-origin stack must retain each held note's source so FOLLOW behavior is restored correctly on fallback.
- POLY voices can overlap sequencer and performance notes. A single track-wide current-source flag is insufficient; the active voice needs its own source.
- `voice_t` is defined by the pinned Felucca-Engines submodule. Do not extend or reorder this shared structure for ENERGUN.

## 4. Recommended host-side representation

Use a small host-owned enum, not a new parameter ID:

- `NOTE_SRC_PERFORMANCE`
- `NOTE_SRC_SEQUENCER`
- `NOTE_SRC_PREVIEW` (or normalize preview to sequencer while retaining it only for diagnostics)

The source must follow the note through the entire lifecycle:

1. Each note ingress passes an explicit source into the internal host note-on path.
2. The four pending engine-switch slots retain the source with note and velocity.
3. Each active voice has a host-owned source byte indexed by its voice slot; update it on both fresh/retrigger and legato pitch changes.
4. The MONO/LEGATO held-note stack retains the source per note so a fallback restores the source of the note that becomes active.
5. Note-off, voice stealing, engine switching, and voice reuse clear or replace source state at the same points as their corresponding note state.

The engine can then read the host-owned per-voice source from `track_t` without changing `voice_t` or the Felucca-Engines callback ABI. Keep the source metadata outside the Metal Core engine's private per-part DSP memory: it is host lifecycle state, not oscillator/filter state.

## 5. FOLLOW selection contract

- FOLLOW ON: sequencer-origin and preview-origin notes use the resolved pattern pitch mapping; performance-origin notes retain the host-resolved live pitch.
- FOLLOW OFF: sequencer-origin and preview-origin notes use stored base FREQ; performance-origin notes retain the host-resolved live pitch.
- METAL remains independent of FOLLOW. FOLLOW must not alter METAL enable, ratio, decay or stored parameter values.
- A note source transition during MONO/LEGATO must not reset phases, filter state, envelopes or feedback delay state merely to update pitch origin.
- For a mixed POLY part, each voice follows its own source independently.

## 6. Remaining integration gates

Before source metadata is coded:

- Inspect all `mono_stack[]` mutation/removal paths and define source-array synchronization for duplicate notes and priority modes.
- Inspect `voice_alloc()`, voice stealing, engine-switch queue overflow, and reset paths to define source cleanup.
- Add host tests for simultaneous live + sequencer notes, mixed-origin MONO fallback, LEGATO transitions, UNISON, POLY, ratchets, audition, arp, engine-switch queue/replay and voice stealing.
- Confirm that the track structure's 16-byte alignment and audio ISR cost remain acceptable after adding host metadata.
- Direct v98 source verification and absolute MIDI → relative NOTE/OCTAVE mapping remain separate open questions.

No source code, shared engine ABI, parameter ID, UI, storage, build/package or device state changed in this audit.
