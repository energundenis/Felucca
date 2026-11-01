# ENERGUN METAL CORE — CP3.1 fixed-point biquad prototype 01

Date: 2026-11-01  
Branch: `energun/cp3-1-biquad-prototype-01`  
Base: `daae06e84c5710e0ed5703c5af38afde927b1ed8`  
Status: isolated host prototype; **not production DSP, not registered, and not a frozen engine-state ABI**.

## 1. Scope

This prototype tests the DF2T candidate from `ENERGUN_METAL_CORE_CP3_1_FIXED_POINT_BIQUAD_CANDIDATE_01.md`. It is deliberately confined to `tests/` and does not alter firmware source, parameter maps, persistence, engine registration, the host `voice_t` ABI, or factory firmware.

The protected v98 reference has two serial and independent low-pass filters: MAIN FILTER and LPG FILTER. They require separate histories. This prototype implements one mono filter instance; two filters use two independent state instances in the host test.

## 2. Numeric candidate

The first Q3.29 proposal is superseded for this prototype by **Q2.30 coefficients**:

- Five signed int32 coefficients: `b0,b1,b2,a1,a2`, each in Q2.30.
- Two signed int32 DF2T history states: `z1,z2`, in the same signal scale as input/output.
- Signed 64-bit products and intermediates.
- Round-to-nearest, ties away from zero for coefficient multiplication.
- Explicit int32 saturation at output and both state updates; host tests count saturation.
- Coefficients are generated only in the host test using an RBJ low-pass formula and compared with a double-precision oracle. No floating point or coefficient generation is present in the candidate sample function.

Q2.30 is selected because normalized `a1` can approach -2; Q1.31 cannot represent that range. The signed Q2.30 int32 range is [-2, 2).

## 3. Host regression evidence

Strict local build:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror \
  -o build/metal_core_biquad_candidate_test \
  tests/metal_core_biquad_candidate_test.c -lm
```

Result: **PASS, 10 checks, 0 failures**.

Coverage:
- Candidate coefficient and state sizes: 20 bytes and 8 bytes.
- Jury stability checks for 28 quantized cutoff/Q combinations.
- 28 impulse responses over 65,536 samples, no saturation, and tail peak below 1,024 LSB.
- 84 sine comparisons against a double-precision recurrence oracle, no saturation.
- Separate MAIN and LPG histories; resetting MAIN leaves LPG history unchanged.
- DC step converges to unity gain without saturation.
- Reset clears both history words.
- An over-range state is explicitly saturated and counted.

Measured worst-case differences for the current test vectors:
- Impulse: 4,763.141 LSB.
- Sine: 5,190.164 LSB.

The vectors use a 5,000,000-unit input amplitude, so the worst measured sine error is approximately 0.104% of that amplitude. The acceptance threshold in this prototype is 0.15%; this is a preliminary numeric gate, not a perceptual or sound-quality approval.

The 100 Hz / Q=20 case is the most demanding low-cutoff/high-resonance corner. A small integer residual remains after the long impulse tail; the test bounds the final tail rather than requiring exact zero, which would be an unrealistic fixed-point condition.

## 4. Important limitations

- This is a host-only test candidate, not the production filter.
- The RBJ coefficient generator uses host double precision; target-side coefficient preparation has not been designed.
- The candidate uses a sample-level process function; block-rate coefficient updates and the source's 10/20/25 ms automation trajectories are not implemented.
- No MAIN/LPG dual-instance automation, trigger cancel-and-hold, Accent trajectory or retrigger policy is implemented.
- No target compiler output or callback cycle measurement has yet been accepted.
- Input headroom from the full OSC → FM/METAL → FOLD chain is not frozen. The 5,000,000-unit vectors do not prove full-engine headroom.
- No filter state memory total is approved. The 8-byte per-filter state remains a DF2T candidate; the two filters must remain independent.

## 5. Next gates

1. Run strict host regression in GitHub Actions.
2. Compile the sample and block-32 probes with the pinned AC79 pi32v2 compiler; inspect disassembly and arithmetic helper calls.
3. Compare target code size and block cost before deciding whether the arithmetic is viable.
4. Add MAIN/LPG automation tests and source-derived trigger timing tests.
5. Revisit coefficient precision / low-frequency high-Q accuracy if target or audio-reference comparisons expose unacceptable error.
6. Only after numeric, timing, memory and lifecycle gates pass may the filter be considered for production integration.

No production DSP, engine registration, parameter IDs, persistence changes, graph connection, hardware access, device write or flash was performed.
