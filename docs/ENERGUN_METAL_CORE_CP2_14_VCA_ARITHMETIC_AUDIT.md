# ENERGUN METAL CORE — CP2.14 VCA arithmetic and state-size audit

Date: 2026-10-31
Branch: `energun-metal-core`
Scope: source-level audit of the isolated sample-domain VCA model. This is not a target benchmark.

## 1. Coefficient approximation

The implementation uses the first-order update

`y[n+1] = y[n] + round((target-y[n]) / tau_samples)`

with the reciprocal represented in Q30. This is the forward-Euler approximation to the exact one-pole coefficient `1 - exp(-1/tau_samples)`.

The remaining error after one time constant, before Q16 per-sample rounding, was evaluated against `exp(-1)`:

| tau (samples) | Approx. remaining | Exact remaining | Relative error |
|---:|---:|---:|---:|
| 220 | 0.3670417383 | 0.3678794412 | -0.22772% |
| 441 | 0.3674619069 | 0.3678794412 | -0.11350% |
| 8,820 | 0.3678599024 | 0.3678794412 | -0.00531% |
| 13,230 | 0.3678638326 | 0.3678794412 | -0.00424% |

At 44.1 kHz, V98's 5 ms minimum time constant rounds to 221 samples (the test uses 220 samples as a direct one-tau arithmetic reference); the discrepancy at this scale remains below 0.23%. Typical 0.2–0.3 s decay/release time constants have much smaller relative error.

Important boundary: the generic helper maps tau=1 sample to coefficient 1 (immediate convergence), which is not an accurate exponential coefficient. The V98 source envelope clamps tau to at least 5 ms, so tau=1 is outside the intended V98 operating domain. Callers must preserve the 5 ms minimum; do not expose this helper as an unrestricted arbitrary-tau API.

## 2. State memory

The state structure contains:
- 3 x int32 Q16 values;
- 5 x uint32 coefficient/timing values;
- 1 x uint8 phase.

The field payload is 33 bytes. With ordinary 4-byte alignment on a 32-bit ABI, expected `sizeof(eng_metal_vca_t)` is 36 bytes. This is a source-level expectation, not a measured AC79 compiler result. A target compiler size check and generated-code inspection remain required before integration.

At 8 simultaneous voices, expected state storage is 288 bytes, excluding any alignment/placement overhead in the eventual voice structure.

## 3. Per-sample work

The hot path has one unsigned 64-bit multiplication for the Q30 coefficient application, plus bounded integer arithmetic and branches. No floating point, division, heap allocation, or lookup table is used in `process()`.

Theoretical maximum at 8 voices and 44.1 kHz is 352,800 64-bit multiplies per second if every voice renders continuously. This arithmetic count is not a CPU-cycle measurement and says nothing conclusive about AC79 timing. Measure target cycles with the pinned JieLi toolchain and a representative worst-case render block before selecting the integration callback.

## 4. Decision / remaining gates

- Keep the current reciprocal/Euler approximation for the isolated model for now: the tested V98 time-constant domain has sub-0.23% one-tau coefficient error.
- Do not replace it with floating-point exponential math.
- Add a compile-time or host assertion for the expected state size, and obtain an actual target-ABI size report.
- Benchmark the sample loop on AC79 before integration.
- Compare sample-domain render processing with Felucca's once-per-control-tick `amp()` callback before choosing the production integration point.
- Keep manual-mode VCA mapping and FM-1 performance/sequencer trigger semantics open until the host interaction contract is specified.

## 5. Validation boundary

The successful CI run `41221213282` confirms the existing host VCA tests and baseline firmware build. It does not execute the ENERGUN VCA on AC79, register the engine, measure target CPU/RAM, or validate physical FM-1 audio.
