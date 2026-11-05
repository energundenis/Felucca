/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — Metal ratio and post-VCA ring modulation primitives.
 *
 * This is an isolated DSP candidate. It is not connected to the production
 * engine descriptor or render path.
 */
#ifndef ENG_METAL_CORE_METAL_H
#define ENG_METAL_CORE_METAL_H

#include <stdint.h>

#define ENG_METAL_RATIO_Q16_4_2 275251u /* nearest Q16.16 value to 4.20 */
#define ENG_METAL_RATIO_Q16_MAX (16u << 16)

/* Scale a Q0.32 carrier increment by a bounded Q16.16 ratio. The returned
 * uint32_t is a modulo-one-cycle phase increment. No division or float.
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

/* Multiply two signed Q15 values. Disabled mode is a bit-exact bypass.
 * Enabled mode clamps inputs first and saturates the positive result at
 * INT16_MAX (the -32768 * -32768 corner rounds to +32768 mathematically,
 * which is not representable in signed Q15).
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
    sample_mag = (uint32_t)(sample_q15 < 0
                                ? -(int64_t)sample_q15 : sample_q15);
    mod_mag = (uint32_t)(modulator_q15 < 0
                             ? -(int64_t)modulator_q15 : modulator_q15);
    product = (uint64_t)sample_mag * mod_mag;
    result_mag = (uint32_t)((product + 16384u) >> 15);

    if (negative) {
        if (result_mag > 32768u)
            result_mag = 32768u;
        return -(int32_t)result_mag;
    }

    if (result_mag > 32767u)
        result_mag = 32767u;
    return (int32_t)result_mag;
}

#endif /* ENG_METAL_CORE_METAL_H */
