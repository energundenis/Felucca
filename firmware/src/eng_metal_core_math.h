/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — bounded Q15 LPG peak calculation.
 *
 * Isolated math primitive only; not yet connected to the engine render path.
 */
#ifndef ENG_METAL_CORE_MATH_H
#define ENG_METAL_CORE_MATH_H

#include <stdint.h>

#define ENG_METAL_Q15_ONE 32767

static inline int32_t eng_metal_lpg_peak_q15(int32_t response_q15,
                                              int32_t module_volume_q15)
{
    int32_t response = response_q15;
    int32_t volume = module_volume_q15;
    int32_t base_peak;

    if (response < 0) response = 0;
    if (response > ENG_METAL_Q15_ONE) response = ENG_METAL_Q15_ONE;
    if (volume < 0) volume = 0;
    if (volume > ENG_METAL_Q15_ONE) volume = ENG_METAL_Q15_ONE;

    /* v98 response contour: 0.25 + RESPONSE * 0.75, in Q15. */
    base_peak = 8192 +
        (int32_t)(((int64_t)response * 24575 + 16383) / ENG_METAL_Q15_ONE);

    /* Option C: MODULE VOLUME scales the transient peak, not its decay shape. */
    return (int32_t)(((int64_t)base_peak * volume + 16383) /
                     ENG_METAL_Q15_ONE);
}

#endif
