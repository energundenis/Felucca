# ENERGUN FM-1 CORE — CP3 DSP/filter workstream

Base: 77c704bd8ef58e2fa03070fd4fefe3bae59e6d3c
Status: launched; host-only qualification; no production DSP approval.

## Objective
Qualify the proposed mono DF2T low-pass biquad for the independent MAIN FILTER and LPG FILTER nodes.

## Work
1. Implement a host-only candidate with Q3.29 coefficients, bounded int64 intermediates and explicit int32 saturation.
2. Generate coefficients only in the host test/reference harness initially; keep floating point out of the eventual target render path.
3. Compare impulse, DC, frequency response and cutoff/Q sweeps against a double-precision oracle.
4. Exercise Q 0.5–20, cutoff corners, short/long automation and independent filter histories.
5. Count all saturation events; valid-range vectors must have zero unintended saturation.
6. Record coefficient quantization, pole stability, limit-cycle/tail behavior and CPU operation counts.

## Acceptance gates
- Deterministic host tests pass under strict warnings and sanitizers.
- No unstable poles or unexplained response error at approved parameter corners.
- No production include, registration or render callback until numerical limits and signal headroom are reviewed.
- AC79 target code-generation probe is a separate follow-up; compile-only evidence is not cycle timing.

Do not change engine IDs, host ABI, parameter IDs, persistence, main branch or device state.
