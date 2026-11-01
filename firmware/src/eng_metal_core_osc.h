/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — shared dual-oscillator source selector.
 *
 * Uses Felucca-Engines' bounded fixed-point oscillator primitives. Each
 * oscillator owns its own phase accumulator; callers supply the phase slot
 * so OSC1, OSC2 and a future FM source cannot accidentally share phase.
 *
 * This is an isolated production DSP primitive, not a registered engine or
 * a complete OSC/FM signal path.
 */
#ifndef ENG_METAL_CORE_OSC_H
#define ENG_METAL_CORE_OSC_H

#include <stdint.h>

#define ENG_METAL_OSC_SINE    0u
#define ENG_METAL_OSC_TRI     1u
#define ENG_METAL_OSC_SAW     2u
#define ENG_METAL_OSC_PULSE   3u

/* Render one sample and advance only this oscillator's phase.
 * phase_inc is Felucca's Q0.32 cycles/sample increment (vmod_t.inc or a
 * separately calculated modulator increment). pulse_width is Q0.32.
 * An invalid waveform or null phase pointer returns silence without state
 * mutation. The output is the source primitive's signed fixed-point sample.
 */
static inline int32_t eng_metal_osc_next(uint32_t *phase,
                                          uint32_t phase_inc,
                                          uint32_t waveform,
                                          uint32_t pulse_width)
{
    uint32_t p;
    int32_t sample;

    if (!phase)
        return 0;

    p = *phase;
    switch (waveform) {
    case ENG_METAL_OSC_SINE:
        sample = osc_sine(p);
        break;
    case ENG_METAL_OSC_TRI:
        sample = osc_tri(p);
        break;
    case ENG_METAL_OSC_SAW:
        sample = osc_saw(p, phase_inc);
        break;
    case ENG_METAL_OSC_PULSE:
        sample = osc_pulse(p, phase_inc, pulse_width);
        break;
    default:
        return 0;
    }

    *phase = p + phase_inc;
    return sample;
}

#endif /* ENG_METAL_CORE_OSC_H */
