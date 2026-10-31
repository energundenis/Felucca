/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — reusable Felucca oscillator primitive tests.
 *
 * Host-only tests. These exercise the pinned Felucca fixed-point primitives;
 * they do not test an ENERGUN engine callback or claim target audio quality.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal osc: %-64s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static void measure_wave(int (*fn)(uint32_t), const char *name,
                         int32_t min_limit, int32_t max_limit,
                         int64_t mean_limit)
{
    int64_t sum = 0;
    int32_t lo = INT32_MAX, hi = INT32_MIN;
    uint32_t i;
    for (i = 0; i < 1024u; i++) {
        int32_t y = fn(i << 22);
        if (y < lo) lo = y;
        if (y > hi) hi = y;
        sum += y;
    }
    check(name, lo >= min_limit && hi <= max_limit &&
                lo < -30000 && hi > 30000 &&
                llabs(sum) < mean_limit * 1024);
}

static int32_t sine_wave(uint32_t ph) { return osc_sine(ph); }
static int32_t tri_wave(uint32_t ph) { return osc_tri(ph); }

static void test_wave_tables(void)
{
    measure_wave(sine_wave, "sine table interpolation: bounded, full-scale, near-zero DC",
                 -32768, 32767, 4);
    measure_wave(tri_wave, "triangle phase mapping: bounded, full-scale, near-zero DC",
                 -32768, 32767, 4);
}

static void test_saw_and_pulse(void)
{
    int64_t saw_sum = 0, pulse_sum = 0;
    int32_t saw_lo = INT32_MAX, saw_hi = INT32_MIN;
    int32_t pulse_lo = INT32_MAX, pulse_hi = INT32_MIN;
    const uint32_t inc = pitch_inc(69u * 16u); /* A4 at the generated sample rate */
    uint32_t i;
    for (i = 0; i < 1024u; i++) {
        uint32_t ph = i << 22;
        int32_t a = osc_saw(ph, inc);
        int32_t b = osc_pulse(ph, inc, 0x80000000u);
        if (a < saw_lo) saw_lo = a;
        if (a > saw_hi) saw_hi = a;
        if (b < pulse_lo) pulse_lo = b;
        if (b > pulse_hi) pulse_hi = b;
        saw_sum += a;
        pulse_sum += b;
    }
    check("polyBLEP saw: bounded full-scale output and near-zero DC",
          saw_lo >= -32768 && saw_hi <= 32767 && saw_lo < -30000 &&
          saw_hi > 30000 && llabs(saw_sum) < 4 * 1024);
    check("50% pulse: bounded full-scale output and near-zero DC",
          pulse_lo >= -32768 && pulse_hi <= 32767 && pulse_lo < -30000 &&
          pulse_hi > 30000 && llabs(pulse_sum) < 4 * 1024);
}

static void test_pitch_table(void)
{
    uint32_t a = pitch_inc(69u * 16u);
    uint32_t b = pitch_inc(81u * 16u);
    double hz = (double)a * (double)FS / 4294967296.0;
    double octave = (double)b / (double)a;
    check("pitch table: A4 phase increment matches 440 Hz",
          fabs(hz - 440.0) < 0.02);
    check("pitch table: octave increment ratio is 2:1",
          fabs(octave - 2.0) < 0.00001);
}

static void test_independent_phase_accumulators(void)
{
    uint32_t ph[5] = {0, 0, 0, 0, 0};
    const uint32_t inc[5] = {
        pitch_inc(48u * 16u), pitch_inc(55u * 16u),
        pitch_inc(60u * 16u), pitch_inc(67u * 16u),
        pitch_inc(72u * 16u)
    };
    uint32_t i, j;
    for (i = 0; i < 1000u; i++)
        for (j = 0; j < 5u; j++)
            ph[j] += inc[j];
    check("five oscillator phases: independent accumulators remain distinct",
          ph[0] != ph[1] && ph[1] != ph[2] && ph[2] != ph[3] &&
          ph[3] != ph[4]);
}

int main(void)
{
    test_wave_tables();
    test_saw_and_pulse();
    test_pitch_table();
    test_independent_phase_accumulators();
    printf("metal osc: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
