# ENERGUN METAL CORE — FOLLOW / MOD RATIO decision 01

Date: 2026-10-31
Status: APPROVED — Option A selected by the user on 2026-10-31. Strict v98 behavior is the V1 reference.

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

## 3. Approved V1 decision

**Option A is approved.** Preserve the v98 behavior exactly:
- FOLLOW OFF: wcMod = panel FREQ × MOD RATIO.
- FOLLOW ON: wcMod = tracked note frequency × 1.5.
- MOD RATIO is overridden while FOLLOW is active.
- Feedback scaling remains referenced to panel FREQ, not tracked note pitch.

Option B is not part of V1. Any later change to make MOD RATIO effective under FOLLOW would be a separate sound-design revision and would require a new explicit decision and listening tests.

## 4. Implementation constraints after approval

- Implement FOLLOW ON as a fixed 1.5 ratio from the tracked note frequency.
- Implement FOLLOW OFF as panel FREQ × user MOD RATIO.
- Keep feedback scaling based on panel FREQ in both modes.
- Do not allow MOD RATIO to alter the FOLLOW ON ratio.
- The current adapter's temporary FOLLOW-ON probe now uses 1.5 (Q16 value 98304) and has a regression assertion. The adapter's panel-FREQ input is still a fixed test placeholder, not a real control mapping.
- Final EDIT labels, defaults, ranges, and curves still require separate parameter-map approval before engine registration.

No firmware image, device query, SysEx, flash write, or hardware change is involved in this decision.
