# ENERGUN METAL CORE — v98 implementation notes 01

Date: 2026-10-31
Source: Library file energun_westcoast_synth_v98_metal_core_follow_metal_on.html
Status: source observations to verify during port; no changes to the reference file.

## 1. METAL at the reference DRIVE value

The Complex FM amount and the METAL FM amount both use wcDrv(), where wcDrv() = WEST COAST DRIVE ^ 1.5. The reference WEST COAST DRIVE is 0. Therefore the METAL FM modulator path has zero depth at the reference setting, even though METAL is ON. The METAL ring-mod path is still connected and active. Preserve the distinction between an enabled path and its current modulation depth.

## 2. LPG MODULE VOLUME shares the envelope gain node

The v98 LPG uses wcLpgGain both as the LPG output gain control and as the per-trigger LPG envelope gain. The MODULE VOLUME handler writes that gain directly, while each trigger schedules peak/tail values on the same AudioParam. It is not implemented as a separate post-envelope volume stage.

A fixed-point port must not silently assume this is an independent multiplier. Either reproduce the reference behavior or propose a separate volume stage as an intentional behavior correction for approval.

## 3. WEST COAST VOLUME has two competing assignments

The direct knob handler assigns wcOut.gain = VOLUME × 12, while the recurring wcVolLoop assigns wcOut.gain = VOLUME × 5. The recurring loop therefore overrides the knob handler's scale. This is a reference-code inconsistency; do not choose one scale by guesswork. Calibrate the intended output gain against the protected v98 sound before implementation.

## 4. SPREAD routing needs verification

The v98 code creates wcPanA and wcPanB and updates both from SPREAD. The inspected main signal graph routes the post-wavefolder path through wcPanA; an independent audible wcPanB path is not established in that graph. Do not promise stereo OSC1/OSC2 spread until the complete routing is verified.

## 5. Scope

These are observations about the browser reference implementation, not instructions to reproduce accidental bugs automatically. The default policy is to preserve intended audible behavior, identify any code inconsistencies, and request approval before changing a behavior that could alter the established Metal Core sound.
