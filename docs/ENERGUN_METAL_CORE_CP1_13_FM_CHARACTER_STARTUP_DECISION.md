# ENERGUN METAL CORE — CP1.13 FM CHARACTER startup decision

Date: 2026-10-31
Branch: energun-metal-core
Status: source verification complete; V1 default established as METALLIC, with a documented pre-audio UI initialization mismatch.

## Decision accepted

The user approved a verification-first procedure for resolving the V98 FM CHARACTER initialization discrepancy. The original V98 HTML reference was subsequently uploaded unchanged to this repository:

- Reference: [energun_metal_core_v98_reference.html](../energun_metal_core_v98_reference.html)
- Reference upload commit: [bfa66551cb5767b9554d1a2d3487a14105b078df](https://github.com/energundenis/Felucca/commit/bfa66551cb5767b9554d1a2d3487a14105b078df)
- Branch: `energun-metal-core`
- File path: repository root (the file was not moved or modified).

## Source trace and findings

The protected V98 source was inspected without modifying it.

1. **JavaScript parameter-object default:** `P.wcFmCharacter` is initialized to `v:2` (the mode enumeration is SMOOTH=0, CLASSIC=1, METALLIC=2). This is the value used by the sound engine before any user interaction.
2. **HTML markup default:** the `wcFmCharacterSelect` element has CLASSIC marked as selected. Before audio initialization, the static page can therefore display CLASSIC while the JavaScript parameter object already contains METALLIC.
3. **Audio initialization order:** the Play action creates/resumes the AudioContext and calls `ensureFx()`; `ensureFx()` calls `wcInit()`. The WAV-recording initialization path also calls `ensureFx()`.
4. **Final UI synchronization:** `wcInit()` calls `wcComplexSync()` before starting the oscillators. `wcComplexSync()` writes `String(P.wcFmCharacter.v)` into `wcFmCharacterSelect.value`. Thus, after audio-engine initialization, the selector is synchronized to METALLIC.
5. **Actual FM calculation:** `wcFmIndexValue()` calls `wcFmCharacterScale()`, which branches on `P.wcFmCharacter.v`. Value 2 selects the METALLIC response curve `0.55*x + 1.15*x*x`; value 1 uses the CLASSIC linear response `x`; value 0 uses the SMOOTH response `.72*sqrt(x)`.
6. **Later updates:** the selector change handler updates `P.wcFmCharacter.v` and refreshes the FM index gain when FM mode is active. The FM-index and FM-envelope paths also call `wcFmIndexValue()`, so the engine uses the parameter object rather than reading the HTML selection directly.
7. **Preset restoration:** saved presets are restored into the preset list at page load; the source does not automatically apply a preset to `P`. A preset changes the mode only when the user explicitly loads/imports it.

## CP1.13 result

**The effective V98 audio-engine startup mode is METALLIC.** This is established by the JavaScript parameter default and the FM-index calculation path. When the audio engine initializes, the UI selector is then synchronized to METALLIC.

A limited UI timing mismatch remains documented: before the first audio initialization, the static HTML selector may show CLASSIC even though the engine parameter object is already METALLIC. This is not an audio/DSP disagreement after initialization; it is a markup-versus-runtime initialization-order discrepancy. Do not silently rewrite the original V98 reference to remove it.

**V1 specification decision: FM CHARACTER default = METALLIC (value 2).** Preserve all three selectable modes and their source-defined response curves. This resolves the V1 default for parameter #12, subject to ordinary implementation parity tests.

## Scope and safety

- This closes the FM CHARACTER default question only; it does not close all of CP1.
- No V98 source was edited. No firmware code, parameter IDs, UI implementation, storage format, Flash allocation, factory firmware or physical FM-1 state was changed.
- The reference upload and this documentation update are on `energun-metal-core`; `main` remains untouched.
- This source inspection establishes source-level startup behavior. It is not a claim of physical-device audio validation or target DSP integration.
