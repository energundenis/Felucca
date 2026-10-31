/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated sample-by-sample LPG envelope tests.
 *
 * Host-only deterministic tests. This does not render a filter or engine.
 */
#include "../firmware/src/eng_metal_core_lpg.h"

#include <stdio.h>
#include <stdint.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal LPG envelope: %-60s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

int main(void)
{
    eng_metal_lpg_env_t env;
    eng_metal_lpg_timing_t timing = { 66u, 200u, 100u, 50u, 80u, 8000u, 2000u };
    int32_t previous, value, level_at_gate, level_after_decay;
    uint32_t i;
    int monotonic = 1;

    eng_metal_lpg_env_reset(&env);
    check("reset is silent and idle",
          eng_metal_lpg_env_process(&env) == 0 &&
          env.phase == ENG_METAL_LPG_ENV_IDLE);

    eng_metal_lpg_env_trigger(&env, 20000, &timing);
    previous = 0;
    for (i = 0; i < 66u; ++i) {
        value = eng_metal_lpg_env_process(&env);
        if (value < previous) monotonic = 0;
        previous = value;
    }
    check("attack rises monotonically from silence", monotonic && previous > 12000);
    check("attack reaches approximately 63 percent after one time constant",
          previous >= 12000 && previous <= 13500);
    check("attack remains active until the gate-tail event",
          env.phase == ENG_METAL_LPG_ENV_ATTACK &&
          env.samples_to_decay == 134u);

    for (i = 66u; i < 200u; ++i)
        eng_metal_lpg_env_process(&env);
    level_at_gate = env.level_q15;
    check("gate event switches to decay without a level discontinuity",
          env.phase == ENG_METAL_LPG_ENV_DECAY &&
          level_at_gate > 19000 && level_at_gate <= 20000);

    for (i = 0; i < 100u; ++i)
        eng_metal_lpg_env_process(&env);
    level_after_decay = env.level_q15;
    check("decay follows a bounded one-pole time constant",
          level_after_decay > 5000 && level_after_decay < 7000 &&
          level_after_decay < level_at_gate);

    for (i = 0; i < 3000u && env.phase != ENG_METAL_LPG_ENV_IDLE; ++i)
        eng_metal_lpg_env_process(&env);
    check("decay reaches silence and returns to idle",
          env.phase == ENG_METAL_LPG_ENV_IDLE && env.level_q15 == 0 &&
          eng_metal_lpg_env_process(&env) == 0);

    /* Retrigger must hold the instantaneous level and move toward the new peak. */
    eng_metal_lpg_env_reset(&env);
    timing.tail_samples = 1000u;
    eng_metal_lpg_env_trigger(&env, 18000, &timing);
    for (i = 0; i < 30u; ++i)
        eng_metal_lpg_env_process(&env);
    previous = env.level_q15;
    eng_metal_lpg_env_trigger(&env, 24000, &timing);
    value = eng_metal_lpg_env_process(&env);
    check("retrigger holds current level instead of restarting at zero",
          value >= previous && value - previous < 1000 &&
          env.phase == ENG_METAL_LPG_ENV_ATTACK);

    /* A retrigger during decay must hold the decaying value and rise smoothly. */
    for (i = 0; i < 1000u; ++i)
        eng_metal_lpg_env_process(&env);
    for (i = 0; i < 100u; ++i)
        eng_metal_lpg_env_process(&env);
    previous = env.level_q15;
    eng_metal_lpg_env_trigger(&env, 26000, &timing);
    value = eng_metal_lpg_env_process(&env);
    check("retrigger during decay holds level and rises without a reset",
          value >= previous && value - previous < 1000 &&
          env.phase == ENG_METAL_LPG_ENV_ATTACK);

    /* Zero-length gate enters decay immediately and never sticks in attack. */
    timing.tail_samples = 0u;
    eng_metal_lpg_env_trigger(&env, 32767, &timing);
    check("zero gate enters decay immediately",
          env.phase == ENG_METAL_LPG_ENV_DECAY &&
          env.target_q15 == ENG_METAL_LPG_ENV_FLOOR);

    eng_metal_lpg_env_reset(&env);
    timing.tail_samples = 1000u;
    eng_metal_lpg_env_trigger(&env, 50000, &timing);
    check("trigger clamps peak to Q15 full scale",
          env.target_q15 == ENG_METAL_Q15_ONE);
    eng_metal_lpg_env_trigger(0, 100, &timing);
    eng_metal_lpg_env_reset(0);
    check("null state and timing pointers are safe",
          eng_metal_lpg_env_process(0) == 0);

    printf("metal LPG envelope: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
