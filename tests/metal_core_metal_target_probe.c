/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — AC79 compile-only probe for the isolated METAL stage.
 *
 * This translation unit checks target compiler/code-generation compatibility.
 * It is not linked into firmware, executed on the device, or cycle-measured.
 */
#include <stdint.h>
#include "../firmware/src/eng_metal_core_metal.h"

#if defined(__clang__) || defined(__GNUC__)
#define METAL_PROBE_NOINLINE __attribute__((noinline, used))
#else
#define METAL_PROBE_NOINLINE
#endif

METAL_PROBE_NOINLINE int32_t eng_metal_metal_target_probe_ring(
    int32_t sample_q15, int32_t modulator_q15, uint32_t enabled)
{
    return eng_metal_ring_apply_q15(sample_q15, modulator_q15, enabled);
}

METAL_PROBE_NOINLINE uint32_t eng_metal_metal_target_probe_ratio(
    uint32_t carrier_inc, uint32_t ratio_q16)
{
    return eng_metal_ratio_increment(carrier_inc, ratio_q16);
}
