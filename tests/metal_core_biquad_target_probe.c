/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — AC79 pi32v2 compile-only DF2T probe.
 * Host-only candidate header; this object is not linked into firmware.
 */
#include <stdint.h>
#include "metal_core_biquad_candidate.h"

#if defined(__clang__) || defined(__GNUC__)
#define BIQUAD_PROBE __attribute__((noinline, used))
#else
#define BIQUAD_PROBE
#endif

BIQUAD_PROBE int32_t eng_metal_biquad_target_probe_sample(
    const test_metal_biquad_coeff_q30_t *coeff,
    test_metal_biquad_state_t *state,
    int32_t input)
{
    return test_metal_biquad_process_q30(coeff, state, input, 0);
}

BIQUAD_PROBE int32_t eng_metal_biquad_target_probe_block32(
    const test_metal_biquad_coeff_q30_t *coeff,
    test_metal_biquad_state_t *state,
    const int32_t *input)
{
    uint32_t i;
    int32_t output = 0;
    for (i = 0; i < 32u; ++i)
        output = test_metal_biquad_process_q30(coeff, state, input[i], 0);
    return output;
}

BIQUAD_PROBE uint32_t eng_metal_biquad_target_probe_state_size(void)
{
    return (uint32_t)sizeof(test_metal_biquad_state_t);
}
