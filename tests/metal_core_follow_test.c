/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated FOLLOW fixed-point pitch tests.
 *
 * Host-only math tests; no host note provenance or engine render is exercised.
 */
#include "../firmware/src/eng_metal_core_follow.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal FOLLOW: %-66s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static int near_u32(uint32_t actual, double expected, double tolerance)
{
    double d = (double)actual - expected;
    if (d < 0.0) d = -d;
    return d <= tolerance;
}

static void test_ratio_table(void)
{
    uint32_t root = eng_metal_follow_ratio_q24(-1, 0);
    uint32_t root_up = eng_metal_follow_ratio_q24(-1, 10);
    uint32_t root_down = eng_metal_follow_ratio_q24(-1, -10);
    uint32_t c = eng_metal_follow_ratio_q24(0, 0);
    uint32_t fifth = eng_metal_follow_ratio_q24(7, 0);
    uint32_t b = eng_metal_follow_ratio_q24(11, 0);
    uint32_t fifth_plus_02 = eng_metal_follow_ratio_q24(7, 2);
    uint32_t max = eng_metal_follow_ratio_q24(11, 20);

    check("ROOT at zero octave is unity",
          root == ENG_METAL_FOLLOW_Q24_ONE);
    check("ROOT plus one octave doubles frequency",
          root_up == 2u * ENG_METAL_FOLLOW_Q24_ONE);
    check("ROOT minus one octave halves frequency",
          root_down == ENG_METAL_FOLLOW_Q24_ONE / 2u);
    check("NOTE C at zero NOTE OCTAVE is unity",
          c == ENG_METAL_FOLLOW_Q24_ONE);
    check("NOTE G is the equal-tempered seven-semitone ratio",
          near_u32(fifth, pow(2.0, 7.0 / 12.0) * ENG_METAL_FOLLOW_Q24_ONE, 1.0));
    check("NOTE B is the equal-tempered eleven-semitone ratio",
          near_u32(b, pow(2.0, 11.0 / 12.0) * ENG_METAL_FOLLOW_Q24_ONE, 1.0));
    check("NOTE OCTAVE 0.2 applies the fractional octave ratio",
          near_u32(fifth_plus_02,
                   pow(2.0, 7.0 / 12.0 + 0.2) * ENG_METAL_FOLLOW_Q24_ONE, 2.0));
    check("maximum documented note/octave ratio remains bounded",
          max > b && max < 8u * ENG_METAL_FOLLOW_Q24_ONE);
}

static void test_bounds_and_invalid_inputs(void)
{
    check("octave values clamp to the documented -2..+2 range",
          eng_metal_follow_ratio_q24(-1, -100) ==
              eng_metal_follow_ratio_q24(-1, -20) &&
          eng_metal_follow_ratio_q24(-1, 100) ==
              eng_metal_follow_ratio_q24(-1, 20));
    check("invalid note values are rejected without table access",
          eng_metal_follow_ratio_q24(-2, 0) == 0u &&
          eng_metal_follow_ratio_q24(12, 0) == 0u);
    check("zero sample rate and invalid note return zero phase increment",
          eng_metal_follow_phase_inc(160u, 0u, -1, 0) == 0u &&
          eng_metal_follow_phase_inc(160u, 44100u, 12, 0) == 0u);
    check("base FREQ clamps to the documented 30..500 Hz range",
          eng_metal_follow_phase_inc(1u, 44100u, -1, 0) ==
              eng_metal_follow_phase_inc(30u, 44100u, -1, 0) &&
          eng_metal_follow_phase_inc(900u, 44100u, -1, 0) ==
              eng_metal_follow_phase_inc(500u, 44100u, -1, 0));
}

static void test_phase_increment(void)
{
    uint32_t base = eng_metal_follow_phase_inc(160u, 44100u, -1, 0);
    uint32_t octave = eng_metal_follow_phase_inc(160u, 44100u, -1, 10);
    uint32_t note = eng_metal_follow_phase_inc(160u, 44100u, 7, 0);
    uint32_t combined = eng_metal_follow_phase_inc(500u, 32000u, 11, 20);
    double scale = 4294967296.0;

    check("base FREQ converts to a Q0.32 phase increment",
          near_u32(base, 160.0 * scale / 44100.0, 1.0));
    check("one-octave FOLLOW increment is exactly twice the base increment",
          octave == base * 2u);
    check("semitone mapping matches the equal-tempered frequency",
          near_u32(note, 160.0 * pow(2.0, 7.0 / 12.0) * scale / 44100.0, 2.0));
    check("maximum base-note mapping stays below Nyquist at 32 kHz",
          combined < 0x80000000u &&
          near_u32(combined,
                   500.0 * pow(2.0, 11.0 / 12.0 + 2.0) * scale / 32000.0,
                   1.0));
}

static void test_source_selector(void)
{
    const uint32_t base = 12345u, note = 67890u;

    check("sequencer + FOLLOW ON selects the tracked step pitch",
          eng_metal_follow_select_inc(1, 1, base, note) == note);
    check("sequencer + FOLLOW OFF restores the stored base FREQ",
          eng_metal_follow_select_inc(0, 1, base, note) == base);
    check("live/performance note tracks with FOLLOW ON",
          eng_metal_follow_select_inc(1, 0, base, note) == note);
    check("live/performance note remains tracked with FOLLOW OFF",
          eng_metal_follow_select_inc(0, 0, base, note) == note);
}

int main(void)
{
    test_ratio_table();
    test_bounds_and_invalid_inputs();
    test_phase_increment();
    test_source_selector();
    printf("metal FOLLOW: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
