# ENERGUN METAL CORE — FM design 01

Status: **APPROVED BASELINE — compatible with the established METAL CORE direction**. This freezes the FM design direction, not completed DSP or proven target performance.

## 1. Design objective

FM must expand the established METAL CORE sound without replacing its identity:
- retain a solid, controllable bass foundation;
- add metallic / inharmonic upper partials when desired;
- support industrial leads and harder transient-rich tones;
- avoid uncontrolled high-frequency whistles, brittle aliasing, clicks, and excessive level growth;
- avoid an obvious 303-style nasal slide.

The previously identified METAL CORE reference setting with FM INDEX at 0 and METAL / FOLLOW engaged remains a valid sound-design reference. FM must therefore be independently controllable and must not be required merely to make the engine produce its core sound.

## 2. Modulation architecture

Use OSC 2 as the FM modulator and OSC 1 as the carrier.

For the first digital implementation, use phase modulation (PM) as the practical implementation of the FM control:
- OSC 1 phase is offset by a scaled OSC 2 signal for each audio sample.
- OSC 2 remains an independently generated oscillator.
- OSC 2 direct audio mix remains independent from its modulation role.
- Modulation is audio-rate, not a low-rate control update.
- Do not add a third hidden modulator.

This choice is deliberate: a phase-offset implementation provides a bounded, practical digital approach and avoids directly accumulating modulator values into OSC 1's phase increment. PM and every form of true FM are not sonically identical in all conditions; the mapping must be evaluated against METAL CORE reference patches and recalibrated if listening tests show a mismatch. UI terminology remains FM to match the instrument design.

Conceptual path:

```text
OSC 1 phase -------------------------------+
                                           v
OSC 2 waveform --> FM AMOUNT --> phase offset --> OSC 1 waveform
       |                                   |
       +--> optional direct audio ----------+--> oscillator mix --> FOLD
```

The final order of direct OSC 2 mixing relative to the FOLD input must be confirmed during DSP integration. The modulation path itself is independent of OSC 2 direct-audio level.

## 3. FM RATIO

- Ratio is a frequency multiplier for the modulator relative to the carrier's note frequency.
- OSC 2 coarse and fine tuning apply as an additional pitch offset to the modulator; ratio remains an independent multiplier.
- Approved continuous range: 0.5:1 to 8.0:1.
- Approved default: 4.20:1, preserving the previously useful METAL CORE ratio reference.
- Provide stable musical landmarks around 0.5, 1.0, 2.0, 3.0, 4.0, and 4.2; the control may remain continuous between them.
- The parameter must not jump unpredictably when crossing integer ratios.
- Ratio changes must be smoothed or phase-increment-ramped to avoid clicks.

The effective modulator frequency is conceptually:

`f_mod = f_note × ratio × 2^(coarse/12 + fine/1200)`

The exact fixed-point conversion and clamping must follow Felucca's pitch / oscillator conventions.

## 4. FM AMOUNT (INDEX)

- Range: 0 to a conservative bounded maximum.
- Approved default: 0.
- At 0, OSC 1 must be unaffected by FM, regardless of ratio or feedback setting.
- Increase amount to add sidebands / metallic complexity.
- Amount must be calibrated in a stable normalized or fixed-point scale, not exposed as an arbitrary unbounded phase offset.
- Use a conservative maximum first; expand only after listening tests and aliasing / CPU validation.
- Amount changes must be smoothed to avoid zipper noise and clicks.

Do not assume that a UI value such as 0.1 or 1.0 maps directly to a physical modulation index until the fixed-point implementation and oscillator amplitude convention are audited.

## 5. FEEDBACK

Proposal: feedback is applied to OSC 2's own modulation waveform, not to the final output and not to the FOLD stage.

- Range: 0 to a conservative bounded maximum.
- Default: 0.
- At 0, OSC 2 is a clean modulator.
- Increasing feedback adds complexity and can make the spectrum more metallic / noisy.
- Feedback must be bounded and normalized so it cannot cause runaway values or DC accumulation.
- Use a short, explicitly bounded feedback state; do not create an unbounded delay line.
- Feedback changes must be smoothed.
- Feedback must not alter OSC 2's direct-audio level or silently change the ratio.

The exact feedback topology (one-sample feedback, small fixed feedback delay, or another bounded phase-feedback method) is an implementation decision to be made after auditing Felucca's existing oscillator/table conventions. Do not guess a topology and ship it without tests.

## 6. Proposed FM page controls

Four primary controls:

| Control | Function | Proposed default |
|---|---|---:|
| FM AMOUNT | Modulation depth / index | 0 |
| FM RATIO | Modulator frequency multiplier | 4.20:1 |
| FEEDBACK | OSC 2 self-feedback amount | 0 |
| FM MIX | Optional direct OSC 2 audio mix | 0% |

FM MIX is a UI alias for the OSC 2 direct-audio level already defined on the OSC page; it must not create a second independent stored parameter. If the UI exposes the same parameter on both pages, both controls must stay synchronized.

If a four-knob page cannot expose all functions without duplication, prioritize AMOUNT, RATIO, FEEDBACK, and keep FM MIX as a linked control to OSC 2 LEVEL.

## 7. Safe initial calibration

Initial validation patch:
- OSC 1 waveform: SINE.
- OSC 2 waveform: SINE.
- OSC 2 coarse/fine tune: 0.
- Ratio: 4.20:1.
- FM AMOUNT: 0.
- FEEDBACK: 0.
- OSC 2 direct level: 0%.
- FOLD and METAL/FOLLOW bypassed only for isolated FM diagnostics; restore them for final character tests.

Validation sequence:
1. Confirm amount = 0 is bit-for-bit / audibly equivalent to OSC 1 without FM.
2. Raise amount gradually and verify sideband growth without sudden level jumps.
3. Sweep ratio while holding amount fixed.
4. Add small feedback increments only after the non-feedback FM path is stable.
5. Test low, middle, and high registers for aliasing, whistling, crackling, and clicks.
6. Re-enable FOLD, FOLLOW, and METAL and evaluate the full METAL CORE character.
7. Verify OSC 2 direct mix independently of FM amount.
8. Measure CPU cost and fixed-point headroom on target hardware.

## 8. Explicitly out of scope

- Third oscillator or separate hidden modulator.
- Through-zero FM.
- Ring modulation.
- Hard sync.
- Unbounded or multi-stage feedback networks.
- Automatic ratio quantization / scale locking.
- Any change to Felucca's existing engines or global effects.

## 9. Approved decision

The following is approved as the FM architecture baseline:
- OSC 2 phase-modulates OSC 1 at audio rate.
- Ratio range 0.5:1–8.0:1, default 4.20:1.
- FM AMOUNT range is conservatively bounded, default 0; exact numeric scale is calibrated during DSP implementation.
- Feedback acts on OSC 2's own modulator signal, default 0, with topology finalized only after source audit.
- OSC 2 direct-audio level remains independent and linked between OSC and FM page if shown twice.
- The existing METAL CORE sound with FM amount 0 remains a valid baseline; FM is an optional extension, not a prerequisite for the core character.
- Preserve the established METAL CORE identity: solid bass foundation, controlled metallic harmonics, and no unwanted whistles, crackles, or forced acid-like nasal motion.

Approval freezes this design direction, not unverified DSP details. The PM/FM sonic mapping and feedback topology must pass listening and technical validation before being considered complete. No engine registration, UI integration, or firmware flashing is authorized by this document alone.
