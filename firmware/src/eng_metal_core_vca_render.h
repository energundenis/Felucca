/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — VCA-only render harness primitives.
 * Shared by the host harness and the AC79 compile-only target probe.
 * This is not a registered production engine.
 */
#ifndef ENG_METAL_CORE_VCA_RENDER_H
#define ENG_METAL_CORE_VCA_RENDER_H

#include "eng_metal_core_vca.h"

#define ENG_METAL_VCA_VOICE_COUNT 8u

typedef struct {
    eng_metal_vca_t voice[ENG_METAL_VCA_VOICE_COUNT];
} eng_metal_vca_part_state_t;

_Static_assert(sizeof(eng_metal_vca_t) == 36u,
               "METAL CORE VCA ABI must remain 36 bytes");
_Static_assert(sizeof(eng_metal_vca_part_state_t) == 288u,
               "eight METAL CORE VCA states must occupy 288 bytes");

/* Q16 VCA x Q15 host amp ramp. Keep the product in 64 bits until Q31 shift. */
static inline int32_t eng_metal_vca_apply_host_amp(int32_t sample,
                                                    int32_t vca_q16,
                                                    int32_t host_amp_q15)
{
    uint32_t gain_q31;
    int64_t product;
    if (vca_q16 < 0) vca_q16 = 0;
    if (vca_q16 > ENG_METAL_VCA_Q16_MAX)
        vca_q16 = ENG_METAL_VCA_Q16_MAX;
    if (host_amp_q15 < 0) host_amp_q15 = 0;
    if (host_amp_q15 > 32767) host_amp_q15 = 32767;
    /* The bounded 17 x 15-bit gain product fits uint32_t; keep only the
     * sample x gain operation as a 64-bit multiply in the render path. */
    gain_q31 = (uint32_t)vca_q16 * (uint32_t)host_amp_q15;
    product = (int64_t)sample * (int64_t)gain_q31;
    return (int32_t)(product >> 31);
}

/* Felucca's CTL=32 ramp convention: advance before each sample, with the
 * signed delta shifted by five. Output is additive, never overwritten. */
static inline void eng_metal_vca_render_constant(eng_metal_vca_t *vca,
                                                  int32_t *out,
                                                  uint32_t n,
                                                  int32_t source,
                                                  int32_t amp0_q15,
                                                  int32_t amp1_q15)
{
    uint32_t i;
    int32_t amp = amp0_q15;
    int32_t da = (amp1_q15 - amp0_q15) >> 5;
    if (!vca || !out)
        return;
    for (i = 0; i < n; ++i) {
        int32_t level;
        amp += da;
        level = eng_metal_vca_process(vca);
        out[i] += eng_metal_vca_apply_host_amp(source, level, amp);
    }
}

#endif
