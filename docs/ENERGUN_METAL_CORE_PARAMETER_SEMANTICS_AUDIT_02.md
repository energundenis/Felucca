# ENERGUN METAL CORE — Felucca parameter semantics audit 02

Date: 2026-10-31
Branch: energun-metal-core
Status: source-backed semantic audit; no source code changed.

## 1. P_LEVEL — confirmed host track-output level

Source:
- firmware/src/params.c: P_LEVEL is F_DB, range 0..127, default 104.
- firmware/src/fx.c: mix_part() and mix_spread() read LEVEL_Q12[P_LEVEL] after track_render(), track_dist(), the track INSERT, SLICER, and performance mute handling.
- The scaled signal is then sent to dry left/right output and to chorus/delay/reverb sends.

Conclusion:
P_LEVEL is a host track mix level, not an oscillator level, voice VCA, LPG module volume, or wavefolder output control. It may remain the final Felucca track-level control for the engine, but it must not replace any internal v98 gain control. Validate the level curve and gain staging against v98 before claiming an exact level match.

## 2. P_ATK / P_DEC / P_SUS / P_REL — confirmed host amplitude envelope

Source:
- firmware/src/params.c: ATK/DEC/REL are F_TIME 0..127; SUS is F_PCT 0..127.
- firmware/src/voice.c: esync_coef() selects ENV_LIN for attack and ENV_EXP for decay/release when ENV SYNC is OFF; ENV SYNC ON derives coefficients from tempo note values.
- env_tick() uses a linear rising attack toward full scale, exponential decay toward sustain, and exponential release toward zero. Sustain is a fixed Q24 target from P_SUS.
- voice_start() starts the envelope on a fresh note; mono legato can retain the existing envelope rather than retriggering it.
- track_render() calculates the host envelope and velocity amplitude, then allows engine-specific e->amp processing. If e->ownenv is set, host ADSR is not used as the engine's amplitude envelope: host env is forced to full scale and the engine's own envelope/done logic owns the voice lifetime.

Conclusion:
The four controls are a coherent host ADSR and are candidates for the normal voice amplitude envelope only if the Metal Core engine is registered to use the host envelope and v98 note/retrigger/legato behavior is shown to match. They are not substitutes for:
- Complex FM index envelope (FM ENV AMT / ATTACK / DECAY);
- LPG transient envelope and LPG DECAY;
- METAL-specific decay behavior.
Do not set ownenv merely to implement LPG or FM-index modulation. Those are separate modulation/envelope paths and should not take ownership of voice lifetime.

ENV SYNC is a host-wide behavior on ATK/DEC/REL. It is not part of the v98 ADSR itself and should not be silently imposed on Metal Core unless explicitly supported and tested.

## 3. P_DETUNE — confirmed UNISON voice spread

Source:
- firmware/src/params.c: integer 0..127, default 40.
- firmware/src/voice.c: mono_play() applies P_DETUNE only when more than one UNISON voice is active. It distributes the voices symmetrically using v->fine, with the outer voices reaching approximately ±40 cents at the maximum.
- For a single voice, the parameter does not apply.

Conclusion:
P_DETUNE is not OSC2 DETUNE. Do not reuse it for Metal Core's OSC2 tuning. If Metal Core supports Felucca UNISON, P_DETUNE may continue to serve its existing host-level UNISON spread semantics independently of any engine-specific OSC2 detune.

## 4. Parameter reuse status

| Felucca parameter | Metal Core V1 use | Status |
|---|---|---|
| P_LEVEL | Final host track mix level | Reuse host function; not an internal module level |
| P_ATK / P_DEC / P_SUS / P_REL | Normal voice amplitude envelope | Conditional reuse; verify v98 note/retrigger/legato behavior and host envelope selection |
| P_DETUNE | Host UNISON voice spread only | Keep host semantics; do not map to OSC2 DETUNE |
| P_FTYPE | None | Not a cutoff/resonance control |
| LFO / modulation matrix | None in initial V1 mapping | Deferred; do not assume patch-bay equivalence |

## 5. Remaining checks before assigning IDs

1. Inspect engine_t flags and voice lifecycle contract for the new engine, especially ownenv, amp, note_on, done, and keep.
2. Confirm the normal v98 voice envelope behavior from the v98 source, including retrigger and legato semantics.
3. Produce a complete list of V1 engine-specific controls and count the independent parameter IDs.
4. Confirm editor pages and persistence constraints against P_COUNT <= 127 and the E0..E7 edit-value mechanism.
5. Only then propose a stable parameter ID allocation.

No source code, parameter IDs, editor pages, firmware image, factory firmware, or physical device were changed.
