# ENERGUN METAL CORE — CP1.10 fixed-point oscillator primitive audit

Date: 2026-10-31
Branch: energun-metal-core
Pinned source: Felucca-Engines `d0260074ce100a47675814b7f38d61f11176c926`.
Status: reusable primitive audit; no DSP code or test files changed.

## 1. Existing Felucca primitives

The pinned `src/dsp.c` already provides static inline fixed-point functions:
- `osc_sine(ph)`: 1024-entry sine table with linear interpolation; Q15 output. The table has 1025 points so interpolation at the last segment is in-bounds.
- `osc_tri(ph)`: phase-derived triangle waveform; Q15-like signed range.
- `osc_saw(ph, inc)`: saw with polyBLEP wrap correction.
- `osc_pulse(ph, inc, pw)`: pulse derived from two polyBLEP saws.
- `pitch_inc(p)` and `cents_inc(...)`: table-based phase increments for the build sample rate and fine tuning.
- The host generates the tables at build time through the pinned `tools/gen_engine_tables.py`; no floating-point table generation is required on the device.

The functions are static and available to engine source files included after `dsp.c` in the Felucca engine compilation unit. Metal Core can reuse them without adding a new public ABI.

## 2. Mapping to the v98 oscillator roles

v98 needs five independent phase accumulators:
- OSC1 carrier;
- OSC2 direct-audio oscillator;
- Complex FM/AM modulator;
- METAL FM modulator;
- METAL ring oscillator.

Suggested waveform mapping:
- OSC1/OSC2: SINE, TRIANGLE, SAW; the reference West Coast oscillator UI does not require pulse for the carrier pair.
- Complex modulator: SINE / TRIANGLE / SAW / SQUARE; use `osc_pulse(ph, inc, 0x80000000u)` for the 50% square.
- METAL FM and METAL ring: independent sine phases using `osc_sine()`.

Each oscillator must maintain independent phase state even when two oscillators use the same base frequency or ratio. FOLLOW changes their phase increments/frequency targets; it must not alias or reset their phase accumulators.

## 3. Numeric and aliasing observations

- `osc_saw()` uses `blep()`, which includes an integer division when a phase wrap is near. The call is bounded, but the target disassembly cost must be measured for high-frequency worst cases.
- `osc_tri()` is a direct phase mapping and has no explicit band-limiting. It is acceptable as an existing primitive candidate, but high-register harmonic behavior should be measured.
- `osc_sine()` interpolation has a documented table-based quality target; use the existing implementation rather than a lower-resolution ad hoc lookup.
- All waveforms share the host phase-increment convention; do not introduce floating-point phase or sample-rate conversions in render.
- FM can create instantaneous phase increments outside the base pitch range. The fixed-point phase increment and modulation sum need explicit bounds before calling the waveform functions.
- The polyBLEP division path and any high-rate FM modulation must be included in target CPU worst-case tests.

## 4. Proposed isolated primitive tests

Before engine registration, create a host-side test harness for:
1. phase wrap and long-run accumulator boundedness for all five independent phases;
2. sine peak, zero-crossing and period accuracy at low/mid/high pitch;
3. triangle symmetry, peak and DC offset;
4. saw polyBLEP wrap continuity at several increments, including maximum supported pitch;
5. square/pulse duty and wrap continuity;
6. two oscillators with equal frequencies but independent phases;
7. FOLLOW frequency updates without unintended phase reset;
8. Complex FM modulation bound and METAL ratio bound;
9. sample-rate table consistency at 44.1 / 48 / 96 kHz;
10. target instruction cost of the worst-case polyBLEP and FM loops.

Record output peak/RMS/DC, frequency error and discontinuity magnitude. Do not treat a successful compile as an aliasing or audio-quality pass.

## 5. Conclusion

The current Felucca fixed-point DSP already contains suitable building blocks for the basic waveform generation and phase-increment work. The first Metal Core oscillator implementation should reuse those primitives and add only the independent per-voice phase/state management and bounded FM modulation needed by the v98 architecture.

No engine source, tests, engine ID, UI, parameter mapping, storage, firmware package, Flash or device state changed.
