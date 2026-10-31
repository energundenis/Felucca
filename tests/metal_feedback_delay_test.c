/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — per-voice 3 ms feedback delay tests.
 */
#define NVOICE 8
#define NPART 4
#include "../firmware/engines/src/felucca_engine.h"
#include "felucca_tables.h"
#include "../firmware/engines/src/dsp.c"
#include "../firmware/src/eng_metal_core.c"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal-feedback: %-66s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static void exact_delay(void)
{
    metal_core_feedback_state_t state;
    uint32_t i;
    int32_t got = 1;

    memset(&state, 0, sizeof state);
    check("delay sample count is ceiling of 3 ms at build sample rate",
          MC_FEEDBACK_DELAY_SAMPLES == (FS * 3u + 999u) / 1000u);
    for (i = 0; i < MC_FEEDBACK_DELAY_SAMPLES; i++) {
        got = metal_core_feedback_delay_tick(&state, 0, (i == 0) ? 1234567 : 0);
        if (got != 0)
            break;
    }
    check("impulse is not returned before full delay", i == MC_FEEDBACK_DELAY_SAMPLES);
    got = metal_core_feedback_delay_tick(&state, 0, 0);
    check("impulse returns after exactly the configured delay", got == 1234567);
}

static void voice_isolation_and_wrap(void)
{
    metal_core_feedback_state_t state;
    uint32_t i;
    int ok = 1;

    memset(&state, 0, sizeof state);
    for (i = 0; i < MC_FEEDBACK_DELAY_SAMPLES; i++) {
        metal_core_feedback_delay_tick(&state, 0, 100 + (int32_t)i);
        if (metal_core_feedback_delay_tick(&state, 1, 0) != 0)
            ok = 0;
    }
    check("each voice has an independent delay history", ok);
    check("ring write index wraps to zero", state.write[0] == 0 && state.write[1] == 0);
    check("voice 0 returns its own first sample after wrap",
          metal_core_feedback_delay_tick(&state, 0, 0) == 100);
    check("voice 1 remains silent after voice 0 wraps",
          metal_core_feedback_delay_tick(&state, 1, 0) == 0);
}

static void reset_and_bounds(void)
{
    metal_core_feedback_state_t state;
    uint32_t i;
    memset(&state, 0, sizeof state);

    metal_core_feedback_delay_tick(&state, 2, 777);
    metal_core_feedback_delay_reset(&state, 2);
    for (i = 0; i < MC_FEEDBACK_DELAY_SAMPLES; i++)
        if (metal_core_feedback_delay_tick(&state, 2, 0) != 0)
            break;
    check("reset clears one voice's entire delay history", i == MC_FEEDBACK_DELAY_SAMPLES);
    check("reset does not alter another voice's write index",
          state.write[0] == 0 && state.write[1] == 0);
    check("invalid voice reset is ignored",
          (metal_core_feedback_delay_reset(&state, NVOICE), 1));
    check("invalid voice read returns silence",
          metal_core_feedback_delay_tick(&state, NVOICE, 99) == 0);
}

int main(void)
{
    exact_delay();
    voice_isolation_and_wrap();
    reset_and_bounds();
    printf(failures ? "metal-feedback: %d FAILED\n" : "metal-feedback: all passed\n", failures);
    return failures != 0;
}
