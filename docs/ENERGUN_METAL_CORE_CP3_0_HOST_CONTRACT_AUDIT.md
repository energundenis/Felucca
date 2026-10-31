# ENERGUN METAL CORE — CP3.0 Felucca host contract audit

Date: 2026-11-01
Branch: `energun-metal-core`
Status: source audit only; no production code changes.

## Sources inspected

- `firmware/src/core.h`
- `firmware/src/engines.c`
- `firmware/src/voice.c`
- `firmware/src/eng_metal_core.c`
- `firmware/src/eng_metal_core_vca.h`
- `firmware/src/eng_metal_core_vca_render.h`
- `firmware/src/eng_metal_core_follow_host.h`
- CP2.19 VCA performance-mode decision and CP2.23 VCA-only harness evidence.

## Confirmed host callback behavior

1. `voice_start()` writes note, velocity, gate, active state, pitch and `track_t.voice_source[]` before calling `engine_t.note_on()`. The production callback can therefore read explicit `NOTE_META_ACCENT` from the existing voice-source metadata at note start.
2. On a sounding-voice retrigger, `voice_start()` saves all three oscillator phase words, calls `note_on()`, restores the phases, then restores only the `voice_t.s[]` words selected by `engine_t.keep`. State held in the part's `ENG_MEM` region is not automatically restored; the engine must implement cancel-and-hold for its VCA state itself.
3. `track_render()` clears the part output, advances block-level modulation, builds `vmod_t`, then calls `engine_t.render()` once for each active voice. The callback must add to the mix and handle `n <= CTL` (CTL is 32).
4. With `ownenv=1`, the host bypasses normal ADSR/velocity, supplies full-scale engine amplitude, and uses `engine_t.done()` to retire a voice after the engine envelope has reached idle. During host voice-steal stage 4, host `env_tick()` still supplies the fade amplitude. The engine must multiply the supplied `m.amp0/m.amp1` ramp into its own output once; it must not replace or duplicate it.
5. `engine_t.block()` runs once per part per render block before voices. `engine_t.mem` is part-scoped, and `eng_mem_claim()` clears the declared byte range when another engine takes ownership of that part's shared region.

## Gate timing limitation

MIDI/sequencer note-off currently changes `voice_t.gate` and `voice_t.stage` in the host event path. The audio callback sees the current Gate state at the next `track_render()` block. The callback API does not provide an intra-block sample offset for a note-off event. Therefore the existing CP2.23 adapter starts Release at the render-block boundary. Sample-accurate sub-block release requires a separate host event/API change and must not be claimed by the current engine.

## Memory findings

- `NVOICE=8`, `NPART=4`.
- `ENG_MEM_PART` is 12,888 bytes when ADPCM is disabled and 13,344 bytes when ADPCM is enabled.
- `ENG_MEM(p)` is a 64-bit-aligned shared per-part region; only the currently active engine owns it.
- Eight VCA states occupy 288 bytes per part. The isolated VCA state fits both current pool configurations, but this does not account for the complete engine's other shared/per-voice state.
- `voice_t` is host-owned. The current audit reserves its three phase words and eight signed state words for explicitly documented oscillator/filter/envelope state only. No ABI growth is proposed.

## Engine-ID / optional-build finding

`NENGINES` and the `ENGINES[]` table are configuration-dependent: SLICE is conditionally index 13 and ADPCM is conditionally index 14. Engine indices are persisted in projects/presets and are append-only. A METAL CORE ID must not reuse index 14 in a build where ADPCM is disabled if that same index denotes ADPCM in an enabled build. Before production registration, reserve a stable ID after the optional ADPCM slot and update count, visibility/order, validity checks and tests for all SLICE/ADPCM build combinations. Do not silently shift existing IDs.

## Contract decisions for CP3

- Apply the CP2.19 dedicated full VCA contour decision with `ownenv=1`; do not run host ADSR a second time.
- Preserve host velocity as a VCA input and explicit Accent as separate metadata; `ownenv=1` means host does not apply velocity automatically.
- Multiply host amplitude ramp / LFO-to-AMP / engine-switch fade into the internal VCA output exactly once.
- Gate-off is block-boundary for the first integration unless a separate approved host API extension is implemented.
- Store eight VCA states in per-part engine memory and account for the complete engine state before registration.
- Keep production engine registration gated until stable ID allocation, the 37-control V1 parameter include/defer map, persistence implications and optional-build tests are resolved.

## Disposition

CP3.0 source contract audit is complete. It does not register METAL CORE, change parameter IDs, alter storage formats, modify the audio graph, or write/flash a device. The next safe coding increment is a test-only adapter against the real Felucca track/voice structures and per-part memory, followed by isolated integration of the already-tested OSC/FM/FOLD/LPG/VCA primitives. Production registration remains gated.
