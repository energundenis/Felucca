/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — Complex Oscillator modulator source tests.
 */
#define NVOICE 8
#define NPART 4
#include "../firmware/engines/src/felucca_engine.h"
#include "felucca_tables.h"
#include "../firmware/engines/src/dsp.c"
#include "../firmware/src/eng_metal_core.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal-fm-mod: %-68s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static int near_i32(int32_t a, int32_t b, int32_t tol)
{
    int32_t d = a - b;
    if (d < 0) d = -d;
    return d <= tol;
}

static void waveform_set(void)
{
    uint32_t inc = pitch_inc(60 * 16);
    check("SINE positive peak", metal_core_modulator_sample(0x40000000u, inc, MC_MOD_SINE) > 32750);
    check("TRIANGLE positive peak", metal_core_modulator_sample(0x80000000u, inc, MC_MOD_TRIANGLE) == 32767);
    check("SAW quarter-cycle level", near_i32(metal_core_modulator_sample(0x40000000u, inc, MC_MOD_SAW), -16384, 2));
    check("SQUARE positive half-cycle", metal_core_modulator_sample(0xA0000000u, inc, MC_MOD_SQUARE) > 32760);
    check("SQUARE negative half-cycle", metal_core_modulator_sample(0x20000000u, inc, MC_MOD_SQUARE) < -32760);
    check("unknown waveform falls back to SINE",
          metal_core_modulator_sample(0x40000000u, inc, 99u) ==
          metal_core_modulator_sample(0x40000000u, inc, MC_MOD_SINE));
}

static void ratio_math(void)
{
    uint32_t base = pitch_inc(60 * 16);
    check("1.0 ratio preserves carrier increment", metal_core_ratio_increment(base, 1u << 16) == base);
    check("0.25 ratio divides increment by four", metal_core_ratio_increment(base, 1u << 14) == base / 4u);
    check("3.07 ratio uses Q16 multiplication",
          metal_core_ratio_increment(base, 201195u) == (uint32_t)(((uint64_t)base * 201195u) >> 16));
    check("16.0 ratio remains bounded below Nyquist",
          metal_core_ratio_increment(pitch_inc(127u * 16u), 16u << 16) == 0x7FFFFFFFu);
}

static void block_render(void)
{
    voice_t v;
    vmod_t m;
    int32_t out[CTL + 1];
    uint32_t i, start, inc;

    memset(&v, 0, sizeof v);
    memset(&m, 0, sizeof m);
    memset(out, 0, sizeof out);
    m.pitch16 = 60 * 16;
    m.inc = pitch_inc(m.pitch16);
    start = 0x23456789u;
    v.ph[2] = start;
    inc = metal_core_ratio_increment(m.inc, 201195u);
    out[CTL] = 0x2468ACE0;

    metal_core_modulator_block(&v, out, CTL, &m, 201195u, MC_MOD_SINE);
    for (i = 0; i < CTL; i++) {
        int32_t want = metal_core_modulator_sample(start + i * inc, inc, MC_MOD_SINE);
        if (out[i] != want)
            break;
    }
    check("wcMod block samples follow independent ratio phase", i == CTL);
    check("wcMod phase advances exactly one block", v.ph[2] == start + CTL * inc);
    check("wcMod block does not write beyond CTL", out[CTL] == 0x2468ACE0);

    memset(&v, 0, sizeof v);
    metal_core_modulator_block(&v, out, CTL, &m, 0u, MC_MOD_SINE);
    check("MOD RATIO input clamps to v98 minimum 0.25", v.ph[2] == CTL * metal_core_ratio_increment(m.inc, 1u << 14));
    memset(&v, 0, sizeof v);
    metal_core_modulator_block(&v, out, CTL, &m, 17u << 16, MC_MOD_SINE);
    check("MOD RATIO input clamps to v98 maximum 16.0", v.ph[2] == CTL * metal_core_ratio_increment(m.inc, 16u << 16));

    {
        uint32_t before = v.ph[2];
        metal_core_modulator_block(&v, out, 0, &m, 201195u, MC_MOD_SINE);
        check("zero-length block leaves wcMod phase unchanged", v.ph[2] == before);
        metal_core_modulator_block(0, out, CTL, &m, 201195u, MC_MOD_SINE);
        metal_core_modulator_block(&v, 0, CTL, &m, 201195u, MC_MOD_SINE);
        metal_core_modulator_block(&v, out, CTL, 0, 201195u, MC_MOD_SINE);
        check("null arguments are safely ignored", v.ph[2] == before);
    }
}

int main(void)
{
    waveform_set();
    ratio_math();
    block_render();
    printf(failures ? "metal-fm-mod: %d FAILED\n" : "metal-fm-mod: all passed\n", failures);
    return failures != 0;
}
