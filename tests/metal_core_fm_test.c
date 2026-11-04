/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — two-operator FM primitive host tests.
 * Host-only candidate tests; no production engine registration or audio claim.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include <stdint.h>
#include <stdio.h>

#include "../firmware/src/eng_metal_core_fm.h"

static int failures;
static void check(const char *name, int ok)
{
    printf("metal fm: %-68s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static void test_phase_offset(void)
{
    check("zero depth produces zero phase deviation",
          eng_metal_fm_phase_offset(32767, 0u) == 0 &&
          eng_metal_fm_phase_offset(-32768, 0u) == 0);
    check("positive and negative deviation are symmetric",
          eng_metal_fm_phase_offset(32767, 0x20000000u) ==
          -eng_metal_fm_phase_offset(-32767, 0x20000000u));
    check("depth input is clamped to the documented maximum",
          eng_metal_fm_phase_offset(32767, UINT32_MAX) ==
          eng_metal_fm_phase_offset(32767, ENG_METAL_FM_DEPTH_PHASE_MAX));
    check("extreme modulator sample stays within the phase span",
          eng_metal_fm_phase_offset(-32768, ENG_METAL_FM_DEPTH_PHASE_MAX) ==
          -(int32_t)ENG_METAL_FM_DEPTH_PHASE_MAX);
}

static void test_operator_independence_and_zero_depth(void)
{
    uint32_t cp = 0x12345678u, mp = 0x87654321u;
    uint32_t cp0 = cp, mp0 = mp;
    const uint32_t ci = 0x01020304u, mi = 0x03040506u;
    uint32_t i;
    int ok = 1;

    for (i = 0; i < 256u; ++i) {
        int32_t got = eng_metal_fm_sine_next(&cp, ci, &mp, mi, 0u);
        int32_t expected = osc_sine(cp0);
        if (got != expected)
            ok = 0;
        cp0 += ci;
        mp0 += mi;
    }
    check("zero-depth FM is bit-identical to the unmodulated carrier",
          ok && cp == cp0 && mp == mp0);
}

static void test_modulation_and_bounds(void)
{
    uint32_t cp_a = 0u, mp_a = 0u, cp_b = 0u, mp_b = 0u;
    uint32_t i;
    int differs = 0, bounded = 1;
    for (i = 0; i < 4096u; ++i) {
        int32_t plain = eng_metal_fm_sine_next(&cp_a, 0x02000000u,
                                                &mp_a, 0x01000000u, 0u);
        int32_t fm = eng_metal_fm_sine_next(&cp_b, 0x02000000u,
                                             &mp_b, 0x01000000u, 0x20000000u);
        if (plain != fm)
            differs = 1;
        if (fm < -32768 || fm > 32767)
            bounded = 0;
    }
    check("nonzero modulator depth changes the carrier waveform", differs);
    check("modulated carrier remains in signed 16-bit audio bounds", bounded);
    check("carrier and modulator phase accumulators advance independently",
          cp_a == cp_b && mp_a == mp_b && cp_a != mp_a);
}

static void test_invalid_state(void)
{
    uint32_t cp = 0x11111111u, mp = 0x22222222u;
    int32_t y = eng_metal_fm_sine_next(0, 1u, &mp, 2u, 3u);
    check("null carrier pointer returns silence without mutating modulator",
          y == 0 && mp == 0x22222222u);
    y = eng_metal_fm_sine_next(&cp, 1u, 0, 2u, 3u);
    check("null modulator pointer returns silence without mutating carrier",
          y == 0 && cp == 0x11111111u);
}

int main(void)
{
    test_phase_offset();
    test_operator_independence_and_zero_depth();
    test_modulation_and_bounds();
    test_invalid_state();
    printf("metal fm: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
