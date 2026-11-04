/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — bounded two-operator phase-modulation FM primitive.
 *
 * The modulator and carrier own independent Q0.32 phase accumulators.
 * depth_phase_q32 is a signed-deviation ceiling expressed as an unsigned
 * phase span; 0x20000000 is 1/8 cycle at full modulation. The primitive
 * performs no allocation, floating point, table lookup, or division.
 *
 * This is an isolated DSP candidate. It is not yet connected to the engine
 * render path or to the final parameter map.
 */
#ifndef ENG_METAL_CORE_FM_H
#define ENG_METAL_CORE_FM_H

#include <stdint.h>
#include "eng_metal_core_osc.h"

#define ENG_METAL_FM_DEPTH_PHASE_MAX 0x40000000u

static inline int32_t eng_metal_fm_phase_offset(int32_t modulator_q15,
                                                 uint32_t depth_phase_q32)
{
    uint32_t magnitude;
    uint64_t product;
    int64_t offset;

    if (depth_phase_q32 > ENG_METAL_FM_DEPTH_PHASE_MAX)
        depth_phase_q32 = ENG_METAL_FM_DEPTH_PHASE_MAX;
    magnitude = (uint32_t)(modulator_q15 < 0
                               ? -(int64_t)modulator_q15
                               : modulator_q15);
    product = (uint64_t)magnitude * depth_phase_q32;
    offset = (int64_t)((product + 16384u) >> 15);
    return modulator_q15 < 0 ? -(int32_t)offset : (int32_t)offset;
}

/* One sine-operator sample. Phase is sampled before each accumulator advances.
 * Invalid pointers return silence and leave every supplied state untouched.
 */
static inline int32_t eng_metal_fm_sine_next(uint32_t *carrier_phase,
                                              uint32_t carrier_inc,
                                              uint32_t *modulator_phase,
                                              uint32_t modulator_inc,
                                              uint32_t depth_phase_q32)
{
    uint32_t carrier, modulator;
    int32_t mod_sample;
    int32_t offset;

    if (!carrier_phase || !modulator_phase)
        return 0;

    carrier = *carrier_phase;
    modulator = *modulator_phase;
    mod_sample = osc_sine(modulator);
    offset = eng_metal_fm_phase_offset(mod_sample, depth_phase_q32);

    *modulator_phase = modulator + modulator_inc;
    *carrier_phase = carrier + carrier_inc;
    return osc_sine(carrier + (uint32_t)offset);
}

#endif /* ENG_METAL_CORE_FM_H */
