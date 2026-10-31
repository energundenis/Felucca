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


/* Source-derived LPG schedule, expressed in target sample counts.
 * This models the v98 setTargetAtTime time constants and gate-limited tail;
 * the Option C peak multiplier is supplied separately by eng_metal_lpg_peak_q15().
 */
typedef struct {
    uint32_t attack_samples;
    uint32_t tail_samples;
    uint32_t gain_decay_tau_samples;
    uint32_t filter_return_samples;
    uint32_t filter_decay_tau_samples;
    uint32_t filter_transient_hz;
    uint32_t filter_return_hz;
} eng_metal_lpg_timing_t;

static inline uint32_t eng_metal_us_to_samples(uint32_t sample_rate,
                                                uint32_t time_us)
{
    return (uint32_t)(((uint64_t)sample_rate * time_us + 500000u) /
                      1000000u);
}

static inline uint32_t eng_metal_lpg_mul_q15_u32(uint32_t value,
                                                  uint32_t factor_q15)
{
    return (uint32_t)(((uint64_t)value * factor_q15 + 16383u) /
                      ENG_METAL_Q15_ONE);
}

static inline void eng_metal_lpg_timing_q15(uint32_t sample_rate,
                                             uint32_t decay_us,
                                             uint32_t gate_samples,
                                             int32_t response_q15,
                                             uint32_t base_cutoff_hz,
                                             eng_metal_lpg_timing_t *out)
{
    uint32_t response, decay_samples, attack_us;
    uint32_t gain_tau_factor, filter_tau_factor, cutoff_factor;
    uint32_t gain_tau_us, filter_tau_us, return_samples;

    if (!out)
        return;
    out->attack_samples = 0;
    out->tail_samples = 0;
    out->gain_decay_tau_samples = 0;
    out->filter_return_samples = 0;
    out->filter_decay_tau_samples = 0;
    out->filter_transient_hz = 0;
    out->filter_return_hz = 0;
    if (!sample_rate)
        return;

    response = (uint32_t)(response_q15 < 0 ? 0 :
                response_q15 > ENG_METAL_Q15_ONE ? ENG_METAL_Q15_ONE :
                response_q15);
    if (decay_us < 20000u) decay_us = 20000u;
    if (decay_us > 2000000u) decay_us = 2000000u;

    /* v98: attack = 1.5 ms + RESPONSE * 6 ms. */
    attack_us = 1500u + eng_metal_lpg_mul_q15_u32(6000u, response);
    decay_samples = eng_metal_us_to_samples(sample_rate, decay_us);
    out->attack_samples = eng_metal_us_to_samples(sample_rate, attack_us);
    out->tail_samples = gate_samples < decay_samples ? gate_samples : decay_samples;

    /* v98: gain time constant = max(6 ms, DECAY * (0.08 + RESPONSE * 0.18)). */
    gain_tau_factor = 2621u + eng_metal_lpg_mul_q15_u32(5898u, response);
    gain_tau_us = eng_metal_lpg_mul_q15_u32(decay_us, gain_tau_factor);
    if (gain_tau_us < 6000u) gain_tau_us = 6000u;
    out->gain_decay_tau_samples = eng_metal_us_to_samples(sample_rate, gain_tau_us);

    /* v98: filter returns after min(gate, DECAY * 0.22). */
    return_samples = eng_metal_us_to_samples(sample_rate,
                          (uint32_t)(((uint64_t)decay_us * 22u + 50u) / 100u));
    out->filter_return_samples = gate_samples < return_samples ?
                                 gate_samples : return_samples;

    /* v98: filter time constant = max(12 ms, DECAY * (0.08 + RESPONSE * 0.12)). */
    filter_tau_factor = 2621u + eng_metal_lpg_mul_q15_u32(3932u, response);
    filter_tau_us = eng_metal_lpg_mul_q15_u32(decay_us, filter_tau_factor);
    if (filter_tau_us < 12000u) filter_tau_us = 12000u;
    out->filter_decay_tau_samples = eng_metal_us_to_samples(sample_rate, filter_tau_us);

    /* v98: min(12000 Hz, base cutoff * (1.25 + RESPONSE * 3.75)). */
    cutoff_factor = 40959u + eng_metal_lpg_mul_q15_u32(122876u, response);
    out->filter_transient_hz = eng_metal_lpg_mul_q15_u32(base_cutoff_hz,
                                                        cutoff_factor);
    if (out->filter_transient_hz > 12000u)
        out->filter_transient_hz = 12000u;
    out->filter_return_hz = base_cutoff_hz < 100u ? 100u : base_cutoff_hz;
}

#endif
