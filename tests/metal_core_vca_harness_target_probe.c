/* SPDX-License-Identifier: GPL-3.0-only
 * CP2.23 AC79 compile-only probe for the combined VCA + host amp ramp path.
 * No engine registration, firmware link, or device write is performed.
 */
#include "../firmware/src/eng_metal_core_vca_render.h"

_Static_assert(sizeof(eng_metal_vca_t) == 36u, "AC79 VCA state ABI");
_Static_assert(sizeof(eng_metal_vca_part_state_t) == 288u,
               "AC79 eight-voice VCA memory");

__attribute__((noinline))
int32_t eng_metal_vca_harness_target_sample(eng_metal_vca_t *vca,
                                             int32_t source,
                                             int32_t host_amp_q15)
{
    return eng_metal_vca_apply_host_amp(source,
                                        eng_metal_vca_process(vca),
                                        host_amp_q15);
}

__attribute__((noinline))
void eng_metal_vca_harness_target_block32(eng_metal_vca_t *vca,
                                          int32_t *out,
                                          int32_t amp0_q15,
                                          int32_t amp1_q15)
{
    eng_metal_vca_render_constant(vca, out, 32u, 12000,
                                  amp0_q15, amp1_q15);
}
