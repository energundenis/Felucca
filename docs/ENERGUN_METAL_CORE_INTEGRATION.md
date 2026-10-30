# ENERGUN METAL CORE — Felucca integration audit

Status: initial source audit; no DSP or UI behavior changed in this commit.

## Working base

- Repository: `energundenis/Felucca` (fork of `hugelton/Felucca`)
- Working branch: `energun-metal-core`
- Felucca base commit: `65b21d344fca68bd4bbc1eb24d7b90edfbe98200`
- BSP submodule: `5942e68b89ab9ccd23f2c541d712ca38fe635f11`
- Felucca-Engines submodule: `d0260074ce100a47675814b7f38d61f11176c926`

## Findings from the source

1. Felucca has four independent parts (`NPART = 4`) and eight voices per part, with a shared sounding-voice budget of eight. The custom engine must use this allocator; it must not create a separate allocator.
2. Engine implementations are included into one translation unit through `firmware/src/engines.c`. The shared engine interface is in the Felucca-Engines submodule, `src/felucca_engine.h`.
3. The engine interface provides `engine_t`: name, two edit pages of four controls (eight `P_E0..P_E7` parameters), presets, four HOME knob mappings, voice behavior flags, note-on/render hooks, optional block/post hooks, and a per-part memory size.
4. Each part's engine state is managed by Felucca's `eng_mem` pool. The pool is sized for the largest enabled engine and claimed/cleared on engine changes. METAL CORE must declare and stay within its own memory budget.
5. Engine IDs are append-only because presets, projects, and the editor protocol store them. The current build has optional SLICE and ADPCM engines; METAL CORE's numeric ID and conditional-build behavior must be designed so existing IDs never shift.
6. `voice.c` renders engines in the audio ISR. DSP changes must be bounded, fixed-point, block-aware, and measured against the target CPU budget.
7. `tests/regress.c` and the existing host test harness provide golden audio renders, clipping/DC/tail checks, voice lifecycle checks, and CPU budget checks. A new engine needs dedicated coverage; golden data must only be updated after reviewing intended sound changes.
8. The existing `P_E0..P_E7` slots are the engine's eight editable values. FOLLOW and METAL are independent ON/OFF controls in the **OSCILLATORS / SYNTH VOICE** block. Their storage/UI representation must preserve associated parameter values while OFF; do not silently repurpose unrelated Felucca parameters or the PHASE engine's E7 FOLD.

## Agreed product constraints

- METAL CORE is our own custom engine. Felucca is the firmware base, not the source of the METAL CORE sound design.
- Initial METAL CORE target is monophonic, with stereo output through Felucca's existing signal path.
- Keep Felucca's MIDI, sync, effects, mixer, presets/projects, and four-part architecture.
- FOLLOW and METAL are displayed in the **OSCILLATORS / SYNTH VOICE** block, not on the FOLD page. FOLLOW OFF bypasses sequencer pitch tracking while retaining base FREQ; METAL OFF bypasses the additional METAL FM and Ring Modulation contributions while retaining METAL RATIO / METAL DECAY values.
- Preserve the reference behavior: METAL ON also selects its METAL-specific trigger attack/decay behavior. Do not add a new DSP algorithm or change the agreed sound architecture.
- Do not alter other engines, pages, or global behavior as a shortcut.
- Do not flash the FM-1 until hardware revision, reproducible build, and recovery path are verified.

## Integration sequence

1. Confirm submodule checkout/build reproducibility and inspect the exact engine build/test inclusion points.
2. Complete the reference-derived DSP/control map: oscillator destinations, FOLLOW pitch-tracking behavior, METAL FM/Ring Mod tap points, trigger envelope selection, parameter scaling, retained OFF-state values, and memory/CPU requirements.
3. Resolve append-only engine ID handling with optional SLICE/ADPCM builds.
4. Implement the engine in isolated source files and register it without modifying existing engines' DSP.
5. Add engine-specific host regression tests and CPU measurements; run the full regression suite.
6. Port the FMini-1-inspired UI patterns into Felucca's four-part UI without enabling its single-track-only application mode.
7. Add FOLLOW/METAL switches in the **OSCILLATORS / SYNTH VOICE** block with persistent, value-preserving OFF behavior.
8. Build and verify the package and recovery route before any hardware installation.

## Explicit non-goals for this audit commit

- No changes to `engines.c`, `core.h`, `voice.c`, `params.c`, or UI runtime behavior.
- No substitution of PHASE's E7 FOLD or the FX insert FOLD for METAL CORE.
- No firmware binary generation, flashing, or claim of hardware validation.
