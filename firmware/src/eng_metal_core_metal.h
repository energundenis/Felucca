/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — Metal ratio and post-VCA ring modulation primitives.
 *
 * The reference METAL block has its own ratio-controlled sine oscillator.
 * Its FM operator and ring-modulator phase state remain separate until the
 * production design proves they can share phase without changing behavior.
 */
#ifndef ENG_METAL_CORE_METAL_H
#define ENG_METAL_CORE_METAL_H

#include <stdint.h>

#define ENG_METAL_RATIO_Q16_4_2 275251u /* 4.1999969: nearest Q16 value to 4.20 */
#define ENG_METAL_RATIO_Q16_MAX (16u << 16)

/* Scale a Q0.32 carrier increment by a bounded Q16.16 frequency ratio.
 * The returned uint32_t is the phase increment modulo one cycle, as in the
 * host's unsigned phase accumulator. No division or floating point.
 */
static inline uint32_t eng_metal_ratio_increment(uint32_t carrier_inc,
                                                  uint32_t ratio_q16)
{
    uint64_t scaled;
    if (ratio_q16 > ENG_METAL_RATIO_Q16_MAX)
        ratio_q16 = ENG_METAL_RATIO_Q16_MAX;
    scaled = (uint64_t)carrier_inc * ratio_q16;
    return (uint32_t)((scaled + 32768u) >> 16);
}

/* Full-depth bipolar ring multiplication in Q15. Both inputs and output are
 * bounded signed Q15. Symmetric magnitude rounding avoids negative bias.
 */
static inline int32_t eng_metal_ring_apply_q15(int32_t sample_q15,
                                                int32_t modulator_q15,
                                                uint32_t enabled)
{
    uint32_t sample_mag, mod_mag, result_mag;
    uint64_t product;
    int negative;

    if (!enabled)
        return sample_q15;
    if (sample_q15 > 32767) sample_q15 = 32767;
    if (sample_q15 < -32768) sample_q15 = -32768;
    if (modulator_q15 > 32767) modulator_q15 = 32767;
    if (modulator_q15 < -32768) modulator_q15 = -32768;

    negative = (sample_q15 < 0) != (modulator_q15 < 0);
    sample_mag = (uint32_t)(sample_q15 < 0 ? -(int64_t)sample_q15 : sample_q15);
    mod_mag = (uint32_t)(modulator_q15 < 0 ? -(int64_t)modulator_q15 : modulator_q15);
    product = (uint64_t)sample_mag * mod_mag;
    result_mag = (uint32_t)((product + 16384u) >> 15);
    if (result_mag > 32768u)
        result_mag = 32768u;
    return negative ? -(int32_t)result_mag : (int32_t)result_mag;
}

#endif /* ENG_METAL_CORE_METAL_H */
