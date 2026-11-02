# ENERGUN FM-1 CORE — CP1.4.2 V1 parameter conversion table (draft)

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **V1 encoding family accepted; numerical conversion table is a draft for verification, not firmware ABI approval.**

## 1. Accepted encoding decision

The V1 parameter-value path uses standard Felucca 7-bit parameter values. Physical source values are quantized into the existing stored-value domain:

- Unipolar controls: raw code 0..127.
- Bipolar controls: use the signed stored domain -64..127 only where the control genuinely spans negative and positive values.
- Frequency, ratio and time controls use logarithmic mappings.
- Levels, depths, gains, amounts and other naturally linear controls use linear mappings.
- Enum and Boolean controls remain discrete and are not treated as continuous 0..127 controls.
- Conversion is deterministic and monotonic; display conversion uses the inverse curve. Quantization rounds to nearest code unless a later control-specific test demonstrates a reason to choose another rule.

High-resolution sidecar-backed parameter values are not selected for V1.

## 2. Formula definitions

For a unipolar logarithmic parameter with physical range [min,max]:

`code = round(127 × ln(value/min) / ln(max/min))`

`display = min × (max/min)^(code/127)`

For a unipolar linear parameter:

`code = round(127 × (value-min)/(max-min))`

`display = min + code × (max-min)/127`

For the bipolar linear FOLD SYMMETRY range [-1,+1], provisional signed mapping:

`code = round(-64 + (value+1) × 191/2)`

`display = -1 + (code+64) × 2/191`

Clamp input to the declared physical range before conversion. Clamp stored codes to the allocated range. Do not apply the bipolar formula to ordinary unipolar controls.

## 3. Per-control provisional mapping

The table records the source default's nominal raw code under the formulas above. These values are **calculated candidates**, not approved stable parameter IDs or final descriptors. The E0–E7 slot-order conflict remains open; mappings are attached to control names, not slot numbers.

| Control | Physical range | Source default | Curve | Default code |
|---|---:|---:|---|---:|
| FREQ | 30–500 Hz | 160 | logarithmic | 76 |
| FM INDEX | 0–1 | 0 | linear | 0 |
| METAL RATIO | 0.5–12 | 4.2 | logarithmic | 85 |
| WEST COAST DRIVE | 0–1 | 0 | linear | 0 |
| FOLD | 0–1 | 0.25 | linear | 32 |
| CUTOFF | 100–8000 Hz | 1200 | logarithmic | 72 |
| RESONANCE | 0.5–20 | 2 | logarithmic | 48 |
| LPG DECAY | 0.02–2 s | 0.25 | logarithmic | 70 |
| OSC2 LEVEL | 0–1 | 0.5 | linear | 64 |
| OSC2 DETUNE | 0–30 cents | 7 | linear | 30 |
| TIMBRE | 0–1 | 0 | linear | 0 |
| MOD RATIO | 0.25–16 | 3.07 | logarithmic | 77 |
| AM DEPTH | 0–1 | 0 | linear | 0 |
| FM FEEDBACK | 0–0.8 | 0 | linear | 0 |
| FM ENV AMT | 0–1 | 0.5 | linear | 64 |
| FM ENV ATTACK | 0.001–1 s | 0.005 | logarithmic | 30 |
| FM ENV DECAY | 0.01–3 s | 0.25 | logarithmic | 72 |
| METAL DECAY | 0.05–3 s | 0.4 | logarithmic | 65 |
| FOLD SYMMETRY | -1–1 | 0 | bipolar linear | 32 |
| WAVEFOLDER DRIVE | 0–1 | 0.2 | linear | 25 |
| LPG RESPONSE | 0–1 | 0.5 | linear | 64 |
| VOICE VCA LEVEL | 0–1 | 0.5 | linear | 64 |
| WAVEFOLDER MODULE VOLUME | 0–1 | 1 | linear | 127 |
| WEST COAST VOLUME | 0–1 | 1 | linear | 127 |
| COMPLEX MODULE VOLUME | 0–1 | 1 | linear | 127 |
| LPG MODULE VOLUME | 0–1 | 1 | linear | 127 |
| ATTACK | 0.001–1 s | 0.01 | logarithmic | 42 |
| DECAY | 0.05–3 s | 0.3 | logarithmic | 56 |
| SUSTAIN | 0–1 | 0 | linear | 0 |
| RELEASE | 0.01–3 s | 0.2 | logarithmic | 67 |

## 4. Quantization and acceptance requirements

Before CP1.4.2 closes, implement host-side reference functions and tests for every continuous control:

1. Endpoints map exactly to the minimum and maximum stored code.
2. Every representable code decodes to a value inside the declared physical range.
3. Mapping is monotonic over the full physical input domain and all 128 (or 192 bipolar) stored codes.
4. Encode(decode(code)) returns the original code for every legal code.
5. Decode(encode(value)) error is measured across the physical range, including source defaults, source step boundaries and perceptually important regions.
6. No NaN, infinity, divide-by-zero, overflow or undefined behavior at endpoints.
7. EDIT, project save/load, user-preset save/load and Motion Lock all use the same canonical mapping; no path may reinterpret raw code as physical value.
8. Legacy project/preset migration preserves existing common parameters and E0–E7 values exactly.
9. Motion Lock values use the same stored code domain and are decoded through the same control-specific curve.
10. Discrete values have explicit enum/Boolean code tables and are excluded from continuous interpolation unless separately approved.

## 5. Known constraints and open gates

- The physical v98 source steps (for example FREQ 1 Hz, ratios 0.01 and envelope times 0.001/0.01 s) are not all individually representable after 7-bit quantization. The accepted design trades exact source-step granularity for native Felucca compatibility.
- The table does not decide final E0–E7 slot order, stable numeric IDs, page IDs, editor protocol extensions, Sidecar format or physical Flash sectors.
- The base-compatible FUN10 + extension sidecar approach remains a proposal pending explicit architecture review. The 7-bit encoding decision does not approve that storage strategy.
- The 32 user presets and four per-part Private banks remain in scope; their persistence architecture must be specified and tested separately.
- No firmware source, binary storage format or physical device was changed by this document.

## 6. Next action

Implement a small host-only conversion reference and exhaustive quantization tests from this table. Review worst-case error and adjust curve family or code allocation only through a documented decision. Then reconcile the E0–E7 ordering and proceed with stable ID allocation.
