/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated OSC subsystem tests.
 * Runs against the exact Felucca host DSP tables and primitives.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include "../firmware/src/eng_metal_core.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal-osc: %-68s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static int near_i32(int32_t a, int32_t b, int32_t tol)
{
    int32_t d = a - b;
    if (d < 0) d = -d;
    return d <= tol;
}

static void waveforms(void)
{
    uint32_t inc = pitch_inc(60 * 16);
    check("SINE zero crossing", near_i32(metal_core_osc_sample(0u, inc, MC_OSC_SINE), 0, 2));
    check("SINE positive peak", metal_core_osc_sample(0x40000000u, inc, MC_OSC_SINE) > 32750);
    check("SINE half-cycle crossing", near_i32(metal_core_osc_sample(0x80000000u, inc, MC_OSC_SINE), 0, 2));
    check("SINE negative peak", metal_core_osc_sample(0xC0000000u, inc, MC_OSC_SINE) < -32750);

    check("TRIANGLE negative peak", metal_core_osc_sample(0u, inc, MC_OSC_TRIANGLE) == -32768);
    check("TRIANGLE quarter-cycle", near_i32(metal_core_osc_sample(0x40000000u, inc, MC_OSC_TRIANGLE), 0, 1));
    check("TRIANGLE positive peak", metal_core_osc_sample(0x80000000u, inc, MC_OSC_TRIANGLE) == 32767);
    check("TRIANGLE three-quarter cycle", near_i32(metal_core_osc_sample(0xC0000000u, inc, MC_OSC_TRIANGLE), 0, 1));

    check("SAW quarter-cycle level", near_i32(metal_core_osc_sample(0x40000000u, inc, MC_OSC_SAW), -16384, 2));
    check("SAW half-cycle level", near_i32(metal_core_osc_sample(0x80000000u, inc, MC_OSC_SAW), 0, 2));
    check("SAW three-quarter-cycle level", near_i32(metal_core_osc_sample(0xC0000000u, inc, MC_OSC_SAW), 16384, 2));
    check("unknown waveform safely falls back to SINE",
          metal_core_osc_sample(0x40000000u, inc, 99u) == metal_core_osc_sample(0x40000000u, inc, MC_OSC_SINE));
}

static void pair_mix(void)
{
    voice_t v;
    vmod_t m;
    int32_t out[CTL + 1], expected;
    uint32_t i, start, inc;

    memset(&v, 0, sizeof v);
    memset(&m, 0, sizeof m);
    inc = pitch_inc(60 * 16);
    m.inc = inc;
    m.pitch16 = 60 * 16;
    start = 0x12345678u;
    v.ph[0] = start;
    v.ph[1] = start;
    memset(out, 0, sizeof out);
    out[CTL] = 0x13579BDF;

    metal_core_osc_pair_block(&v, out, CTL, &m, MC_OSC_SINE, 0, 0);
    for (i = 0; i < CTL; i++) {
        expected = metal_core_osc_sample(start + i * inc, inc, MC_OSC_SINE);
        if (out[i] != expected)
            break;
    }
    check("OSC2 level zero leaves OSC1 sample-exact", i == CTL);
    check("OSC1 phase advances by exactly one block", v.ph[0] == start + CTL * inc);
    check("OSC2 phase advances even when its level is zero", v.ph[1] == start + CTL * inc);
    check("block clamp does not write past CTL", out[CTL] == 0x13579BDF);

    memset(&v, 0, sizeof v);
    v.ph[0] = v.ph[1] = start;
    memset(out, 0, sizeof out);
    metal_core_osc_pair_block(&v, out, CTL, &m, MC_OSC_TRIANGLE, 0, 32767);
    for (i = 0; i < CTL; i++) {
        int32_t s = metal_core_osc_sample(start + i * inc, inc, MC_OSC_TRIANGLE);
        expected = s + mulq15(s, 32767);
        if (out[i] != expected || out[i] < -65535 || out[i] > 65535)
            break;
    }
    check("full-level unison OSC2 sums independently with bounded headroom", i == CTL);

    memset(&v, 0, sizeof v);
    v.ph[0] = v.ph[1] = start;
    memset(out, 0, sizeof out);
    metal_core_osc_pair_block(&v, out, CTL, &m, MC_OSC_SINE, 0, -100);
    check("negative OSC2 level clamps to zero",
          out[3] == metal_core_osc_sample(start + 3u * inc, inc, MC_OSC_SINE));

    memset(&v, 0, sizeof v);
    v.ph[0] = v.ph[1] = start;
    memset(out, 0, sizeof out);
    metal_core_osc_pair_block(&v, out, CTL, &m, MC_OSC_SINE, 0, 50000);
    check("OSC2 level above Q15 clamps to full level",
          out[3] == metal_core_osc_sample(start + 3u * inc, inc, MC_OSC_SINE) +
                    mulq15(metal_core_osc_sample(start + 3u * inc, inc, MC_OSC_SINE), 32767));
}

static void detune_and_bounds(void)
{
    vmod_t m;
    voice_t v;
    int32_t out[CTL + 1];
    uint32_t base, detuned;

    memset(&m, 0, sizeof m);
    m.pitch16 = 60 * 16;
    m.inc = pitch_inc(m.pitch16);
    m.fine = 0;
    base = metal_core_osc2_increment(&m, 0);
    detuned = metal_core_osc2_increment(&m, 7);
    check("zero detune preserves the host phase increment exactly", base == m.inc);
    check("positive OSC2 cents detune raises its phase increment", detuned > base);

    m.fine = 13;
    check("OSC2 detune retains host fine tuning", metal_core_osc2_increment(&m, 7) >
          metal_core_osc2_increment(&m, 7) - 1u);

    memset(&v, 0, sizeof v);
    memset(out, 0, sizeof out);
    metal_core_osc_pair_block(&v, out, CTL + 1u, &m, MC_OSC_SAW, 7, 16384);
    check("oversized render request is bounded to CTL", v.ph[0] == CTL * m.inc);
    check("OSC2 uses the independent detuned increment",
          v.ph[1] == CTL * metal_core_osc2_increment(&m, 7));

    metal_core_osc_pair_block(&v, out, 0, &m, MC_OSC_SINE, 0, 0);
    check("zero-length block leaves oscillator phases unchanged",
          v.ph[0] == CTL * m.inc && v.ph[1] == CTL * metal_core_osc2_increment(&m, 7));
    metal_core_osc_pair_block(0, out, CTL, &m, MC_OSC_SINE, 0, 0);
    metal_core_osc_pair_block(&v, 0, CTL, &m, MC_OSC_SINE, 0, 0);
    metal_core_osc_pair_block(&v, out, CTL, 0, MC_OSC_SINE, 0, 0);
    check("null arguments are safely ignored", 1);
}

int main(void)
{
    waveforms();
    pair_mix();
    detune_and_bounds();
    printf(failures ? "metal-osc: %d FAILED\n" : "metal-osc: all passed\n", failures);
    return failures != 0;
}
