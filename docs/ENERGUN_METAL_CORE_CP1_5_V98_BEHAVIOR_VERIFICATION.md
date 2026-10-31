# ENERGUN METAL CORE — CP1.5 v98 behavior verification

Date: 2026-10-31
Branch: energun-metal-core
Reference inspected: protected Library file `energun_westcoast_synth_v98_metal_core_follow_metal_on.html`.
Status: source-level behavior audit; no runtime firmware or DSP changed.

## 1. WEST COAST VOLUME — resolved

The v98 source contains three assignments to `wcOut.gain`:
- Audio graph initialization: `P.wcVol.v * 12`.
- Knob handler for `wcVol`: `P.wcVol.v * 12`.
- Persistent `wcVolLoop()` scheduled with `requestAnimationFrame`: `P.wcVol.v * 5`.

The loop runs continuously and overwrites the other two assignments. The stable runtime output gain is therefore **VOLUME × 5**, not ×12. The ×12 value is a transient initialization/interaction assignment and is not the effective steady-state calibration.

V1 reference target: use a single deterministic output-gain mapping calibrated to the stable v98 behavior (×5 in the browser reference), with no competing writer. Verify fixed-point headroom and clipping independently; do not copy the transient ×12 assignment.

## 2. FM CHARACTER — resolved with UI initialization caveat

The v98 parameter object initializes `wcFmCharacter.v = 2` (METALLIC).
The selector markup marks `CLASSIC` (value 1) as selected.
During `wcComplexSync()`, the selector is assigned `String(P.wcFmCharacter.v)`; this sync occurs during audio initialization.

Consequences:
- Internal DSP default is **METALLIC (2)**.
- Before audio initialization, the static HTML selector visually displays CLASSIC (1).
- After audio initialization, the selector is corrected to METALLIC (2).
- The actual FM index curves are: SMOOTH = `0.72 * sqrt(x)`; CLASSIC = `x`; METALLIC = `0.55*x + 1.15*x*x`, with x clamped 0..1.

V1 reference target: METALLIC default. Initialize UI and DSP from one canonical value so there is no pre-audio mismatch.

## 3. LPG MODULE VOLUME — not an independent post-envelope gain

The knob handler writes `P.wcLpgVolume.v` to `wcLpgGain.gain`.
The per-step trigger code also writes the LPG transient envelope to that same `wcLpgGain.gain` AudioParam, scheduling its peak and decay. The knob and envelope therefore share one gain node and can overwrite/compete with each other's automation.

Consequences:
- The stored control exists in v98 and remains listed in the 37-value reference inventory.
- It is not a clean independent post-envelope module-volume stage.
- A separate post-envelope multiplier in V1 would change the reference architecture and needs an explicit sound-design decision.
- For faithful V1, preserve the shared-node behavior or approve a deliberate redesign after listening tests. Do not silently reinterpret it as an independent gain stage.

## 4. VCA / ADSR parity — host reuse not approved

The v98 sequencer trigger explicitly schedules its own amplitude path:
- Normal mode: ATTACK 10 ms, DECAY 300 ms, SUSTAIN 0, RELEASE 200 ms by default.
- METAL mode: ATTACK 2 ms and METAL DECAY 400 ms; RELEASE remains 200 ms.
- VCA peak is separately scaled by VOICE VOLUME × step velocity × accent factor.
- Gate duration is derived from the sequencer's sixteenth-note interval.
- LPG and Complex FM-index envelopes are additional independent trigger schedules.

Felucca host ADSR is a shared per-voice lifecycle envelope. It also participates in host voice allocation, retrigger/legato, velocity and note release. The host ADSR parameter defaults and scaling are not numerically identical to the v98 controls; the host release/lifetime behavior must not be conflated with the v98 internal LPG or FM envelopes.

Decision for the V1 architecture proposal:
- **Do not reuse P_ATK/P_DEC/P_SUS/P_REL as a drop-in replacement at this stage.**
- Keep the four v98 normal ADSR values and the VCA peak as Metal Core's own control state unless a later controlled golden-render comparison proves parity.
- Continue using Felucca's host voice lifecycle to allocate/release voices; the Metal Core internal amplitude contour must not own or prolong host voice lifetime.
- METAL's 2 ms attack and METAL DECAY selection are internal trigger behavior, not a mutation of the host's global ADSR.

This leaves the proposed private bank at **13 values**, not 9, pending any future parity test.

## 5. Updated storage proposal implications

- E0..E7 proposal unchanged.
- The 16 proposed common P_MC controls remain a candidate only; no IDs are assigned.
- Private bank remains 13 controls because ADSR host reuse is not approved.
- LPG MODULE VOLUME remains a stored value, but its v98 semantics are shared with the LPG envelope; it must not be documented or implemented as a separate post-envelope gain without approval.
- WEST COAST VOLUME stable reference scale is ×5.
- FM CHARACTER effective default is METALLIC (2).

## 6. Remaining gate

Before CP1 can close:
1. Approve the 37 stored values and the 13-value private-bank interpretation.
2. Decide whether LPG MODULE VOLUME should preserve the shared-node behavior or intentionally become a separate gain stage.
3. Freeze the detailed V1 signal tap/gain map, including the METAL ring tap and output gain.
4. Finish sidecar format, project/preset/archive association, A/B recovery and power-loss design.
5. Keep flash-sector allocation blocked until the full FM-1 V1.5 device dump is audited.

No parameter IDs, engine ABI, project format, storage, flash, firmware package or device state changed.
