# ENERGUN METAL CORE — ADSR compatibility audit 01

Date: 2026-10-31
Branch: energun-metal-core
Status: historical source-backed design audit; its provisional V1 recommendation is superseded by CP2.19.

> **SUPERSEDED DECISION:** Section 5's provisional `ownenv=0` recommendation is no longer valid. The later user-approved decision in `ENERGUN_METAL_CORE_CP2_19_VCA_PERFORMANCE_MODE_DECISION.md` requires the dedicated METAL CORE VCA contour for both live MIDI and sequencer performance, with `ownenv=1`. Do not implement the provisional host-ADSR path described below.

## 1. Reference v98 amplitude envelope

The v98 reference source defines:
- ATTACK: 0.001..1.0 s, step 0.001, default 0.01 s.
- DECAY: 0.05..3.0 s, step 0.01, default 0.30 s.
- SUSTAIN: 0..1, step 0.01, default 0.
- RELEASE: 0.01..3.0 s, step 0.01, default 0.20 s.
- VOICE VOLUME: 0..1, default 0.5.

At each sequencer trigger, wcTrig():
- chooses ordinary ATTACK / DECAY, unless METAL is enabled;
- with METAL enabled, forces ATTACK to 0.002 s and uses METAL DECAY (default 0.4 s);
- computes peak from VOICE VOLUME × velocity × accent gain;
- computes sustain from VOICE VOLUME × SUSTAIN × velocity × accent factor;
- holds the current VCA gain at trigger time, then schedules attack, decay and release;
- schedules RELEASE from the sequencer gate time.

The Web Audio setTargetAtTime time constants are approximately ATTACK/3, DECAY/3 and RELEASE/3. This creates exponential approaches, not a linear attack. A retrigger starts from the held current gain rather than unconditionally resetting to zero.

## 2. Felucca host ADSR

Felucca stores the common ADSR controls as 7-bit values:
- P_ATK: F_TIME, 0..127, default 10.
- P_DEC: F_TIME, 0..127, default 70.
- P_SUS: F_PCT, 0..127, default 90.
- P_REL: F_TIME, 0..127, default 60.

When ENV SYNC is OFF:
- attack uses ENV_LIN[P_ATK] and rises linearly in control ticks;
- decay uses ENV_EXP[P_DEC] and approaches sustain exponentially;
- release uses ENV_EXP[P_REL] and approaches zero exponentially.

When ENV SYNC is ON, attack/decay/release coefficients are calculated from tempo note values. ENV SYNC is a host feature, not a v98 reference control.

Lifecycle:
- a fresh voice start resets the host envelope to zero and starts attack;
- a sounding-voice retrigger begins the host attack from the current envelope level;
- mono legato without retrigger keeps the existing envelope;
- note-off changes the voice to release stage;
- velocity is multiplied into the host amplitude after the envelope calculation.

## 3. Compatibility assessment

The Felucca ADSR is structurally usable as a generic host amplitude envelope, but it is not curve-identical to v98.

Known mismatches:
1. Attack shape: host linear rise vs v98 exponential target approach.
2. Time representation: host 0..127 lookup-table coefficients vs v98 seconds / Web Audio time constants. There is no valid direct numeric mapping.
3. METAL behavior: v98 forces a 2 ms attack and substitutes METAL DECAY. The host ADSR does not do this automatically.
4. Legato: host mono legato may retain the current envelope; v98 sequencer triggers explicitly hold and reschedule the VCA segments.
5. Velocity/accent: both systems scale amplitude, but their exact peak/sustain/accent curves differ and require calibration.
6. ENV SYNC: host tempo-synced ADSR has no direct equivalent in the v98 VCA envelope and must not be silently enabled as a Metal Core default.

## 4. Default-value warning

The host's global defaults (ATK 10, DEC 70, SUS 90, REL 60) are not the v98 reference defaults. In particular, v98 SUSTAIN defaults to zero while the host P_SUS default is 90/127.

The Metal Core engine must not assume that selecting the engine automatically gives the correct v98 envelope defaults. Any engine-specific default initialization or preset override must be designed without changing other Felucca engines' global defaults or unexpectedly overwriting a user's existing track settings.

For the reference patch with METAL ON, the effective v98 trigger values are ATTACK 0.002 s and METAL DECAY 0.4 s; ordinary ATTACK 0.01 s and DECAY 0.30 s are stored but bypassed while METAL is on.

## 5. Historical provisional V1 recommendation — superseded

The original audit provisionally suggested:
- using the host ADSR for the first normal-voice integration prototype (`ownenv=0`);
- treating it as an approximation until measured against v98;
- keeping FM ENV, LPG envelope/decay, and METAL DECAY independent;
- keeping P_LEVEL as the final host track output level;
- leaving ENV SYNC disabled by default;
- comparing note/gate/velocity/accent/retrigger/legato cases before acceptance.

**Disposition:** this provisional recommendation was explicitly superseded by CP2.19. The current approved contract is:
- `ownenv=1`; host ADSR is bypassed for METAL CORE;
- every live MIDI and sequencer performance note uses the dedicated internal VCA Attack → Decay → Sustain → Release contour;
- MIDI Note Off is applied at the first render-block boundary currently exposed by the host API, not at a sample-accurate sub-block offset;
- retrigger holds the instantaneous VCA level;
- expressive MIDI velocity must not be treated as sequencer Accent without a separate approved mapping;
- host LFO-to-AMP and engine-switch fade remain independent multiplicative controls and must be applied exactly once.

See `ENERGUN_METAL_CORE_CP2_19_VCA_PERFORMANCE_MODE_DECISION.md` and `ENERGUN_METAL_CORE_CP2_20_EXPLICIT_VCA_GATE_OFF.md` for the current product decision and isolated Gate-off helper boundary.

## 6. Historical comparison matrix

| Test | v98 reference behavior to capture | Host behavior / current integration requirement |
|---|---|---|
| Fresh note | Attack to peak | Dedicated internal VCA attack |
| Short gate | Release begins at sequencer gate | Internal VCA Gate-off; block-boundary host event for MIDI |
| Long gate | Sustain level after decay | Internal VCA sustain |
| Retrigger while sounding | Hold current VCA gain, reschedule segments | Internal VCA cancel-and-hold |
| Mono legato | New pitch / trigger behavior from sequencer | Verify host callback lifecycle and VCA policy |
| Velocity | Peak and sustain scaling | Dedicated VCA velocity input; no host ADSR multiplication |
| Accent | 1.18 peak and 1.08 sustain factors in v98 | Accent provenance remains a separate open mapping |
| METAL ON | 2 ms attack + METAL DECAY | Internal VCA applies approved METAL timing |
| ENV SYNC | No equivalent in v98 | Must remain off unless separately accepted |

No firmware source, parameter IDs, UI, binary image, factory firmware, or physical device were changed by this historical audit.
