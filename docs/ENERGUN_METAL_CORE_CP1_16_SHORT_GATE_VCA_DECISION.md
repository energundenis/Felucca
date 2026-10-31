# ENERGUN METAL CORE — CP1.16 short-Gate / long-Attack VCA rule

Date: 2026-10-31
Branch: energun-metal-core
Status: user decision accepted; implementation and regression test pending.

## 1. Decision

The user selected **Option B** for the V98 short-Gate edge case.

Preserve the V98 VCA character and its one-pole Attack / Decay / Release response, but prevent a delayed Decay segment from reopening the VCA after Release has begun.

When the Gate-off / Release event occurs before the scheduled Decay segment start:

- Release takes priority at the Gate-off sample/time.
- The pending Decay transition is cancelled or suppressed.
- No later Decay target may raise the VCA level during or after Release.
- The VCA remains in Release until it reaches the defined silence threshold; the voice lifecycle must not complete early.
- Retrigger during Release still uses cancel-and-hold semantics and starts a fresh contour from the current level.

If Attack is still active when the Gate ends, Release begins from the instantaneous VCA level at Gate-off; it does not wait for Attack or Decay to finish. This is the safe bounded interpretation of Option B and avoids a post-release reopening.

## 2. Compatibility boundary

This is an intentional correction to the V98 event-order edge case, not a redesign of the normal envelope:

- Normal Gate/Attack/Decay/Sustain ordering remains source-compatible with V98.
- METAL fast Attack (2 ms), METAL DECAY selection, Velocity, Accent, LEVEL, SUSTAIN and RELEASE retain their documented source behavior.
- The Release time constant remains max(5 ms, RELEASE / 3).
- The V98 sustain floor remains 0.0001.
- The internal VCA remains engine-owned; Felucca host ADSR is not cascaded with it.
- LPG and FM-index envelopes remain independent.

## 3. Required implementation behavior

The isolated VCA model must represent event ordering explicitly. At the Gate-off event, it must invalidate any Decay event whose start time is later than Gate-off. The sample-domain implementation must not enqueue or execute stale targets after Release begins.

The implementation must hold the instantaneous envelope value at each event boundary and apply the Release one-pole segment from that value. Use deterministic fixed-point arithmetic; do not introduce floating point or division into the sample loop.

## 4. Required regression coverage

VCA-11 is updated to cover:
1. Gate shorter than max(5 ms, Attack) with METAL OFF.
2. Gate ending during Attack.
3. Gate ending before the scheduled Decay start.
4. Confirmation that the envelope never rises after Release starts.
5. Retrigger during the resulting Release.
6. Normal Gate/Attack/Decay cases remain unchanged.

Acceptance: no post-Release rise, no discontinuity at Gate-off beyond declared fixed-point tolerance, deterministic output, and completion only after the internal VCA reaches its silence criterion.

## 5. Status and safety

- User decision: **Option B accepted**.
- This resolves the short-Gate event-order specification question only.
- Isolated VCA implementation, reference fixtures, host tests, target CPU/RAM measurements and render integration remain pending.
- No firmware code, engine registration, parameter IDs, UI, persistent storage, Flash allocation, package, factory firmware or physical FM-1 state changed.
- The main branch remains untouched. No device write or flash operation is authorized.
