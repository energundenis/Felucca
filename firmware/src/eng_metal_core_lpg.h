/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated sample-rate LPG gain envelope.
 *
 * Fixed-point Q15 level / Q30 one-pole coefficient. No allocation, floating
 * point, table lookup, or division in the per-sample processing function.
 */
#ifndef ENG_METAL_CORE_LPG_H
#define ENG_METAL_CORE_LPG_H

#include <stdint.h>
#include "eng_metal_core_math.h"

#define ENG_METAL_LPG_ENV_IDLE   0u
#define ENG_METAL_LPG_ENV_ATTACK 1u
#define ENG_METAL_LPG_ENV_DECAY  2u
#define ENG_METAL_LPG_ENV_Q30_ONE 1073741824u
#define ENG_METAL_LPG_ENV_FLOOR  3

typedef struct {
    int32_t level_q15;
    int32_t target_q15;
    uint32_t coefficient_q30;
    uint32_t samples_to_decay;
    uint32_t decay_coefficient_q30;
    uint8_t phase;
} eng_metal_lpg_env_t;

/* Compute the one-pole coefficient at event time, never in the sample loop.
 * alpha ~= 1/tau is a stable first-order approximation to 1-exp(-1/tau).
 */
static inline uint32_t eng_metal_lpg_env_coefficient(uint32_t tau_samples)
{
    if (!tau_samples)
        tau_samples = 1u;
    return (uint32_t)(ENG_METAL_LPG_ENV_Q30_ONE / tau_samples);
}

static inline void eng_metal_lpg_env_reset(eng_metal_lpg_env_t *env)
{
    if (!env)
        return;
    env->level_q15 = 0;
    env->target_q15 = 0;
    env->coefficient_q30 = 0;
    env->samples_to_decay = 0;
    env->decay_coefficient_q30 = 0;
    env->phase = ENG_METAL_LPG_ENV_IDLE;
}

/* Retrigger starts from the current level (Web Audio cancel-and-hold behavior)
 * rather than resetting to zero. timing is the CP2.3 source-derived schedule.
 */
static inline void eng_metal_lpg_env_trigger(eng_metal_lpg_env_t *env,
                                              int32_t peak_q15,
                                              const eng_metal_lpg_timing_t *timing)
{
    uint32_t attack_samples;
    if (!env || !timing)
        return;

    if (peak_q15 < 0) peak_q15 = 0;
    if (peak_q15 > ENG_METAL_Q15_ONE) peak_q15 = ENG_METAL_Q15_ONE;

    attack_samples = timing->attack_samples ? timing->attack_samples : 1u;
    env->target_q15 = peak_q15;
    env->coefficient_q30 = eng_metal_lpg_env_coefficient(attack_samples);
    env->decay_coefficient_q30 =
        eng_metal_lpg_env_coefficient(timing->gain_decay_tau_samples);
    env->samples_to_decay = timing->tail_samples;

    if (!timing->tail_samples) {
        env->target_q15 = ENG_METAL_LPG_ENV_FLOOR;
        env->coefficient_q30 = env->decay_coefficient_q30;
        env->phase = ENG_METAL_LPG_ENV_DECAY;
    } else {
        env->phase = ENG_METAL_LPG_ENV_ATTACK;
    }
}

/* One sample. Returns the current nonnegative Q15 gain. */
static inline int32_t eng_metal_lpg_env_process(eng_metal_lpg_env_t *env)
{
    int32_t delta;
    uint32_t magnitude;
    uint64_t product;
    uint32_t step;

    if (!env || env->phase == ENG_METAL_LPG_ENV_IDLE)
        return 0;

    delta = env->target_q15 - env->level_q15;
    if (delta) {
        magnitude = (uint32_t)(delta < 0 ? -delta : delta);
        product = (uint64_t)magnitude * env->coefficient_q30;
        step = (uint32_t)((product + (1ull << 29)) >> 30);
        if (!step)
            step = 1u;
        if (step >= magnitude)
            env->level_q15 = env->target_q15;
        else if (delta > 0)
            env->level_q15 += (int32_t)step;
        else
            env->level_q15 -= (int32_t)step;
    }

    if (env->phase == ENG_METAL_LPG_ENV_ATTACK) {
        if (env->samples_to_decay)
            --env->samples_to_decay;
        if (!env->samples_to_decay) {
            env->target_q15 = ENG_METAL_LPG_ENV_FLOOR;
            env->coefficient_q30 = env->decay_coefficient_q30;
            env->phase = ENG_METAL_LPG_ENV_DECAY;
        }
    } else if (env->phase == ENG_METAL_LPG_ENV_DECAY &&
               env->level_q15 <= ENG_METAL_LPG_ENV_FLOOR) {
        env->level_q15 = 0;
        env->target_q15 = 0;
        env->phase = ENG_METAL_LPG_ENV_IDLE;
    }

    return env->level_q15;
}

#endif
