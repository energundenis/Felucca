# ENERGUN METAL CORE — Felucca voice lifecycle audit 03

Date: 2026-10-31
Branch: energun-metal-core
Status: source-backed integration audit; no source code changed.

## 1. Host engine contract

The Felucca-Engines interface (firmware/engines/src/felucca_engine.h, submodule hugelton/Felucca-Engines) confirms:
- note_on(t, v) is called for a voice start or retrigger.
- render(t, v, out, n, m) adds samples to the output; it must not overwrite the host mix.
- vmod_t supplies block-level pitch, amplitude ramp (amp0 to amp1), cutoff, shape, and envelope value.
- amp() is an optional engine amplitude-curve hook; it receives the already calculated host ADSR value and returns Q15 amplitude.
- ownenv=1 replaces host ADSR and velocity with full-scale amplitude; the engine must implement its own voice-ending condition through done().
- ownenv=0 (the default) keeps the host ADSR and velocity behavior.
- keep selects per-voice state slots preserved over a retrigger.
- mem reserves bounded per-part engine state, shared by the one engine currently active on that part.

## 2. Metal Core V1 envelope decision

The v98 wcTrig() source schedules a dedicated VCA envelope on every sequencer trigger:
- peak = wcLevel * velocity * accentGain;
- sustain = wcLevel * wcSus * velocity * accentFactor;
- attack, decay, and release are scheduled with setTargetAtTime;
- the envelope is cancelled/held at the trigger time before new segments are scheduled.

Felucca host ADSR:
- starts attack on a fresh voice start;
- attack rises linearly to full scale;
- decay approaches sustain exponentially;
- release approaches zero exponentially;
- mono legato can retain the current envelope rather than restarting it;
- velocity is applied by the host to the envelope amplitude.

The host ADSR is structurally suitable as the first V1 amplitude-envelope implementation, but it is not mathematically identical to the v98 Web Audio VCA envelope. In particular, the attack curve, time-constant mapping, retrigger/legato transitions, and velocity/accent handling must be compared and calibrated. Do not claim bit-exact or curve-exact parity.

Integration recommendation:
- Start with ownenv=0 so the host ADSR and velocity remain active.
- Do not set ownenv=1 merely because Metal Core contains an LPG or FM-index envelope.
- Keep LPG gain/filter envelope, Complex FM-index envelope, and METAL-specific decay separate from the host amplitude envelope.
- Use engine_t.amp only if a measured, justified correction is needed; it must consume the ADSR value rather than replace unrelated modulation envelopes.
- Confirm the final V1 note/retrigger/legato policy against the v98 reference before implementation is considered sound-faithful.

## 3. Output level decision

P_LEVEL is applied by the host mixer after engine rendering and track processing. It is therefore a useful final track mix level, but it is not equivalent to the v98 per-trigger wcLevel VCA peak/sustain control.

Keep these functions distinct:
- host P_LEVEL: final track output level;
- Metal Core voice VCA level/envelope: per-note amplitude shaping;
- OSC1 / OSC2 / folder / LPG module levels: internal voice signal-path gains.

Do not map the v98 voice VCA level directly to P_LEVEL.

## 4. Note lifecycle and state

- note_on must initialize the engine's per-voice state for a fresh note and handle retrigger without clicks.
- Felucca's host restores selected phase/state slots after note_on on a sounding-voice retrigger according to engine_t.keep; the Metal Core implementation must explicitly choose which states persist.
- A normal ADSR-controlled Metal Core voice should not need ownenv or done() to end its voice; host release owns voice lifetime.
- Any per-voice oscillator phases, filter memories, LPG state, and feedback delay state must fit the documented voice slots or bounded engine_t.mem; memory ownership and retrigger rules must be explicit.
- The host renders in blocks of CTL samples (32); the engine render callback must support n <= CTL and add to out[].

## 5. Parameter mapping consequences

| Control | Integration conclusion |
|---|---|
| P_ATK / P_DEC / P_SUS / P_REL | Keep host ADSR for V1 initially; calibrate against v98; do not use for FM ENV, LPG DECAY, or METAL DECAY |
| P_LEVEL | Keep as final host track level; separate from v98 voice VCA level |
| P_DETUNE | Remains host UNISON spread; not OSC2 detune |
| engine_t.ownenv | Leave 0 for normal host ADSR-controlled voice |
| engine_t.amp | Optional ADSR-shaping hook only; not a substitute for LPG/FM envelopes |
| engine_t.done | Not required for normal ADSR release lifecycle |
| engine_t.keep | Must be chosen per state slot after retrigger tests |

## 6. Next step

Complete the full V1 control inventory and parameter budget:
- count controls that need independent stored values;
- decide which eight can occupy E0..E7;
- determine which common IDs are genuinely available before P_E0 while preserving P_COUNT <= 127;
- retain the previously approved sidecar approach for any additional shared/per-track values and motion events;
- reconcile project, preset, backup, editor, and motion-lock behavior before allocating IDs.

No firmware source, parameter IDs, UI, binary image, factory firmware, or physical device were changed.
