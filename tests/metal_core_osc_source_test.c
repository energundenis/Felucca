/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated oscillator source tests.
 * Exercises the actual pinned Felucca oscillator primitives through the
 * production selector; no registered engine or hardware is involved.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include "../firmware/src/eng_metal_core_osc.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal osc source: %-68s %s\n", name, ok ? "PASS" : "FAIL");
    failures += !ok;
}

static void measure_wave(uint32_t waveform, const char *name,
                         uint32_t inc, uint32_t pulse_width, int32_t peak_min)
{
    uint32_t phase = 0u, i;
    int32_t lo = INT_MAX, hi = INT_MIN;
    int64_t sum = 0;
    for (i = 0; i < 4096u; ++i) {
        int32_t y = eng_metal_osc_next(&phase, inc, waveform, pulse_width);
        if (y < lo) lo = y;
        if (y > hi) hi = y;
        sum += y;
    }
    check(name, lo >= -32768 && hi <= 32767 &&
               lo < -peak_min && hi > peak_min &&
               llabs(sum) < 12 * 4096);
}

static void test_waveforms(void)
{
    const uint32_t inc = pitch_inc(69u * 16u);
    measure_wave(ENG_METAL_OSC_SINE, "sine: bounded full-scale output and near-zero DC",
                 inc, 0x80000000u, 30000);
    measure_wave(ENG_METAL_OSC_TRI, "triangle: bounded full-scale output and near-zero DC",
                 inc, 0x80000000u, 30000);
    measure_wave(ENG_METAL_OSC_SAW, "polyBLEP saw: bounded full-scale output and near-zero DC",
                 inc, 0x80000000u, 30000);
    measure_wave(ENG_METAL_OSC_PULSE, "50% pulse: bounded half-scale output and near-zero DC",
                 inc, 0x80000000u, 15000);
}

static void test_independent_phases(void)
{
    uint32_t p1 = 0u, p2 = 0u;
    uint32_t i1 = pitch_inc(48u * 16u);
    uint32_t i2 = pitch_inc(67u * 16u);
    uint32_t i;
    for (i = 0; i < 1024u; ++i) {
        (void)eng_metal_osc_next(&p1, i1, ENG_METAL_OSC_SAW, 0x80000000u);
        (void)eng_metal_osc_next(&p2, i2, ENG_METAL_OSC_PULSE, 0x80000000u);
    }
    check("OSC1 and OSC2 keep independent phase accumulators",
          p1 == i1 * 1024u && p2 == i2 * 1024u && p1 != p2);
}

static void test_wrap_and_invalid_inputs(void)
{
    uint32_t phase = 0xFFFFFF00u;
    uint32_t before;
    int32_t y;
    y = eng_metal_osc_next(&phase, 0x00001000u,
                           ENG_METAL_OSC_SINE, 0u);
    check("phase accumulator wraps naturally at 32 bits",
          phase == 0x00000F00u && y >= -32768 && y <= 32767);

    before = phase;
    y = eng_metal_osc_next(&phase, 123u, 99u, 0x80000000u);
    check("unknown waveform returns silence and leaves phase unchanged",
          y == 0 && phase == before);
    check("null phase pointer returns silence safely",
          eng_metal_osc_next(0, 123u, ENG_METAL_OSC_SINE, 0u) == 0);
}

int main(void)
{
    test_waveforms();
    test_independent_phases();
    test_wrap_and_invalid_inputs();
    printf("metal osc source: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
