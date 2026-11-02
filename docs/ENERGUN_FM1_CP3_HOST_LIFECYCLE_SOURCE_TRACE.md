# ENERGUN FM-1 CORE — CP3 host lifecycle source trace

Date: 2026-11-01
Source branch inspected: `energun-metal-core`
Source commit: `e6877d47f6b5684efab1e2cf759ed37b2ea76cd0`
Scope: source inspection only; no production code or host ABI changes.

## 1. Voice-slot model and global sounding budget

The host declares `NVOICE = 8` slots in each of `NPART = 4` tracks. These are not 32 simultaneously sounding voices. `voice.c::voices_busy()` counts active voices whose stage is not 4 across all parts and enforces one shared budget of eight sounding voices.

Consequences for the engine adapter:
- Per-part engine state must still reserve one state record for every one of that part's eight host slots.
- Runtime CPU/render load is bounded by the shared eight-voice budget, plus short stage-4 steal fades.
- Tests that instantiate four independent tracks prove memory mapping/isolation only; they do not prove global allocator behavior.

## 2. POLY allocation and voice stealing

Source trace: `voice.c::voice_alloc()`, `voice_victim()`, `voice_room()`, `voice_kill()`, `voice_reuse()`.

Observed policy:
1. DRUM may reuse the voice associated with its lane.
2. An already-active voice playing the same note is reused first.
3. A free slot may require a victim because the global eight-voice budget is full.
4. Victim priority is oldest released voice, then extra UNISON voice, then (for hard allocation) oldest held POLY voice other than the lowest held note. The bass/lowest note is protected.
5. If the victim belongs to the requesting part and can be restarted in place, the slot is returned directly.
6. A victim from another part is marked Gate-off / stage 4 and fades; it is not immediately treated as a fresh idle slot.
7. With no free slot, the requesting part reuses its oldest released voice, else its oldest held voice other than the lowest note.

A test-only lifecycle matrix must distinguish same-part in-place restart from cross-part stage-4 steal. It must not assume every steal is a fresh allocation.

## 3. Retrigger continuity boundary

Source trace: `voice.c::voice_start()`.

The host computes `sounding = v->active && v->stage != 0` before changing the voice's note, Gate, active flag and stage. It then calls the engine's `note_on()`. For a sounding non-sampled voice, after `note_on()` the host restores:
- all three host phase words `v->ph[0..2]`;
- selected `v->s[0], s[1], s[4..7]` slots according to `engine_t.keep`.

The host comment describes this as keeping phases and filter state on retrigger / steal to avoid clicks.

Important callback constraint: when an engine's `note_on()` runs, the host has already overwritten `active/stage`. The callback cannot discover the previous host stage from the current `voice_t`. An engine-owned lifecycle state (e.g. the previous VCA phase) is a candidate discriminator for fresh allocation versus sounding retrigger, but must be tested with the real callback ordering.

## 4. MONO / LEGATO / UNISON

Source trace: `voice.c::mono_play()`, `trk_note_on_source()`.

- MONO calls `mono_play(..., retrig=1)`: note-on retriggers the voice.
- LEGATO and UNISON retrigger only when there is no currently sounding note; otherwise they update pitch / modulation-note data and glide while preserving the current envelope.
- A note that does not win LOW/HIGH priority can be added to the mono stack without replacing the currently sounding note.
- UNISON allocates multiple host slots and is subject to the same shared global voice budget.
- `voice_source[]` and `mono_source[]` retain note provenance, including explicit Accent metadata, separately from MIDI velocity.

These paths are not covered by the current CP3 lifecycle contract test. They remain required host-level acceptance cases.

## 5. Engine memory ownership and switching

Source trace: `engines.c::eng_mem_claim()`, `voice.c::engine_block()`.

- The engine memory pool is one region per part, aligned to 64 bits.
- Pool size is 12,888 bytes with ADPCM disabled and 13,344 bytes with ADPCM enabled.
- `eng_mem_claim()` zeroes the selected engine's declared `engine_t.mem` bytes, rounded up to 32-bit words, only when the part's memory owner changes.
- If the selected engine has no memory, the region is left untouched; switching back to the previous memory-owning engine preserves that engine's state.
- Engine switching first fades active voices over `XF_BLOCKS = 4` blocks, clears host voice activity, changes the engine, claims memory, then replays queued notes.
- A direct engine assignment also calls `eng_mem_claim()` at the next engine block.

For METAL CORE, the production state must fit the applicable pool and its `engine_t.mem` declaration must exactly match the complete aligned state structure. The existing 288-byte VCA-only state fits both pools, but this says nothing about the final filter, oscillator, feedback and modulation state.

## 6. Test coverage boundary and next required cases

Existing CP3 real-track adapter and lifecycle matrix tests cover engine-side mapping and selected VCA lifecycle semantics. They do not execute the host's static allocator functions or the actual engine-switch dispatcher. The source trace is therefore evidence of host policy, not runtime test evidence.

Required next tests:
1. Same-note active retrigger preserves engine-owned continuity state.
2. Fresh inactive-slot reuse resets stale per-voice feedback state.
3. Released-but-not-idle slot is not treated as fresh.
4. Same-part in-place victim restart is distinguished from cross-part stage-4 steal.
5. Global eight-voice budget: oldest released victim, extra UNISON victim, oldest held non-bass victim, lowest-note protection.
6. MONO retrigger versus LEGATO pitch-only update; LOW/HIGH priority fallback; UNISON under budget pressure.
7. Accent metadata remains independent from velocity through queued notes during engine switch.
8. Engine switch fade, memory claim zeroing on owner change, and preservation when switching to/from a no-memory engine.
9. Complete typed part-state size and alignment assertions for both pool configurations.

No production engine registration, ABI growth, parameter or persistence change, factory firmware modification, device access or flashing was performed.
