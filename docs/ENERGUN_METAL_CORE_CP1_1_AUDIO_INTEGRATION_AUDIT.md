# ENERGUN METAL CORE — CP1.1 Felucca audio integration audit

Date: 2026-10-31
Branch: energun-metal-core
Status: source-backed audit; no DSP algorithm, parameter ID, engine registration, UI behavior, or storage format implemented.

## 1. Host audio path and engine contract

The current Felucca audio path is:

`audio ISR -> audio_block() -> mix_block() -> track/part rendering -> track processing and mixer -> stereo I2S output`.

The custom engine must remain a normal Felucca engine:
- Use the existing four-part architecture and shared voice allocator. Do not create a second allocator or a private audio callback.
- The engine interface is supplied by the pinned Felucca-Engines submodule.
- `note_on(t, v)` handles fresh note starts and retriggers.
- `render(t, v, out, n, m)` adds samples to the existing mix; it must not overwrite `out[]`.
- Host render blocks are `CTL=32` samples; support bounded `n <= CTL`.
- The initial Metal Core voice is monophonic by product decision, but it still uses the host track's voice allocation and lifecycle.
- Host owns stereo placement and global effects; Metal Core emits its mono voice signal into the normal part path.

## 2. Envelope and voice lifecycle

The source-backed lifecycle audit confirms:
- Felucca's default `ownenv=0` retains host ADSR and velocity.
- Host ADSR is linear attack plus exponential decay/release; mono legato can retain an existing envelope.
- v98 uses a separately scheduled trigger VCA with velocity/accent scaling and target-time segments. Its curve and retrigger/legato behavior are not mathematically identical to the host ADSR.
- V1 should start with `ownenv=0`; do not transfer voice lifetime to Metal Core merely to implement LPG or FM-index modulation.
- Keep the following envelopes distinct: host voice ADSR, LPG transient/decay, Complex FM-index envelope, and METAL-specific decay/trigger behavior.
- `P_LEVEL` is the host's final track mixer gain, not the v98 per-trigger VCA level or an internal module-volume control.
- `P_DETUNE` is host UNISON voice spread, not OSC2 detune.

The host ADSR is a candidate for normal voice amplitude shaping, but v98 parity must be calibrated and must not be claimed as exact before measurement.

## 3. Memory and DSP constraints

- Persistent per-part state must be declared through `engine_t.mem` and fit the shared `ENG_MEM_PART` pool.
- Per-voice state must use only documented voice state slots and explicitly define `keep` behavior on retrigger.
- Oscillator phases, filter/LPG state, and any feedback memory require a bounded and measured memory budget.
- DSP must be fixed-point and bounded per 32-sample control block.
- CPU cost must be measured both in host tests and with the pi32v2 target disassembly/budget check.
- The engine must pass golden render, clipping/DC, tail, voice lifecycle, and target CPU regressions before sound validation.

## 4. Parameter and persistence constraints

Current source-backed inventory:
- `P_COUNT=111`, `P_E0=103`, eight engine edit values `P_E0..P_E7`.
- IDs must remain below 128 because bit 7 is used by motion-lock records. At most 16 common IDs can be inserted before E0; this is a hard ceiling, not an implementation target.
- The proposed V1 inventory has 35 independently stored controls including FOLLOW and METAL toggles.
- The existing FUN10 project object has only three bytes of tail reserve; one common parameter needs four packed bytes for four tracks. No full additional common parameter fits in the current project object.
- The current proposal therefore requires a separately versioned project sidecar for controls beyond existing E0..E7 and any approved common IDs.
- Candidate XDATA sectors `0xE9000–0xF1000` are not approved for use. They are inside the update-loader scan window; the final 256 bytes per sector must remain erased. The candidate range must be checked against a full dump from the user's own FM-1 before any storage implementation.
- Sidecar A/B transaction, generation/CRC association, archive backup/restore, browser editor protocol, and power-loss tests are mandatory before using it.

No new parameter IDs or flash sectors may be assigned at this audit stage.

## 5. Engine registration and compatibility

- Engine IDs are persisted and append-only. Existing indices must never move.
- Optional SLICE and ADPCM builds affect the engine table; the new ID must be appended in a way that preserves all existing IDs under all supported build configurations.
- The current `eng_metal_core.c` remains intentionally isolated and is not included in `engines.c`.
- Do not register a silent placeholder.
- Existing engines, MIDI, sync, sequencer, effects, projects, presets, and editor behavior remain unchanged.

## 6. UI placement correction

The agreed FOLLOW and METAL switch location is the **OSCILLATORS / SYNTH VOICE** block, not FOLD. The stale scaffold comment was corrected in commit `d6452c1935c96e7c51048e7eb506898dd88277ce`. This was a comment-only correction; no runtime UI or DSP changed.

## 7. CP1.1 conclusion and next gate

The host audio integration boundary is understood well enough to begin isolated DSP primitive prototypes, but full engine integration is blocked on:
1. completing the reference-derived parameter and signal-flow specification;
2. deciding the exact V1 control allocation and approving the sidecar design;
3. obtaining and auditing the user's full FM-1 V1.5 dump before selecting flash sectors;
4. resolving append-only engine ID behavior across optional build configurations.

Next: CP1.2 — freeze the reference-derived control/signal map and a safe parameter allocation proposal. No firmware registration, flash write, package, or device flashing is authorized by this audit.
