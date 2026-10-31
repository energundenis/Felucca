# ENERGUN METAL CORE — CP3.1 filter source audit 01

Date: 2026-11-01
Branch: `energun-metal-core`
Reference: protected `energun_metal_core_v98_reference.html`
Status: source trace complete for the two filter nodes; target filter algorithm/state layout remains undecided.

## 1. Main filter

The reference creates `wcFilt = A.createBiquadFilter()`, sets type `lowpass`, initializes frequency from CUTOFF and Q from RESONANCE.

Parameter behavior:
- CUTOFF handler: target frequency = current CUTOFF, time constant 20 ms.
- RESONANCE handler: target Q = current RESONANCE, time constant 20 ms.
- Sequencer trigger: main-filter cutoff is scheduled to base CUTOFF with 10 ms time constant; when Accent is on, it first targets min(12 kHz, base × 1.15), then returns to base at 65% of step Gate with 25 ms time constant.

This filter is before the LPG filter.

## 2. LPG filter

The reference creates a second independent `BiquadFilterNode`, type `lowpass`, initialized to 1,800 Hz and Q=0.7.

Parameter / trigger behavior:
- RESPONSE knob handler schedules LPG filter frequency to 500 + RESPONSE × 7,500 Hz with 25 ms time constant.
- On each sequencer trigger, LPG filter frequency automation cancels prior scheduled events.
- Transient target = min(12 kHz, CUTOFF × (1.25 + RESPONSE × 3.75)).
- Return target = max(100 Hz, CUTOFF).
- Return event time = min(LPG gain tail, DECAY × 0.22).
- Return time constant = max(12 ms, DECAY × (0.08 + RESPONSE × 0.12)).

The LPG gain envelope is a separate GainNode and separate state machine. Its attack / tail / decay time constants must not be conflated with the LPG filter frequency automation.

## 3. State and memory conclusion

The source confirms two serial, independent low-pass biquad nodes. It does not expose their internal state representation because the reference delegates implementation to Web Audio.

A target port may choose a fixed-point DF2T biquad, for which two delay words per mono filter is a plausible candidate. Under that specific choice, two filters require four 32-bit state words = 16 bytes per voice. This is not a source-proven exact size and must not be represented as a final memory fact until the topology, numeric format, channel count and implementation are frozen.

Likewise, FM-index envelope state remains a provisional 24-byte accounting candidate. The v98 source confirms its separate base/peak/attack/return automation but does not dictate the fixed-point C structure.

## 4. Required implementation gates

- Select mono per-voice filter topology and biquad realization.
- Specify coefficient and state formats, range clamps and saturation.
- Prove stability across CUTOFF / RESONANCE range.
- Preserve distinct main-filter and LPG-filter automation times and trigger behavior.
- Define retrigger cancellation/hold policy.
- Measure target code size, memory and render cost.
- Add host reference-vector tests before production integration.

No filter DSP, production engine registration, parameter IDs, persistence changes, firmware package or device write is included.
