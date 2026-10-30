# ENERGUN METAL CORE — Eight-Parameter V1 Control Map

Status: proposed V1 mapping for review during implementation. It preserves the native Felucca preset/project format and does not add a persistence extension.

## 1. Constraint confirmed in source

Felucca's engine interface defines exactly eight engine EDIT descriptors, `P_E0..P_E7`. The UI exposes them as two four-knob pages:

- EDIT 1: `P_E0, P_E1, P_E2, P_E3`
- EDIT 2: `P_E4, P_E5, P_E6, P_E7`

The native engine preset stores `int8_t e[8]`; projects store the track parameter array, including these eight values. The parameter descriptor supports signed ranges, enum labels and formatting. Therefore the first Metal Core version can save eight persistent controls without changing global preset/project formats.

## 2. Proposed page layout

Rename the two pages through engine-specific descriptors/help where supported; do not alter the shared page table for all engines.

### EDIT 1 — OSC / FM

| Stored value | Label | Proposed range | Default | DSP meaning |
|---|---|---:|---:|---|
| E0 | OSC 1 WAVE | 0..3 enum | SINE | Selects OSC1 waveform |
| E1 | OSC 2 WAVE | 0..3 enum | SINE | Selects OSC2 waveform |
| E2 | OSC 2 TUNE | -64..63 | 0 | Bipolar macro spanning coarse ±24 semitones and fine ±50 cents; center is exact unison |
| E3 | OSC 2 LEVEL | 0..127 | 0 | Direct OSC2 audio level, independent of FM amount |

### EDIT 2 — FM / FOLD / LPG

| Stored value | Label | Proposed range | Default | DSP meaning |
|---|---|---:|---:|---|
| E4 | FM RATIO | 0..127 | calibrated value for 4.20:1 | Logarithmic mapping 0.5:1..8:1; landmarks must include 0.5, 1, 2, 3, 4, 4.2 |
| E5 | FM AMOUNT | 0..127 | 0 | Bounded phase modulation depth; 0 preserves the base tone |
| E6 | FOLD | 0..127 | 0 | Musical fold macro; drives fold amount and a restrained linked symmetry/drive curve |
| E7 | LPG | 0..127 | calibrated neutral/open value | Musical LPG macro; linked mapping of cutoff, resonance, decay and response |

The ranges/defaults above are design targets, not yet validated DSP scales. The exact descriptor values and formatting must be set only after fixed-point transfer functions and tests are defined.

## 3. Why this mapping

- OSC1/OSC2 waveform choice and OSC2 direct level remain directly accessible.
- OSC2 tune is one continuous bipolar control; the mapping must have a precise center and no pitch discontinuity.
- FM ratio and FM amount remain independent, preserving the approved FM topology.
- FOLD and LPG each receive a single macro rather than pretending four independent controls can fit into one stored value.
- The initial FM feedback remains internally fixed at zero. It is not silently coupled to FM amount.
- FOLD mode starts at a carefully selected default (CLASSIC candidate); SOFT/HARD remain future mode selection, not secretly folded into an undocumented parameter.
- LPG's linked macro must preserve a predictable response: opening it raises cutoff and response while managing resonance and decay conservatively. It must not cause self-oscillation or transient spikes.

## 4. Explicit V1 limitations

The following controls are not independently exposed in V1:
- OSC2 coarse and fine tune as separate knobs;
- FM feedback;
- FOLD symmetry, drive and SOFT/CLASSIC/HARD mode;
- LPG cutoff, resonance, decay and response as four separate knobs.

They remain implementation constants / calibrated relationships in V1, not hidden persistent controls. Their values must be documented and tested. Do not imply that they are separately editable or saved.

If independent control of these parameters is required before implementation is complete, stop and design a persistence/UI extension explicitly. Do not expand `voice_t`, change project format, or pack undocumented sub-values into the eight signed EDIT bytes as a shortcut.

## 5. Modulation and parameter-lock compatibility

The eight visible values retain stable identities `P_E0..P_E7`, so native preset save/load and sequencer parameter locks can refer to them. Macro controls should be smooth and bounded. Any internal derived values (e.g. LPG cutoff/resonance) must be recomputed deterministically from the stored macro at block/control rate.

Avoid changing the meaning of an EDIT parameter between pages or modes: parameter locks and automation rely on stable parameter identity.

## 6. Implementation gate

Before coding:
1. Confirm engine-specific page labels and descriptor rendering can show useful units/names without changing shared Felucca UI.
2. Define exact integer-to-DSP curves for OSC2 tune, FM ratio, FM amount, FOLD and LPG.
3. Decide calibrated neutral/open LPG default and FOLD default using the clean Metal Core reference.
4. Add mapping unit tests: min/default/max, monotonicity, exact center, no overflow, no discontinuity.
5. Verify save/load and parameter locks for all eight values.

No DSP code, UI integration, firmware build, hardware test or flash is included in this document.
