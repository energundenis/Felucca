# ENERGUN METAL CORE — CP1.6 detailed signal tap and gain map

Date: 2026-10-31
Branch: energun-metal-core
Reference inspected: protected Library file `energun_westcoast_synth_v98_metal_core_follow_metal_on.html`.
Status: source-level signal-flow audit; DSP implementation and engine registration remain blocked.

## 1. Main oscillator and folder path

The detailed v98 audio graph is not just the simplified OSC → FM → FOLD → LPG chain.

Primary OSC1 path:
1. OSC1 oscillator.
2. Complex FM frequency modulation input; AM mode is applied through the Complex amplitude path.
3. Complex amplitude node.
4. COMPLEX MODULE VOLUME.
5. Shared TIMBRE input gain (`1 + TIMBRE × 6`).
6. Fixed first reflection shaper (four reflections over its source input range).
7. Adjustable second wavefolder transfer (SOFT / CLASSIC / HARD, amount, symmetry, drive; Web Audio reference uses 4× oversampling).
8. WAVEFOLDER MODULE VOLUME.
9. Pan A.
10. Main low-pass filter.

OSC2 direct-audio path:
1. OSC2 oscillator with its own detune.
2. OSC2 LEVEL.
3. Joins at the shared TIMBRE input gain, before both folder stages.
4. It does not pass through COMPLEX MODULE VOLUME; that gain node is only on the OSC1/Complex path.
5. It does pass through the two folder stages and WAVEFOLDER MODULE VOLUME.

Complex FM modulator is separate from OSC2. METAL FM modulator is also separate. Do not merge their phases, ratios, envelopes or controls.

## 2. METAL path and tap point

- METAL FM modulator (`wcMMod`) connects through its index gain to OSC1 frequency.
- METAL ratio sets both METAL FM modulator and METAL ring oscillator frequencies from the tracked base frequency.
- METAL ring oscillator (`wcRingOsc`) drives a gain AudioParam through `wcRingDepth`.
- With METAL ON, `wcRingDepth` is connected to `wcRingNode.gain`; the node's static gain is set to zero, so the ring oscillator modulates the post-VCA signal amplitude.
- With METAL OFF, the ring modulation connection is removed, the node gain returns to one, and METAL FM index returns to zero.
- The METAL ring modulation tap is after the VCA and before WEST COAST VOLUME.

This is not a second oscillator mixed before the folder. Preserve it as a post-VCA amplitude-modulation path.

## 3. Main filter, LPG, VCA and output

After the folder path:
1. Main low-pass filter (`wcFilt`): CUTOFF and RESONANCE.
2. LPG low-pass biquad (`wcLpgFilter`): initial 1800 Hz, Q 0.7; trigger transient raises cutoff based on RESPONSE, then returns toward main cutoff.
3. LPG gain/envelope (`wcLpgGain`): RESPONSE determines transient peak and attack; DECAY controls its tail. LPG MODULE VOLUME writes to this same gain AudioParam, so it is not an independent post-envelope gain.
4. Voice VCA (`wcVCA`): peak from VOICE VOLUME × velocity × accent; normal ADSR or METAL-specific trigger attack/decay and shared release.
5. METAL ring amplitude modulation node (`wcRingNode`).
6. WEST COAST VOLUME gain (`wcOut`), stable v98 multiplier ×5.
7. Six-band EQ (outside the 37-control Metal Core inventory).
8. Host master input / effects / stereo output.

The host voice lifecycle must remain separate from these internal envelopes. The internal VCA/LPG must not keep a Felucca voice active beyond the host's note lifecycle.

## 4. Deferred branch

RING2 is a separate branch:
- Stage-1 shaper tap feeds the RING2 wet path.
- The folder output feeds RING2 dry path through Pan A.
- RING MIX and RING RATIO control that branch.
- It rejoins at the main filter input.

RING2 remains deferred from initial V1. Do not confuse it with the METAL ring path, which is post-VCA and uses METAL RATIO.

## 5. Gain-stage audit and implementation requirements

- COMPLEX MODULE VOLUME is OSC1/Complex-path-only.
- OSC2 LEVEL is applied before the shared TIMBRE/folder path.
- TIMBRE is a pre-stage-1 input gain, not a final volume.
- WAVEFOLDER MODULE VOLUME is after the adjustable second folder and before the main filter.
- LPG MODULE VOLUME shares the LPG envelope node.
- VOICE VOLUME is the internal VCA peak control, not host P_LEVEL.
- WEST COAST VOLUME is after VCA/METAL ring and before EQ; stable v98 scale ×5.
- Host P_LEVEL remains the track mixer level after engine render.

V1 fixed-point implementation must establish explicit headroom at every stage, clamp or saturate with measured behavior, and test:
- OSC1-only and OSC2-only gain paths;
- simultaneous OSC1/OSC2 without unintended level doubling;
- worst-case TIMBRE and folder drive;
- FM and METAL FM maximum depth;
- LPG/VCA multiplication;
- METAL ring modulation peak;
- output ×5 and host P_LEVEL;
- DC offset, clipping, overflow, and denormal-equivalent low-level behavior.

Do not copy browser floating-point transfer curves and assume they are fixed-point equivalent. The 4× wavefolder oversampling needs a separately designed alias-aware target implementation.

## 6. CP1.6 conclusion

The v98 detailed signal graph and the major gain/tap positions are now documented. Remaining approval decisions:
1. Preserve or redesign the shared LPG volume/envelope node.
2. Confirm RING2 remains deferred.
3. Approve the 37 stored controls and 13-value private bank.
4. Freeze the exact signal map and gain staging for DSP implementation.
5. Complete sidecar/recovery design and full device-dump audit before Flash allocation.

No parameter IDs, DSP code, engine registration, ABI, project storage, sidecar, Flash, firmware package or physical device state changed.
