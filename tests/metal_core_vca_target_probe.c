/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — AC79/pi32v2 compile and code-generation probe.
 *
 * Compile-only evidence. This does not register an engine, link firmware,
 * execute on AC79, or measure target cycles.
 */
#include "../firmware/src/eng_metal_core_vca.h"

_Static_assert(sizeof(eng_metal_vca_t) == 36u,
               "AC79 VCA state layout must remain 36 bytes");

#if defined(__clang__) || defined(__GNUC__)
#define VCA_PROBE __attribute__((noinline, used))
#else
#define VCA_PROBE
#endif

VCA_PROBE int32_t eng_metal_vca_target_probe_process(eng_metal_vca_t *vca)
{
    return eng_metal_vca_process(vca);
}

VCA_PROBE int32_t eng_metal_vca_target_probe_block32(eng_metal_vca_t *vca)
{
    uint32_t i;
    int32_t value = 0;
    for (i = 0; i < 32u; ++i)
        value = eng_metal_vca_process(vca);
    return value;
}

VCA_PROBE uint32_t eng_metal_vca_target_probe_size(void)
{
    return (uint32_t)sizeof(eng_metal_vca_t);
}
