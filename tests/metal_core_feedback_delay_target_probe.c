/* SPDX-License-Identifier: GPL-3.0-only
 * AC79 compile-only probe for the provisional feedback delay arithmetic.
 * This is not production DSP and is never linked into the firmware image.
 */
#include <stdint.h>
#include "metal_core_feedback_delay_candidate.h"

__attribute__((noinline))
int32_t eng_metal_feedback_target_probe_interpolate(
    const test_metal_feedback_delay_t *delay)
{
    return test_metal_feedback_delay_read(delay);
}

__attribute__((noinline))
int32_t eng_metal_feedback_target_probe_gain_q15(
    int32_t delayed, uint16_t gain_q15)
{
    int64_t product = (int64_t)delayed * (int64_t)gain_q15;
    product += product >= 0 ? 16384 : -16384;
    return (int32_t)(product >> 15);
}

__attribute__((noinline))
void eng_metal_feedback_target_probe_write(
    test_metal_feedback_delay_t *delay, int32_t input)
{
    test_metal_feedback_delay_write(delay, input);
}
