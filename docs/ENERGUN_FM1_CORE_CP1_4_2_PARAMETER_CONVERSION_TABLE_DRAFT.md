# ENERGUN FM-1 CORE — CP1.4.2 V1 parameter conversion table (draft)

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **V1 encoding family accepted; numerical conversion table is a draft for verification, not firmware ABI approval.**

## 1. Accepted encoding decision

The V1 parameter-value path uses standard Felucca 7-bit parameter values. Physical source values are quantized into the existing stored-value domain:

- Unipolar controls: raw code 0..127.
- Bipolar controls use the standard signed 7-bit descriptor range -64..63 (128 codes), with zero represented exactly; the generic project byte container can carry -64..127, but that wider container range is not the bipolar descriptor range.
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

For bipolar FOLD SYMMETRY [-1,+1], use the signed descriptor range -64..63 with exact zero. The two halves have one-code asymmetry because the range contains 128 codes:

`value < 0: code = round(64 × value); display = code / 64`

`value >= 0: code = round(63 × value); display = code / 63`

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
| FOLD SYMMETRY | -1–1 | 0 | centered bipolar linear | 0 |
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
3. Mapping is monotonic over the full physical input domain and all 128 stored codes for both unipolar and signed bipolar descriptors.
4. Encode(decode(code)) returns the original code for every legal code.
5. Decode(encode(value)) error is measured across the physical range, including source defaults, source step boundaries and perceptually important regions.
6. No NaN, infinity, divide-by-zero, overflow or undefined behavior at endpoints.
7. EDIT, project save/load, user-preset save/load and Motion Lock all use the same canonical mapping; no path may reinterpret raw code as physical value.
8. Legacy project/preset migration preserves existing common parameters and E0–E7 values exactly.
9. Motion Lock values use the same stored code domain and are decoded through the same control-specific curve.
10. Discrete values have explicit enum/Boolean code tables and are excluded from continuous interpolation unless separately approved.

## 4A. Initial host-only reference implementation

- Added `tests/metal_core_parameter_quantization_test.c` as a standalone C host oracle; it is not linked into firmware and does not assign IDs or persistence ABI.
- Added it to `tests/run_metal_core_fast.sh` as strict C11 / `-Wall -Wextra -Werror` compilation and execution.
- Coverage: 30 continuous controls; expected default codes; exact endpoints; exhaustive encode(decode(code)) for all 128 unipolar or 128 signed bipolar codes; decoded-range checks; 10,001-point monotonicity sweep; invalid/non-finite input rejection; absolute/relative/default quantization error metrics.
- FOLD SYMMETRY uses the signed descriptor range -64..63 and a centered piecewise linear mapping so physical zero maps exactly to code 0: negative values scale by 64, nonnegative values by 63. The generic FUN10 container can serialize -64..127, but that broader container range must not be mistaken for the signed bipolar descriptor range.
- Fast CI run [41403689043](https://github.com/energundenis/Felucca/actions/runs/41403689043) completed successfully on test-code HEAD `726028e9fda0a5d6b174faee0ae4e48e83f6c352`: all 16 focused tests passed, including the new CP1.4.2 reference (0 failures across 30 controls). The full baseline build run `41403689065` was still running when this note was prepared. This was the initial reference run before the signed bipolar range was corrected; its initial 192-code FOLD SYMMETRY candidate is superseded by the 128-code signed test and passing run `41404580795`.
- Measured worst-case relative error for logarithmic mappings: FREQ 1.114%, METAL RATIO 1.259%, CUTOFF 1.740%, RESONANCE 1.463%, LPG DECAY 1.830%, MOD RATIO 1.651%, FM ENV ATTACK / ATTACK 2.757%, FM ENV DECAY / RELEASE 2.271%, METAL DECAY / DECAY 1.625%. These are dense-grid measurements, not acceptance thresholds.
- For linear controls near zero, relative-to-value error is not a useful quality metric (it becomes 100% at zero even when absolute error is one code step). Use absolute error normalized to the control's full-scale span for linear controls when setting acceptance limits.
- Numerical acceptance thresholds are intentionally not invented before reviewing source-step/perceptual priorities.
- Added verified v98 source-step metadata for all 30 continuous controls and exercised every declared step boundary in the host reference. Fast CI run [41404054183](https://github.com/energundenis/Felucca/actions/runs/41404054183) passed all 16 tests with 0 failures. The quantizer checked **9,434 source-step boundary values** across the 30 controls; every boundary mapped monotonically and decoded within the declared physical range.
- Maximum measured source-step-unit errors are largest for the log-mapped controls with fine source UI increments: FM ENV ATTACK 26.0 steps, normal ATTACK 26.0, MOD RATIO 25.55, METAL RATIO 14.66, CUTOFF 13.13, FM ENV DECAY / RELEASE 6.18. This is expected loss of source UI granularity under 7-bit quantization; the corresponding dense-grid relative errors remain below 2.76% for all log controls. These measurements do not by themselves establish acceptable musical resolution.
- The full baseline build/regression run [41404054153](https://github.com/energundenis/Felucca/actions/runs/41404054153) was still in progress at the time of this update; only the focused lane is confirmed green.

## 4C. Proposed quantization acceptance thresholds (review required)

The measured dense-grid errors match the theoretical nearest-code limits. Proposed gates for review, not yet user-approved:

- Logarithmic controls: maximum relative decode error <= 3.0%. The worst theoretical range is 0.001..1 s (ATTACK / FM ENV ATTACK), at 2.7569%; the measured host sweep matches this bound.
- Unipolar linear controls: maximum absolute error <= half one code interval, i.e. 0.5/127 of the physical span (0.3937% of full span).
- Bipolar FOLD SYMMETRY: maximum absolute error <= 0.5/63 = 0.00793651 physical units (0.3968% of the full -1..+1 span), using the centered -64..63 mapping.
- Source-step error is reported in source-step units as a diagnostic, not as a pass/fail gate: some original v98 controls have far more than 128 source positions. For example, the measured worst-case errors are 26 source steps for ATTACK/FM ENV ATTACK, 25.55 for MOD RATIO and 14.66 for METAL RATIO, while their relative physical errors remain within the proposed 3% logarithmic bound.

The limits above follow the mathematical nearest-quantization interval, rather than a subjective musical tolerance. Approval is still required before they become CP1.4.2 closure criteria.

## 5. Known constraints and open gates

- The physical v98 source steps (for example FREQ 1 Hz, ratios 0.01 and envelope times 0.001/0.01 s) are not all individually representable after 7-bit quantization. The accepted design trades exact source-step granularity for native Felucca compatibility.
- The table does not decide final E0–E7 slot order, stable numeric IDs, page IDs, editor protocol extensions, Sidecar format or physical Flash sectors.
- The base-compatible FUN10 + extension sidecar approach remains a proposal pending explicit architecture review. The 7-bit encoding decision does not approve that storage strategy.
- The 32 user presets and four per-part Private banks remain in scope; their persistence architecture must be specified and tested separately.
- No firmware source, binary storage format or physical device was changed by this document.

## 6. Next action

Review the corrected signed mapping's test run and worst-case quantization metrics. Establish per-control error acceptance limits and add exact legacy project/preset compatibility fixtures. Then reconcile the E0–E7 ordering and proceed with stable ID allocation.


## 4B. Felucca signed descriptor range clarification — 2026-11-02

A source-level recheck of `params.c`, `project.c` and `motion.c` identified an important distinction:
- FUN10 project values are serialized as one byte using `value + 64`; the generic accepted in-memory/wire value range is -64..127.
- The existing Felucca bipolar parameter format (`F_BIPCT`) is specifically -64..63, i.e. 128 signed codes with exact zero. It is not the full generic -64..127 container range.
- The FOLD SYMMETRY reference and test were corrected to use -64..63, with a piecewise centered mapping and code 0 for physical zero. The wider 192-value range must not be used merely because the project byte container can represent it.
- Corrected signed-mapping fast CI run [41404580795](https://github.com/energundenis/Felucca/actions/runs/41404580795) passed on code HEAD `29125b419a83b10965036d85ab7b313755dcd1eb`: FOLD SYMMETRY now uses exactly 128 signed codes (-64..63), maps physical zero to code 0, and passes endpoint, exhaustive round-trip, monotonicity and all 201 source-step boundary checks. All 16 focused tests passed.
- Source evidence: `firmware/src/params.c` existing `F_BIPCT` descriptors use -64..63; `firmware/src/project.c` packs `p + 64` and rejects bytes above 191 on load; `firmware/src/motion.c` stores motion values as `int8_t` and validates against descriptor min/max as well as -64..127.
- This correction is a host reference/test change only. No production parameter descriptor, ID, serializer, project format or firmware source was changed.


## 4D. Strict mathematical quantization-bound regression — 2026-11-02

- Extended the host reference with a 100,001-point sweep per control, enforcing nearest-code mathematical error bounds, plus finite out-of-range clamping checks.
- Linear controls: maximum absolute error <= 0.5/127 of the physical span.
- Log controls: maximum relative error <= the exact half-code multiplicative bound derived from each range. Worst measured value is 2.7569% for ATTACK and FM ENV ATTACK.
- Bipolar FOLD SYMMETRY: maximum absolute error <= 0.5/63 (0.00793651) with the centered -64..63 mapping.
- Linear worst-case measured error is 0.3937% of full scale. All 30 controls passed the nearest-code bound and endpoint-clamp checks.
- Fast CI [41406252578](https://github.com/energundenis/Felucca/actions/runs/41406252578) passed on code HEAD `89528e4948b58b98232543014d805ec3bd306616`: 16/16 focused tests passed; 0 quantization-reference failures.
- These are mathematical quantizer-correctness bounds, not a musical/perceptual approval. CP1.4.2 remains open for review of musical resolution, base-compatible FUN10 migration, user-preset extension scope and exact expanded-count compatibility fixtures.


## 4E. Logarithmic one-code spacing reference

The following values show the approximate physical increment between adjacent raw codes at the low endpoint, the control default and the high endpoint. For a logarithmic control, adjacent values have a constant ratio, while the absolute increment grows with the displayed value. This makes the trade-off visible for musical-resolution review; it does not approve the curve.

| Control | Adjacent step at min | At default | At max | Adjacent code ratio |
|---|---:|---:|---:|---:|
| FREQ (Hz) | 0.672 | 3.584 | 11.200 | 1.022400 |
| METAL RATIO | 0.01267 | 0.10643 | 0.30408 | 1.025340 |
| CUTOFF (Hz) | 3.511 | 42.128 | 280.851 | 1.035106 |
| RESONANCE | 0.01474 | 0.05894 | 0.58945 | 1.029472 |
| LPG DECAY (s) | 0.000739 | 0.009232 | 0.073853 | 1.036927 |
| MOD RATIO | 0.00832 | 0.10220 | 0.53263 | 1.033289 |
| FM ENV ATTACK (s) | 0.0000559 | 0.0002795 | 0.055898 | 1.055898 |
| FM ENV DECAY (s) | 0.000459 | 0.011484 | 0.137806 | 1.045935 |
| METAL DECAY (s) | 0.001638 | 0.013106 | 0.098293 | 1.032764 |
| ATTACK (s) | 0.0000559 | 0.0005590 | 0.055898 | 1.055898 |
| DECAY (s) | 0.001638 | 0.009829 | 0.098293 | 1.032764 |
| RELEASE (s) | 0.000459 | 0.009187 | 0.137806 | 1.045935 |

Interpretation: the 7-bit log curve is relatively fine around low frequencies and short times, but its absolute step becomes wider at the upper end. For example, FREQ is about 3.58 Hz/code at its 160 Hz default and 11.2 Hz/code near 500 Hz. The original v98 1 Hz control increment is therefore not retained throughout the range. The user-facing editor must show the decoded physical value, not imply that every source UI step is available.


## 4F. Logarithmic curve trade-off against linear mapping

This comparison is a resolution aid for review. “Linear step” is the constant physical increment if the same control used a linear 0..127 mapping; “log step at default” is the approximate adjacent-code physical increment of the current logarithmic candidate near its default.

| Control | Linear step | Log step at default | Log / linear |
|---|---:|---:|---:|
| FREQ (Hz) | 3.701 | 3.584 | 0.968× |
| METAL RATIO | 0.09055 | 0.10643 | 1.175× |
| CUTOFF (Hz) | 62.205 | 42.128 | 0.677× |
| RESONANCE | 0.15354 | 0.05894 | 0.384× |
| LPG DECAY (s) | 0.015591 | 0.009232 | 0.592× |
| MOD RATIO | 0.12402 | 0.10220 | 0.824× |
| FM ENV ATTACK (s) | 0.007866 | 0.000279 | 0.036× |
| FM ENV DECAY (s) | 0.023543 | 0.011484 | 0.488× |
| METAL DECAY (s) | 0.023228 | 0.013106 | 0.564× |
| ATTACK (s) | 0.007866 | 0.000559 | 0.071× |
| DECAY (s) | 0.023228 | 0.009829 | 0.423× |
| RELEASE (s) | 0.023543 | 0.009187 | 0.390× |

Interpretation: log is strongly advantageous around short attack times, resonance, and lower cutoff values. Near the METAL RATIO default, log is slightly coarser than linear; this is the clearest control-specific trade-off in the current curve family. FREQ near its default has similar absolute resolution under either curve, while log reallocates codes toward its lower range. If later listening tests show the METAL RATIO operating region needs finer increments around 4.2, a segmented/piecewise nonlinear curve may be evaluated, but it would require a new documented curve, recomputed defaults, and a fresh exhaustive regression before acceptance.


## 4G. Exact-head full baseline regression result

- Full baseline CI [41406252564](https://github.com/energundenis/Felucca/actions/runs/41406252564) also completed SUCCESS on the same code HEAD `89528e4948b58b98232543014d805ec3bd306616`: unmodified Felucca firmware build, AC79 target-compiler VCA probe, all host/web regressions and artifact upload completed. Artifact [energum-fm1-baseline-89528e4948b58b98232543014d805ec3bd306616.zip](https://github.com/energundenis/Felucca/actions/runs/41406252564/artifacts/13124032492), SHA-256 `209f57ff896b4dff66c5be24b9f7c570c2ddc7b371b935192247229199660ef1`.
- This is baseline/toolchain/regression evidence only. It does not register/build METAL CORE as a production engine, qualify its complete DSP graph on AC79, or validate hardware behavior.


## 4H. Expanded fast-regression suite — 2026-11-02
- Fast CI [41408456648](https://github.com/energundenis/Felucca/actions/runs/41408456648) SUCCESS on code HEAD `992e4591629277d9952fc76610928972e515a6f0`: all 17 focused tests passed.
- The suite now also includes the real Felucca `track_render()` / `voice_start()` OSC dispatch lifecycle test. The CP1.4.2 quantization reference remains PASS: 30 controls, 9,434 source-step boundaries, 100,001-point nearest-code bound sweep, endpoint clamping, 0 failures.
- Full baseline CI for this exact HEAD is still running; the new legacy FUN10 np=127 rejection and old-firmware 127-value user-preset mapping fixtures are pending that full lane.
