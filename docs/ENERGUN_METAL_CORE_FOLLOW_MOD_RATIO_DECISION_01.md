# ENERGUN METAL CORE — FOLLOW / MOD RATIO decision 01

Date: 2026-10-31
Status: open decision; no behavior selected or implemented.

## 1. Audited v98 behavior

The preserved v98 reference has two different assignments for the wcMod frequency:

1. During initialization and when the panel MOD RATIO control changes:
   `wcMod.frequency = panel FREQ * MOD RATIO`.
2. In the FOLLOW branch of `wcTrig()`, when a note is triggered:
   `wcMod.frequency = tracked note frequency * 1.5`.

Therefore, with FOLLOW enabled, the note-trigger assignment overrides the panel MOD RATIO value with a fixed 1.5 ratio. With FOLLOW disabled, the panel FREQ × MOD RATIO setting remains in control.

A separate part of the v98 feedback graph uses panel FREQ as its feedback reference. That reference is not FOLLOW-tracked note pitch. This distinction is preserved in the current DSP implementation and adapter test.

## 2. V1 choices

### Option A — preserve v98 exactly

- FOLLOW OFF: wcMod = panel FREQ × MOD RATIO.
- FOLLOW ON: wcMod = tracked note × 1.5.
- The MOD RATIO control does not change wcMod ratio while FOLLOW is on.
- Feedback scaling continues to use panel FREQ.

Pros: closest to the accepted v98 Metal Core reference.
Trade-off: MOD RATIO is effectively overridden by FOLLOW note triggers.

### Option B — make MOD RATIO effective under FOLLOW

- FOLLOW OFF: wcMod = panel FREQ × MOD RATIO.
- FOLLOW ON: wcMod = tracked note × MOD RATIO.
- Feedback scaling continues to use panel FREQ, independently of tracked note pitch.

Pros: predictable ratio control while playing sequenced notes.
Trade-off: changes the v98 reference behavior and may change the established METAL CORE character.

## 3. Current recommendation

Do not silently choose either behavior during integration.

If strict v98 sound compatibility is the priority, choose Option A. If the priority is a meaningful MOD RATIO control during FOLLOW performance, choose Option B and treat it as an intentional V1 sound change requiring listening tests.

## 4. Implementation gate

Until a choice is approved:
- keep the audited fixed 1.5 FOLLOW behavior documented;
- keep panel-FREQ feedback reference independent from tracked pitch;
- do not freeze the E0..E7 parameter semantics;
- do not register METAL CORE in `ENGINES[]`;
- do not assign final EDIT labels, defaults, ranges or curves for MOD RATIO.

No firmware image, device query, SysEx, flash write, or hardware change is involved in this decision.
