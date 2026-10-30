# ENERGUN METAL CORE — ADSR compatibility audit 01

Date: 2026-10-31
Branch: energun-metal-core
Status: source-backed design audit; documentation only. No firmware source or device changes.

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

The Felucca ADSR is structurally usable as the first V1 normal voice envelope, but it is not curve-identical to v98.

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

## 5. V1 decision

- Keep the host ADSR as the provisional normal amplitude-envelope path (ownenv=0) for the first integration prototype.
- Treat this as an approximation until measured against v98, not as exact parity.
- Keep FM ENV, LPG envelope/decay, and METAL DECAY independent; never map them onto P_ATK/P_DEC/P_SUS/P_REL.
- Keep P_LEVEL as the final host track output level, not the v98 per-note VCA level.
- Do not enable ENV SYNC by default for Metal Core.
- Before implementation is accepted, render and compare the same note/gate/velocity/accent/retrigger/legato cases in v98 and the Felucca emulator.
- If the host envelope cannot meet the required musical behavior, evaluate a dedicated Metal Core VCA envelope in a separate design review. Do not switch to ownenv=1 without a focused audit of the host's active/gate/release lifecycle and done() semantics.

## 6. Required comparison matrix

| Test | v98 reference behavior to capture | Felucca behavior to compare |
|---|---|---|
| Fresh note | Attack to peak | Host linear attack |
| Short gate | Release begins at sequencer gate | Host note-off release |
| Long gate | Sustain level after decay | Host sustain level |
| Retrigger while sounding | Hold current VCA gain, reschedule segments | Host retrigger attack from current envelope |
| Mono legato | New pitch / trigger behavior from sequencer | Host legato may retain envelope |
| Velocity | Peak and sustain scaling | Host velocity multiplication |
| Accent | 1.18 peak and 1.08 sustain factors in v98 | Host velocity/accent path |
| METAL ON | 2 ms attack + METAL DECAY | Host ADSR plus any separately designed engine behavior |
| ENV SYNC | No equivalent in v98 | Must remain off unless explicitly accepted |

No firmware source, parameter IDs, UI, binary image, factory firmware, or physical device were changed.
