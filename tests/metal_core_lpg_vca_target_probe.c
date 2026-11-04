/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — AC79/pi32v2 LPG gain + VCA combined block probe.
 *
 * Compile/codegen-only probe for the two independent envelope states and
 * their serial gain application. Not registered or executed on the target.
 */
#include "../firmware/src/eng_metal_core_lpg.h"
#include "../firmware/src/eng_metal_core_vca_render.h"

typedef struct {
    eng_metal_vca_t vca;
    eng_metal_lpg_env_t lpg;
} eng_metal_lpg_vca_target_state_t;

_Static_assert(sizeof(eng_metal_vca_t) == 36u,
               "AC79 LPG+VCA probe VCA state ABI");
_Static_assert(sizeof(eng_metal_lpg_vca_target_state_t) <= 64u,
               "AC79 LPG+VCA per-voice state must stay compact");

__attribute__((noinline, used))
void eng_metal_lpg_vca_target_probe_gate_off(
    eng_metal_lpg_vca_target_state_t *state)
{
    if (!state)
        return;
    eng_metal_vca_gate_off(&state->vca);
    eng_metal_lpg_env_gate_off(&state->lpg);
}

__attribute__((noinline, used))
void eng_metal_lpg_vca_target_probe_block(
    eng_metal_lpg_vca_target_state_t *state,
    int32_t *out, uint32_t n, int32_t source,
    int32_t amp0_q15, int32_t amp1_q15)
{
    uint32_t i;
    int32_t amp, da;
    if (!state || !out)
        return;
    if (n > 32u)
        n = 32u;
    amp = amp0_q15;
    da = (amp1_q15 - amp0_q15) >> 5;
    for (i = 0; i < n; ++i) {
        int32_t vca_level, lpg_level, sample;
        amp += da;
        vca_level = eng_metal_vca_process(&state->vca);
        lpg_level = eng_metal_lpg_env_process(&state->lpg);
        sample = eng_metal_vca_apply_host_amp(source, vca_level, amp);
        out[i] += (int32_t)(((int64_t)sample * lpg_level) >> 15);
    }
}
