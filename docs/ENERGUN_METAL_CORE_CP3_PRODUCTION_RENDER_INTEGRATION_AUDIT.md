# ENERGUN METAL CORE — CP3 production render integration audit

Date: 2026-11-04

Status: source audit only; no production engine registration or render-path modification.

## Audited Felucca call path

The current production voice path is in `firmware/src/voice.c`, function
`track_render(track_t *t, int32_t *out, uint32_t n)`.

For each part it resolves `const engine_t *e = ENGINES[t->engine]`, then:

1. calls optional `e->block(t)` once per part/block;
2. iterates the part's `NVOICE` voice slots;
3. runs host voice lifecycle and host pitch/modulation preparation;
4. calls `e->render(t, v, out, n, &m)` once for each active voice;
5. calls optional `e->post(t, out, n)` once after all voices.

The engine descriptor also participates in host-owned lifecycle through
`ownenv`, `done`, `amp`, and `sampled`. These fields are not interchangeable:
an engine with `ownenv` bypasses the host ADSR except for shared voice-steal
fade handling, and `done` is consulted to end the voice.

## Integration implications

- The first complete METAL CORE implementation belongs behind a real
  `engine_t` descriptor and the per-voice `render` callback; a separate
  top-level mixer is not required.
- Per-voice oscillator phase can use the already-audited `voice_t.ph[0..1]`
  adapter. Its note-on callback must respect Felucca's sounding-retrigger
  preservation semantics.
- Part-wide state belongs in the engine's `mem` region and must fit the
  shared `ENG_MEM_PART` limit. The current memory-accounting model is
  provisional and is not a final state-layout approval.
- LPG gain envelope and LPG filter-frequency state must remain distinct.
  Neither may silently replace the host VCA/ADSR contract.
- The current isolated LPG Gate Off helper is not called by `track_render`;
  it must be driven by the correct note/gate lifecycle callback before it can
  affect audio.
- MAIN FILTER and LPG FILTER require independent histories if both are
  enabled in series.
- `render` must add into the supplied output buffer and preserve SPREAD
  routing semantics. It must not clear or replace the host mix.
- Any new persistent state must be initialized on fresh/reused voice and
  engine-memory ownership transitions, not only on ordinary note-on.

## Required implementation order

1. Freeze the parameter map and FOLLOW/METAL enable/value retention semantics.
2. Define the final per-voice and per-part state layout; add compile-time size
   assertions against the selected engine memory pool.
3. Implement a minimal descriptor in an isolated integration branch, with
   note-on, note-off and done semantics explicitly tested.
4. Connect OSC1/OSC2 and FM/METAL/FOLD in the per-voice render callback.
5. Add MAIN FILTER then LPG FILTER with separate state and numerical tests.
6. Connect LPG gate lifecycle and gain envelope without conflating it with
   host ADSR or VCA.
7. Add the host VCA/ramp and additive-output behavior, then run full regression.
8. Run AC79 compile/codegen and measure worst-case callback cycles at the
   target sample rates before production registration is accepted.

## Explicit non-claims

This audit does not implement or register an engine, alter engine IDs,
parameter/preset ABI, projects, storage, Factory V15, or physical Flash.
It does not claim target runtime timing or audio qualification.
