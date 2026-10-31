# ENERGUN METAL CORE — CP2.12 host FOLLOW adapter contract

Date: 2026-10-31
Branch: `energun-metal-core`
Status: source-audited integration contract; implementation not yet connected.

## 1. Host facts verified

The Felucca render path provides the following inputs at `engine_t.render(t, v, out, n, &m)`:

- `track_t *t`: owns `voice_source[NVOICE]`.
- `voice_t *v`: the exact active voice being rendered; the host guarantees it belongs to `t->v[]`.
- `vmod_t *m`: final block pitch increment in `m->inc`.
- `v->note`: target MIDI note set by `voice_start()` and refreshed by MONO/LEGATO note changes.
- `pitch_inc()`: Felucca's sample-rate-specific MIDI pitch table conversion.
- `FS`: the compile-time sample rate used to generate that table.
- `NOTE_SRC_PERFORMANCE` and `NOTE_SRC_SEQUENCER`: host source tags; stopped step preview is tagged SEQUENCER, arpeggiator remains PERFORMANCE.

Source files inspected:
- `firmware/src/voice.c`: voice start, glide, final `m.inc`, render callback.
- `firmware/src/mod.c`: modulation-matrix pitch recomputation and fine pitch.
- `firmware/src/core.h`: source enum and per-voice source array.
- `firmware/src/engines.c`: shared DSP table and engine include ordering.

## 2. Adapter contract

The eventual wrapper should accept `track_t *t`, `voice_t *v`, `const vmod_t *m`, Metal Core `base_inc`, and FOLLOW state.

It should:
1. Validate the voice index derived from `v - t->v` is below `NVOICE`.
2. Read source from `t->voice_source[index]`; never infer source from the MIDI note value.
3. Compute the host C4 reference with `pitch_inc(60 * 16)`.
4. Compute the target-note reference with `pitch_inc(v->note * 16)`.
5. Use final `m->inc`, not `pitch_inc(v->note * 16)`, as the numerator; this preserves glide, track tune, bend, LFO, envelope, fine pitch and modulation-matrix pitch already resolved by Felucca.
6. Call the isolated CP2.11 Q24 ratio helper to produce the selected Metal Core phase increment.

FOLLOW selection:
- PERFORMANCE: final host pitch relative to C4; FOLLOW has no effect.
- SEQUENCER + FOLLOW ON: final host pitch relative to C4.
- SEQUENCER + FOLLOW OFF: final host pitch relative to target step note, removing only the sequencer note's absolute pitch while retaining the host pitch delta.

All pitch increments must come from the same generated Felucca pitch table / sample rate. The adapter must not rebuild a second MIDI table or use the CP2.9 -2..+2 octave clamp.

## 3. Important implementation cautions

- The host passes a valid voice pointer from the track's own `t->v[]`; pointer subtraction is only valid under that callback contract. Keep a defensive index bound before indexing `voice_source[]`.
- `m->inc` already contains residual fine pitch: `voice.c` applies it after `pitch_inc()`, and `mod.c` reapplies it after modulation-matrix pitch changes.
- `v->note` is the target note, while `v->pitch_cur` is the glide state. The final-to-target ratio intentionally preserves glide as an independent offset in FOLLOW OFF.
- The helper uses 64-bit division. This is not approved for the audio callback until target instruction cost is measured; if expensive, compute/cache the ratio at the host's block/control boundary without changing note semantics.
- The adapter must not mutate `t`, `v`, or `m`.

## 4. Tests required before integration

- Mock adapter tests verify source lookup for each voice index and all three source/FOLLOW combinations.
- Actual host-table tests use Felucca's generated `pitch_inc()` at supported sample rates (44.1, 48, 96 kHz).
- Full MIDI 0..127 table sweep checks monotonic increments, no zero values, and expected octave ratios.
- Glide test checks FOLLOW OFF follows the host glide delta while remaining anchored to base FREQ.
- Pitch Bend, track tune, LFO pitch, envelope pitch, fine pitch and modulation-matrix pitch tests compare adapter output with the corresponding host increment ratios.
- Source lifecycle tests cover voice reuse, MONO/LEGATO, arpeggiator, stopped preview and engine-switch queue.
- Target CPU cost and Nyquist/aliasing behavior must be measured before callback integration.

## 5. Scope boundary

CP2.12 defines the host adapter only. It does not register Metal Core, add UI or parameter IDs, change preset/storage, package firmware or write to the FM-1. CP2.11 CI is still pending; no claim of render integration is made.
