# ENERGUN METAL CORE — CP3.1 fixed-point biquad candidate 01

Date: 2026-11-01
Branch: `energun-metal-core`
Status: engineering proposal for review and host prototyping; **not production DSP and not a frozen state ABI**.

## 1. Decision basis

The protected v98 reference uses two serial, independent Web Audio low-pass BiquadFilterNodes:
- MAIN FILTER: CUTOFF 100–8,000 Hz; RESONANCE 0.5–20; default 1,200 Hz / Q=2.
- LPG FILTER: initial 1,800 Hz / Q=0.7; RESPONSE and per-trigger automation can raise its cutoff, capped at 12,000 Hz.

The FM-1 audio path renders 32-sample blocks (`CTL=32`) and its nominal output sample rate is approximately 44.1 kHz. Both 8 kHz and 12 kHz are below Nyquist at this rate. The host audio stream and engine helpers are integer/fixed-point; floating-point work must not enter the render callback.

The two filters must have independent histories and independent cutoff automation. This document proposes a conventional normalized low-pass biquad, not a claim of bit-exact equivalence to the browser's implementation.

## 2. Candidate realization

Candidate: **Direct Form II Transposed (DF2T), mono, per voice, two signed 32-bit delay states**.

For normalized coefficients and input/output in the engine's chosen common signal scale:

```text
y  = sat32(b0*x + z1)
z1 = sat32(b1*x - a1*y + z2)
z2 = sat32(b2*x - a2*y)
```

All coefficient × signal products are evaluated in signed 64-bit intermediates, rounded before rescaling, and combined in a wider intermediate before saturation. No division, trigonometric function, coefficient generation, allocation, or unbounded loop is allowed in the per-sample callback.

Candidate coefficient storage: signed Q3.29 (scale 2^29). This range accommodates normalized low-pass denominator coefficient a1, whose magnitude approaches 2 near Nyquist. Candidate delay states: signed int32 in the same signal scale as x/y, with explicit saturation. This is a *candidate numeric contract*; signal headroom and saturation thresholds must be validated against actual pre-filter levels from the oscillator/FM/fold chain before approval.

For the RBJ-style low-pass prototype:
- `w0 = 2*pi*fc/fs`
- `alpha = sin(w0)/(2*Q)`
- `a0 = 1 + alpha`
- `b0 = (1-cos(w0))/(2*a0)`
- `b1 = (1-cos(w0))/a0`
- `b2 = b0`
- `a1 = -2*cos(w0)/a0`
- `a2 = (1-alpha)/a0`

The recurrence above uses the conventional denominator sign convention (subtract a1*y and a2*y). Coefficients are normalized by a0 before quantization.

## 3. Memory implication — conditional only

With two int32 states per mono biquad:
- one filter = 8 bytes per voice;
- MAIN + LPG filters = 16 bytes per voice;
- eight voice slots = 128 bytes per part.

This is the same 16-byte candidate already shown in the CP3.1 subtotal, now tied to an explicit proposed realization. It is **not yet accepted as final**: state range, quantization, filter input scale, saturation, and target compiler layout still need evidence. Coefficients are proposed as part/block-level data, not duplicated per voice; this sharing model must be checked against voice-specific modulation before implementation.

## 4. Parameter smoothing and automation

Do not smooth the biquad's quantized coefficients by blindly interpolating all five coefficients: that can leave the stable-coefficient manifold and produce unstable intermediate filters.

Proposed separation:
1. Maintain smoothed frequency and Q control values.
2. Advance their source-derived trajectories with bounded one-pole/event state.
3. Recalculate normalized coefficients at a bounded control/block boundary (at most once per 32-sample block initially).
4. Hold that coefficient set for the next block.
5. Measure block-rate stepping in host renders; if audible, increase update cadence or use a proven stable parameter-domain interpolation method.

Preserve distinct source timing:
- Main CUTOFF / RESONANCE knob updates: 20 ms time constant.
- Main trigger base cutoff: 10 ms; Accent transient cap = min(12 kHz, base × 1.15), return begins at 65% of Gate with 25 ms time constant.
- LPG RESPONSE direct frequency update: 25 ms.
- LPG trigger transient: min(12 kHz, CUTOFF × (1.25 + RESPONSE × 3.75)); return to max(100 Hz, CUTOFF) at the source-derived event and time constant recorded in the filter source audit.

The main-filter Accent trajectory and LPG filter trajectory are independent. A new trigger must implement the source's cancel-and-hold semantics for the LPG automation; exact state transition tests are required.

## 5. Numerical and behavioral risks

1. **High resonance:** MAIN Q reaches 20. Prove bounded internal state for worst-case resonant input and sustained excitation; test near cutoff and at low/high cutoff.
2. **Input headroom:** oscillator, FM, METAL and wavefolder stages can exceed nominal full scale. The final pre-filter signal bound is not yet frozen. Do not select saturation limits from the output format alone.
3. **Coefficient quantization:** test Q3.29 against a double-precision oracle across cutoff/Q corners and sweep trajectories. Record max magnitude/phase error and pole stability.
4. **Very low cutoff:** 100 Hz at 44.1 kHz requires fine coefficient resolution. Check decay time, limit cycles and denormal-equivalent integer tails.
5. **High cutoff:** check stability and coefficient range at 8 kHz main / 12 kHz LPG transient and at every supported sample rate.
6. **Block updates:** 32-sample coefficient cadence may cause zipper/sideband artifacts. Compare against sample-updated reference; no approval without audio regression.
7. **State saturation:** saturating a recursive state is a safety boundary, not a normal operating mode. Count and assert zero saturation in all valid test vectors.

## 6. Required host tests before target port

- DC and impulse response; silence and tail-to-zero behavior.
- Frequency response against a double-precision reference grid.
- Resonance sweep Q=0.5, 0.7, 1, 2, 5, 10, 20.
- Cutoff corners 100, 1200, 8000 Hz and LPG 1800 / 500 / 12000 Hz (respecting Nyquist).
- Step and continuous cutoff/Q automation with 10, 20 and 25 ms constants.
- Accent transient and return timing, including short Gate.
- LPG retrigger while frequency is moving; cancel-and-hold behavior.
- Worst-case full-scale and over-range input; assert bounded states and zero unintended saturation in the approved operating range.
- Independent two-filter histories and eight-voice isolation.
- Deterministic output and no floating-point operations in target render path.

## 7. Target port gates

Only after host tests pass:
- choose/verify a fixed-point method for sin/cos or equivalent coefficient generation outside the sample loop;
- inspect pi32v2 compiler output for 64-bit multiply and saturation helpers;
- measure coefficient preparation cost and complete callback cost;
- assert exact struct sizes and alignment on AC79;
- verify both `FELUCCA_ADPCM=0` and `FELUCCA_ADPCM=1` memory configurations;
- keep the existing `voice_t` ABI unchanged.

No filter code was added in this proposal. No production engine registration, parameter IDs, persistence changes, graph connection, factory firmware modification, device write or flash was performed.

## 8. Disposition

DF2T is the preferred candidate for a first host prototype because its two-state mono realization has a compact and explicit memory footprint. Q3.29 coefficients and int32 signal-scale states are proposals pending numeric-range proof. CP3.1 remains open.
