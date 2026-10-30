# ENERGUN METAL CORE — Voice Lifecycle and Preset Persistence Audit 01

Status: source audit. This document records host behavior and integration constraints; it does not implement DSP or change the firmware.

## Audited source revisions

- Felucca branch: `energun-metal-core`
- Engine ABI submodule: Felucca-Engines `d0260074ce100a47675814b7f38d61f11176c926`
- Relevant host files: `firmware/src/voice.c`, `firmware/src/params.c`, `firmware/src/project.c`
- Relevant engine ABI: `src/felucca_engine.h`

## 1. Voice start and retrigger behavior

The host voice-start path in `voice.c`:

1. Captures all three phase accumulators and selected engine state words before calling the engine's `note_on`.
2. Sets note, velocity, gate, active, stage, pitch, modulation velocity/random and glide.
3. For a voice that was not already sounding, resets the host amplitude envelope to zero.
4. Calls the engine's `note_on`.
5. If this is a retrigger/steal of a sounding non-sampled voice, restores all three phase accumulators unconditionally.
6. Restores only state slots selected by `engine_t.keep`: bits 0, 1, 4, 5, 6, 7 correspond to `s[0]`, `s[1]`, `s[4]`, `s[5]`, `s[6]`, `s[7]`.

Consequences for Metal Core:

- OSC1/OSC2 phase continuity is already handled by the host on sounding retriggers; do not implement duplicate phase preservation.
- `s[2]` and `s[3]` are not preserved by this host path. They are suitable candidates for LPG filter integrators that are intentionally reinitialized by `note_on` on retrigger.
- If `s[0]`, `s[1]`, or `s[4..7]` are used for audible state that must survive retrigger, the corresponding `keep` bits must be set and tested. Do not set keep bits by guess.
- For a fresh voice, the host does not restore the old state after `note_on`; the engine must initialize every state slot it relies on.
- The host's mono legato path can update pitch without calling a full voice restart. Any LPG envelope behavior during legato must be explicit and must not assume every note-on causes a reset.

## 2. Note-off and voice lifetime

The host owns the ordinary ADSR/gate lifecycle. Metal Core's first version should use that lifecycle and must not claim a voice remains alive after the host releases it.

The LPG decay is a timbral/control envelope, not a replacement for the track ADSR and not an independent voice-lifetime controller. It may shape the post-FOLD LPG stage while the voice is active, but note-off, release, voice stealing, and engine switching remain governed by Felucca.

Before implementation, test the exact behavior for:
- short gate and fast retrigger;
- long held note then note-off;
- mono legato without retrigger;
- voice steal while a previous release is audible;
- changing engine while old voices fade.

## 3. Parameter descriptors and stored EDIT values

The engine ABI exposes eight `param_desc_t edit[8]` descriptors, indexed as `P_E0..P_E7`. Felucca's parameter lookup uses the active/requested engine descriptor (or its optional `desc` hook) for label, range, default and formatting. The UI labels the two groups as EDIT 1 (E0..E3) and EDIT 2 (E4..E7).

The native preset structure contains `int8_t e[8]` for the engine's eight EDIT values, plus separate envelope, filter-envelope, mono/poly and FX-send fields. This confirms that all eight Metal Core EDIT values can be part of the native preset model.

Projects serialize the track parameter array, including engine selection and the engine parameter values. The project loader validates/clamps stored parameter values against the current descriptors. The project format is versioned and has explicit compatibility handling; do not alter the global project schema just to add Metal Core.

Important distinction:
- Eight EDIT values are natively represented in presets/projects.
- This does **not** establish a mechanism for persisting an unlimited number of extra hidden engine-specific parameters.
- The first implementation must therefore fit the sound-defining controls into eight stored values, unless a separately designed extension to persistence is approved.

## 4. Parameter design consequence

The current conceptual pages (OSC, FM, FOLD, LPG) each contain more than two independent controls, while the host only exposes eight persistent EDIT values. We must resolve this before writing engine code.

Recommended approach for v1:
- Keep eight persistent macro parameters with meaningful musical ranges and clear labels.
- Put closely related sub-controls behind mode-dependent descriptors only if the same stored value has a well-defined interpretation in each mode.
- Do not hide independent controls in volatile state; they would not recall reliably.
- Do not silently collapse OSC2 direct level, FM amount, ratio, feedback, FOLD amount, symmetry, drive, LPG cutoff, resonance, decay and response into eight knobs without a deliberate priority decision.
- Consider using a compact page/mode layout where the four physical knobs address one set at a time, but remember that changing pages does not create additional stored parameters.

This is a genuine design gate: the full agreed control list exceeds the native persistent parameter count. The right answer may be a two-layer control scheme or a later persistence extension, but it should be decided explicitly rather than improvised in DSP code.

## 5. Implementation rules derived from this audit

- Initialize all state used by a new voice in `note_on`.
- Keep `s[2]` and `s[3]` as initial LPG filter integrators if reset-on-retrigger is desired.
- Assign `s[0]`, `s[1]`, and `s[4..7]` only after the keep/retrigger behavior of each state has been decided.
- Leave phase retention to the host.
- Use native `preset_t.e[8]` and project parameter serialization; do not introduce a second preset system.
- Validate save → power-cycle/load → recall for every exposed EDIT value once the engine is registered.
- Do not change project serialization or the shared voice ABI as part of the first Metal Core implementation.

## 6. Next gate

Before DSP implementation, approve the v1 persistent-control strategy: eight macro controls versus an explicitly designed extension. Once that is decided, the DSP can be implemented against stable parameter semantics and tested without later remapping saved sounds.

No engine DSP, UI integration, firmware build, target timing test, hardware test or flash is included in this audit.
