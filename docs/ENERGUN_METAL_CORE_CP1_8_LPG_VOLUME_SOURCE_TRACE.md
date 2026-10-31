# ENERGUN METAL CORE — CP1.8 LPG MODULE VOLUME source trace

Date: 2026-10-31
Branch: energun-metal-core
Reference: protected Library HTML `energun_westcoast_synth_v98_metal_core_follow_metal_on(1).html`, 140,537 bytes.
Status: exact source trace; no implementation or parameter-map changes.

## 1. Exact source behavior

The v98 source declares:
- `wcLpgVolume = 1` as a stored parameter, range 0..1;
- one audio node, `wcLpgGain`;
- graph: `wcFilt → wcLpgFilter → wcLpgGain → wcVCA`.

The knob handler (`wcLpgVolume`) writes:
`wcLpgGain.gain.setTargetAtTime(P.wcLpgVolume.v, t, .02)`.

The sequencer trigger (`wcTrig`) writes to the exact same AudioParam:
1. `wcHold(wcLpgGain.gain, t0)` cancels scheduled automation at trigger time while holding the current value.
2. It computes `peakLpg = 0.25 + RESPONSE × 0.75`.
3. It schedules that peak using the LPG attack.
4. It schedules the gain to `0.0001` at the LPG tail.

Critically, `peakLpg` does **not** multiply by `P.wcLpgVolume.v). The stored MODULE VOLUME value is therefore overwritten by the next trigger's envelope peak and does not act as an independent gain multiplier during the note.

## 2. Correct interpretation

The prior CP1.5 statement that the knob and envelope share one gain node was correct but incomplete. The stronger conclusion is:
- there is no separate post-envelope LPG gain node;
- the module-volume knob writes a base value to the same gain AudioParam;
- the next sequencer trigger replaces that base with the RESPONSE-derived envelope;
- the effective per-step LPG peak is controlled by RESPONSE, not MODULE VOLUME.

Therefore v98 contains a visible/stored MODULE VOLUME control whose effect is transient or overwritten during normal sequencer operation. This is a reference implementation defect/semantic collision, not a clean independent volume stage.

## 3. LPG design document correction

The sentence in LPG design 02 that says “MODULE VOLUME controls the LPG gain” must not be read as a claim that it is an independent gain multiplier throughout the trigger. It only describes the knob's direct write. The trigger subsequently overrides that same gain.

The accurate reference model is:
- RESPONSE determines LPG transient peak and opening attack;
- DECAY determines per-step tail;
- MODULE VOLUME writes the shared gain's non-trigger/base value but is overwritten by the next trigger envelope;
- LPG filter cutoff has its own transient schedule;
- LPG gain remains before the internal VCA.

## 4. V1 design decision options (not yet selected)

**Option A — faithful bug compatibility**
- Keep a single shared gain AudioParam equivalent.
- Preserve the fact that the trigger overwrites the module-volume setting.
- Highest literal source compatibility, but MODULE VOLUME has little/no useful control during sequencer playback.

**Option B — useful independent module output gain**
- Keep the LPG envelope gain node and add a separate post-envelope LPG volume multiplier.
- The output is multiplied by the stored MODULE VOLUME after the transient envelope.
- Makes the control useful and predictable, but intentionally changes v98 behavior and adds one gain stage/state.

**Option C — envelope-scaled LPG peak**
- Keep one LPG gain stage but compute the trigger peak as `(0.25 + RESPONSE × 0.75) × MODULE VOLUME`.
- No extra gain node; MODULE VOLUME becomes a meaningful envelope-peak scale.
- Also changes v98 behavior but costs less state/CPU than Option B.

No option is silently selected. The choice must be made before finalizing LPG-03 expected results and the fixed-point signal map.

## 5. Recommendation for sound-faithful V1

Prefer **Option C** as the first listening candidate: it preserves a single LPG gain stage and the existing response/decay contour while making MODULE VOLUME effective during every trigger. Keep Option A as the exact-reference compatibility mode only if strict v98 reproduction is required. Option B is appropriate only if the desired architecture explicitly requires a post-envelope module output level.

This is a recommendation, not approval or implementation.

## 6. Verification evidence and remaining gates

- Source locations: parameter declaration in `P`; knob creation in the LOW-PASS GATE module; audio node creation and connection in `wcInit`; knob dispatch in `wcUpdateParam`; envelope scheduling in `wcTrig`; `wcHold` helper.
- No parameter IDs, engine ABI, DSP source, UI, project/preset storage, sidecar, Flash allocation, package or device state changed.
- CP1.8 closes the source ambiguity but leaves the user-facing V1 behavior choice open.


## 7. Approved V1 behavior — Option C (2026-10-31)

The user approved **Option C — envelope-scaled LPG peak**.

- Preserve one LPG gain stage.
- Compute the trigger peak as `(0.25 + RESPONSE × 0.75) × MODULE VOLUME`.
- MODULE VOLUME scales the per-trigger peak; it is not a post-envelope gain node.
- RESPONSE continues to define the peak contour and opening attack; DECAY continues to define the tail.
- This intentionally departs from the v98 shared-AudioParam overwrite behavior.
- Isolated Q15 helper: `firmware/src/eng_metal_core_math.h`, function `eng_metal_lpg_peak_q15()`.
- Host test: `tests/metal_core_lpg_test.c`; runner integration is part of CP2.2.
- The helper is not connected to `eng_metal_core.c` and does not make the engine playable or change registration, IDs, UI, storage, package, or device state.
