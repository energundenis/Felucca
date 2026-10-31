# ENERGUN METAL CORE — CP1.15 V98 VCA source specification

Date: 2026-10-31
Branch: energun-metal-core
Status: source trace recorded; sample-domain port and mode-mapping decisions remain open.

## 1. Sources inspected

- Protected original reference: `energun_metal_core_v98_reference.html`, upload commit `bfa66551cb5767b9554d1a2d3487a14105b078df`.
- V98 parameter object: source line 390.
- V98 audio graph and VCA initialization: lines 508 and 526.
- V98 `wcSyncOn()`: line 534.
- V98 `wcTrig(p)`: lines 570–589.
- V98 `wcHold()`: line 593.
- Felucca voice envelope/lifecycle: `firmware/src/voice.c`, `env_tick()` around lines 608–635 and engine processing around lines 720–770.
- Existing own-envelope example: `firmware/src/eng_fm6.c`, `fm6_done()` around line 357 and `ENG_FM6` descriptor around lines 430–442.

No source code was changed.

## 2. V98 parameter defaults and ranges

Values below are directly read from the V98 parameter object; they are not newly selected values.

| Parameter | Default | Range | Used by VCA |
|---|---:|---:|---|
| LEVEL | 0.50 | 0..1 | Overall peak and sustain scale |
| SUSTAIN | 0.00 | 0..1 | Sustain target |
| RELEASE | 0.20 s | 0.01..3 s | Release time constant input |
| ATTACK | 0.01 s | 0.001..1 s | Non-METAL attack |
| DECAY | 0.30 s | 0.05..3 s | Non-METAL decay |
| METAL | ON | 0/1 | Selects special attack/decay |
| METAL DECAY | 0.40 s | 0.05..3 s | METAL decay time constant input |

When METAL is ON, V98 overrides ATTACK to 0.002 s and uses METAL DECAY instead of DECAY. RELEASE remains RELEASE. These are source defaults, not an independent approval of the entire 37-control inventory.

## 3. Trigger and target equations

At each `wcTrig(p)` event, V98 captures `t0 = AudioContext.currentTime`.

### Gate duration

`gate = 60 / BPM / 4` seconds (one sixteenth-note interval), independent of the step's ratchet subdivision. Ratchets call `wcTrig(p)` again at subdivisions of that interval; each trigger cancels/holds the current VCA automation before scheduling a new contour.

### Velocity and Accent

- If step velocity exists: `vel = clamp(p.vel, 0.1, 1.0)`; if absent: `vel = 0.8`.
- `accentGain = 1.18` when accented, otherwise 1.0.
- `peak = LEVEL × vel × accentGain`.
- `sustain = max(0.0001, LEVEL × SUSTAIN × vel × (accent ? 1.08 : 1.0))`.

The VCA target values can exceed LEVEL when Accent is enabled; preserve the source's arithmetic, then separately validate fixed-point headroom and final output safety.

### Attack

`attackTime = METAL ? 0.002 : ATTACK`

At `t0`, target `peak` with time constant:

`tauAttack = max(0.005, attackTime / 3)`

### Decay / Sustain

`decayTime = METAL ? METAL_DECAY : DECAY`

At `t0 + max(0.005, attackTime)`, target `sustain` with:

`tauDecay = max(0.005, decayTime / 3)`

### Release

At `t0 + gate`, target `0.0001` with:

`tauRelease = max(0.005, RELEASE / 3)`

The V98 implementation uses Web Audio `AudioParam.setTargetAtTime`. Its ideal one-pole segment is:

`y(t) = target + (y_start - target) × exp(-(t - t_start) / tau)`

The V1 sample-domain implementation must approximate this response deterministically and document the selected fixed-point coefficient and rounding.

## 4. Retrigger / automation cancellation

Before scheduling a new VCA contour, V98 calls `wcHold(wcVCA.gain, t0)`. Where supported, this uses `cancelAndHoldAtTime(t0)`, preserving the current gain and cancelling future automation. The fallback calls `cancelScheduledValues(t0)` and does not explicitly restore the instantaneous value; this fallback is not mathematically equivalent. The firmware implementation must preserve current envelope level on retrigger and must not reproduce a discontinuous fallback artifact.

Ratchet triggers use the same trigger path. Retrigger can therefore occur during Attack, Decay or Release; tests must cover all three.

## 5. VCA placement and operating modes

The V98 audio graph routes:

`LPG filter → LPG gain → VCA gain → ring/output stage`

Therefore VCA is the final per-voice amplitude stage after the LPG gain, not a replacement for the LPG envelope.

V98 has two operating modes:

- Sequencer mode (`P.wcSeq=1`): VCA initializes at zero; `wcTrig(p)` schedules the above per-step contour.
- Manual mode (`P.wcSeq=0`): `wcSyncOn()` sets VCA to `LEVEL × (WC ON ? 1 : 0)`; the sequencer-triggered VCA contour is not used.

This distinction is part of the original reference. The exact mapping of these two modes onto FM-1 host MIDI/performance notes versus the integrated ENERGUN sequencer remains an explicit specification item; do not silently assume every incoming MIDI note should trigger the sequencer VCA contour.

## 6. Felucca integration contract verified in current host source

The current `voice.c` engine processing confirms:

1. The host calls `env_tick()` for active voices.
2. With `engine_t.ownenv` enabled, it calls the engine's `done(t, v)` callback. If done, the host clears `active`, `gate`, `stage`, `env` and `env_out`.
3. While the engine reports not-done, host ADSR output is forced to full scale; it does not shape the engine amplitude.
4. The engine may provide its own amplitude callback `amp(t, v, env)`; the resulting value becomes `m.amp1` without the host's normal velocity multiplication when `ownenv` is enabled.
5. FM6 is a working example: `ENG_FM6` sets `.ownenv = 1` and `.done = fm6_done`. Its `fm6_done()` checks its own note/envelope state and returns true when the sound is finished.

For METAL CORE, the chosen VCA must therefore apply Velocity/Accent itself. The final implementation must decide whether to multiply its sample-domain contour inside `render()` or use the engine amplitude callback; verify the exact `engine_t` callback signatures from the pinned Felucca-Engines submodule before coding. The `done()` callback must only end a voice after its internal VCA has completed the release.

## 7. Source-derived edge cases requiring explicit tests / resolution

- **Short gate vs Attack:** V98 schedules Release at `t0 + gate` even if `gate < max(5 ms, attackTime)`. The later-scheduled Decay target can then occur after the Release event. This can produce a non-intuitive envelope event order for long non-METAL Attack or very short Gate. Do not silently normalize the order; add a regression fixture and decide whether V1 should preserve the reference ordering or use a safe bounded rule.
- **SUSTAIN = 0:** V98 clamps sustain target to 0.0001, not exact zero.
- **Accent headroom:** peak may be 1.18 × LEVEL × velocity. Fixed-point range and final mixer headroom must be checked.
- **Retrigger:** must hold the current envelope value, cancel stale future segments and schedule a new contour.
- **Mode mapping:** sequencer-triggered envelope versus manual constant LEVEL behavior must be mapped deliberately to FM-1 note sources.
- **Release and host lifecycle:** `done()` is polled by the host in its voice loop; the implementation must prove release completion and no stuck/early-ended voices.
- **Voice stealing / engine changes:** ensure state is reset/retired consistently when the host reassigns a voice or switches engines.

## 8. Next implementation gates

1. Read the pinned Felucca-Engines submodule header/API at the exact submodule SHA and record `engine_t` callbacks and `voice_t` gate/velocity semantics.
2. Decide the FM-1 mapping for V98 sequencer and manual modes.
3. Resolve the short-gate/long-attack event-order case with an explicit compatibility rule.
4. Define sample-rate-independent one-pole coefficient generation (outside sample loop) and fixed-point formats.
5. Build a pure isolated VCA module and deterministic fixtures against the equations above.
6. Add lifecycle, retrigger, velocity/accent and voice-steal tests.
7. Update per-voice state/memory budget, then measure target CPU before any render integration.

## 9. Status and safety

- CP1.14 architecture choice remains RESOLVED: V98-compatible engine-owned VCA, no host-ADSR cascade.
- CP1.15 source trace is RECORDED; numeric sample-domain implementation is NOT STARTED.
- CP1 overall remains OPEN.
- No firmware code, engine registration, parameter IDs, UI, persistent storage, Flash allocation, package, factory firmware or physical FM-1 state changed.
- `main` remains untouched.

## 10. Pinned Felucca-Engines API verification (CP1.15 follow-up)

The host repository pins `firmware/engines` to submodule commit `d0260074ce100a47675814b7f38d61f11176c926` (Felucca-Engines). The exact header `src/felucca_engine.h` at that commit was inspected; header blob SHA: `8e4dd5d5e1ec22f4a1a897ad15e783c42349037a`.

Verified API details:

- `engine_t.amp(track_t *, voice_t *, int32_t adsr)` is an optional callback called once per control tick. It receives the host ADSR value in Q15 after `env_tick()` and returns Q15 amplitude.
- `engine_t.ownenv = 1` means the engine's envelopes own voice amplitude: host ADSR is not applied, host velocity multiplication is not applied, ENV modulation source is zero, and the voice remains active until `done()` returns true.
- `engine_t.done(track_t *, voice_t *)` is called once per control tick before the render callback. It is the engine's responsibility to report completion only after its own Release is finished.
- `voice_t` exposes `gate`, `active`, `vel`, `mvel`, `stage`, `env` and `env_out`. The header documents `note_on` on voice start/retrigger and the `vmod_t` amplitude ramp `amp0 → amp1`.
- Existing FM6 demonstrates `.ownenv = 1` and `.done = fm6_done`; its completion callback checks its own per-voice note/envelope state.

The API verification gate is therefore RESOLVED for this exact pinned submodule revision. It does not yet prove the best VCA implementation strategy or target CPU cost. A sample-domain VCA may be applied in the engine render loop; alternatively the optional `amp` callback can provide a control-rate Q15 value for the host's block amplitude ramp. Select one after deterministic accuracy tests, and do not multiply both paths.

The current host source calls `done()` before `render()`. If a sample-domain envelope reaches silence during a render block, the host can observe completion on the next control tick; test and document this block-boundary behavior to prevent clicks or premature voice reuse.
