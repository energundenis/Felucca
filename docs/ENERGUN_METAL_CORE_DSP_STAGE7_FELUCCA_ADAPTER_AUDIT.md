# ENERGUN METAL CORE — DSP stage 7: Felucca callback adapter audit

Date: 2026-10-31
Branch: `metal-core-dsp-stage1-osc`
Status: an unregistered callback adapter prototype now targets Felucca's actual `engine_t` interface. Host tests use the pinned engine header and a minimal track/memory shim. This is not a full firmware build.

## 1. Source-grounded host contract

Pinned Felucca-Engines header:
`firmware/engines/src/felucca_engine.h`, submodule commit `d0260074ce100a47675814b7f38d61f11176c926`.

The actual interface provides:
- `block(track_t *)`: once per block and part, before voices, including when no voice sounds;
- `render(track_t *, voice_t *, int32_t *, uint32_t, const vmod_t *)`: adds up to CTL samples to the part output;
- `post(track_t *, int32_t *, uint32_t)`: after voices, on the summed part output, including when no voice sounds; return value indicates whether an audible engine tail remains;
- `mem`: bytes in the shared per-part engine memory region.

The host `engines.c` owns `ENG_MEM_PART`, `ENG_MEM(p)`, `ENG_MEM_FITS`, and zeroes an engine's memory region when a different memory-using engine claims that part. The pool is zero-initialized at boot.

The host `voice.c` calls `block` before the voice loop and `post` after the voice loop. It also has a silent-sustain path that can skip an individual voice's render callback while still reaching `post`.

The host `fx.c` calls `track_render` for every part in each mix block. If there are no sounding voices and no effect tail, `mix_part` returns without sending the silent buffer to the mix. Therefore the adapter's `post` advances internal phases but returns 0: continuous internal state does not create an audible tail or force silent part mixing.

## 2. Adapter prototype

Files:
- `firmware/src/eng_metal_core.c`
- `firmware/src/eng_metal_core_adapter.inc`
- `firmware/src/engines.c` includes both files after FM6.

The adapter:
- supplies a required no-op `note_on` callback: Felucca's `voice_start()` calls `e->note_on(t, v)` unconditionally, so NULL is not valid; the no-op deliberately leaves per-part phases and delay history untouched on retrigger;
- obtains one `metal_core_part_state_t` from `ENG_MEM(t - trk)`;
- declares exact `mem = sizeof(metal_core_part_state_t)` and `ENG_MEM_FITS`;
- uses `block` to initialize the host-zeroed region on first use and reset the per-block advancement guard;
- uses `render` to advance and emit the shared per-part DSP state;
- uses `post` to advance the same state only if no render happened;
- returns 0 from `post` because there is no audible effect tail;
- declares `poly = 1` for the monophonic V1 adapter.

The adapter descriptor `ENG_METAL_CORE_ADAPTER` is deliberately not inserted into `ENGINES[]`. No engine index, preset, editor protocol, parameter mapping, or UI has changed.

Temporary oscillator settings in the adapter are compile/test scaffolding only. The feedback reference is a fixed panel-FREQ placeholder independent of the note's tracked pitch, so the adapter does not regress the audited v98 feedback scaling. This is not a user-adjustable panel parameter and does not resolve the FOLLOW/MOD RATIO discrepancy. These settings do not define the approved E0..E7 map and must be replaced after parameter semantics and audio calibration are approved.

## 3. Host tests

`tests/metal_complex_fm_test.c` now includes the actual pinned `engine_t` header, the core, and the adapter with a minimal host track/memory shim. It checks:
- callback pointers and descriptor memory/poly fields, including the mandatory non-resetting `note_on` hook;
- first-block initialization from zeroed host memory;
- active render advances phases and delay once;
- post does not double-advance after active render;
- post advances the state when no voice rendered;
- render resumes the same state after an idle block;
- feedback reference remains independent of a changed FOLLOW-tracked note pitch.

The previous state-machine test now explicitly zeroes its test structs before calling the initialization helper. Production initialization relies on Felucca's documented zeroed per-part pool; it no longer clears the whole feedback ring in the first audio callback.

## 4. Note and engine-switch lifecycle audit

Additional source audit of `firmware/src/voice.c` and `firmware/src/engines.c`:

- `voice_start()` calls `e->note_on(t, v)` unconditionally. The adapter therefore provides a no-op hook rather than NULL.
- On a sounding retrigger, the host restores `voice_t.ph[0..2]` after `note_on` for non-sampled engines. The METAL CORE phase and feedback ring are separate per-part state and are left untouched by the no-op hook.
- `trk_nvoice()` uses `engine_t.poly` as the cap for POLY and UNISON. `.poly = 1` therefore limits the adapter to one host voice in those modes; MONO/LEGATO continue through the host's existing mono path.
- During an engine switch, `engine_block()` claims the new engine's memory after the old engine fade. If another memory-using engine owns the region, `eng_mem_claim()` zeroes the new engine's declared `mem` bytes. A memoryless engine does not claim or clear the region, so switching through one preserves the previous memory-using engine's state, matching Felucca's documented memory semantics.
- The adapter does not add custom note-off or engine-switch hooks. Note release remains the host ADSR's responsibility; the per-part oscillators and feedback delay continue through silent blocks while METAL CORE remains the selected engine.

These are source-audit conclusions, not a target-device test.

## 5. Build boundary

What is tested:
- C compilation of the adapter against the actual `engine_t` interface;
- syntax-only compilation of the real `firmware/src/engines.c` translation unit, using Felucca's actual include order, pinned engine submodule, host preamble (`libc.c`, `core.h`) and generated host tables/sample/FM6 headers;
- callback sequencing through host tests, including the required non-resetting `note_on` hook;
- fixed-point DSP regressions at 44.1 and 96 kHz.

What is not tested:
- the complete `felucca.c` firmware translation unit in the JieLi target toolchain;
- code generation/ABI under JieLi's pi32v2 clang (the `engines.c` check above is host-compiler syntax-only);
- full firmware image/package build;
- target CPU and RAM measurements;
- actual FM-1 hardware or audio output.

Felucca's documented full build needs the JieLi pi32v2 toolchain, AC79 SDK and other prerequisites; those are not available through this host-only CI workflow. The source include in `engines.c` now passes a host-compiler syntax-only check in the real source include order. This is stronger than the adapter shim test, but it is still not evidence of a successful target build.

No engine registration, flashing, SysEx, device access, flash write, erase or restore has occurred.

## 6. Next gate

1. The host callback tests and full `engines.c` syntax-only harness now pass.
2. Note/retrigger and one-voice-cap behavior have been source-audited; next audit the parameter handoff and engine-switch persistence in a dedicated host regression.
3. Resolve the v98 FOLLOW/MOD RATIO discrepancy and freeze parameter semantics.
4. Only then prepare a separate reviewable integration change. Keep `ENGINES[]` registration and UI changes gated behind explicit review.
