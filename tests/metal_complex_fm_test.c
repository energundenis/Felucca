/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — audio-rate Complex FM carrier tests.
 */
#define NVOICE 8
#define NPART 4
#include "../firmware/engines/src/felucca_engine.h"
#include "felucca_tables.h"
#include "../firmware/engines/src/dsp.c"
#include "../firmware/src/eng_metal_core.c"

#include <stdio.h>
#include <string.h>

static int near_i32(int32_t a, int32_t b, int32_t tol)
{
    int32_t d = a - b;
    if (d < 0) d = -d;
    return d <= tol;
}

static int failures;

static void check(const char *name, int ok)
{
    printf("metal-complex-fm: %-60s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static void signed_step(void)
{
    uint32_t base = pitch_inc(60 * 16);
    int32_t positive = metal_core_fm_instantaneous_step(base, (uint64_t)base, 16384);
    int32_t negative = metal_core_fm_instantaneous_step(base, (uint64_t)base, -16384);
    int32_t high = metal_core_fm_instantaneous_step(base, (uint64_t)base * 1024u, 32767);
    int32_t low = metal_core_fm_instantaneous_step(base, (uint64_t)base * 1024u, -32768);

    check("positive modulator raises instantaneous frequency", (uint32_t)positive > base);
    check("negative modulator lowers instantaneous frequency", negative < (int32_t)base);
    check("FM can reverse carrier direction", negative < (int32_t)base && 
          metal_core_fm_instantaneous_step(base, (uint64_t)base * 1024u, -32768) < 0);
    check("positive instantaneous step is bounded below Nyquist", high == 0x7FFFFFFF);
    check("negative instantaneous step is bounded below negative Nyquist", low == -0x7FFFFFFF);

    check("signed SAW uses bounded BLEP width",
          metal_core_osc_sample_signed(0x34567890u, -((int32_t)base), MC_OSC_SAW) ==
          metal_core_osc_sample(0x34567890u, base, MC_OSC_SAW));
    check("reverse SAW BLEP smooths phase zero", 
          near_i32(metal_core_osc_sample_signed(0u, -((int32_t)base), MC_OSC_SAW), 0, 2));
    check("reverse SAW BLEP smooths phase wrap", 
          near_i32(metal_core_osc_sample_signed(0xFFFFFFFFu, -((int32_t)base), MC_OSC_SAW), 0, 2));
}

static void bypass_exact(void)
{
    voice_t a, b;
    vmod_t m;
    int32_t ref[CTL], fm[CTL];
    uint32_t base = pitch_inc(60 * 16), i;

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    memset(&m, 0, sizeof m);
    m.inc = base;
    m.pitch16 = 60 * 16;
    a.ph[0] = b.ph[0] = 0x12345678u;
    a.ph[1] = b.ph[1] = 0x76543210u;
    a.ph[2] = b.ph[2] = 0xABCDEF01u;

    metal_core_osc_pair_block(&a, ref, CTL, &m, MC_OSC_SAW, 0, 16384);
    metal_core_complex_fm_pair_block(&b, fm, CTL, base, base, 201195u,
                                     MC_OSC_SAW, MC_MOD_SINE, 0, 32767,
                                     MC_FM_CHAR_METALLIC, 16384);
    for (i = 0; i < CTL && ref[i] == fm[i]; i++) {}
    check("FM INDEX zero is sample-exact bypass", i == CTL);
    check("bypass keeps OSC1 phase identical", a.ph[0] == b.ph[0]);
    check("bypass keeps OSC2 phase identical", a.ph[1] == b.ph[1]);
    check("wcMod phase runs independently during bypass", b.ph[2] != a.ph[2]);

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    a.ph[0] = b.ph[0] = 0x12345678u;
    a.ph[1] = b.ph[1] = 0x76543210u;
    metal_core_osc_pair_block(&a, ref, CTL, &m, MC_OSC_SINE, 0, 32767);
    metal_core_complex_fm_pair_block(&b, fm, CTL, base, base, 201195u,
                                     MC_OSC_SINE, MC_MOD_SAW, 26000, 0,
                                     MC_FM_CHAR_METALLIC, 32767);
    for (i = 0; i < CTL && ref[i] == fm[i]; i++) {}
    check("WEST COAST DRIVE zero is sample-exact bypass", i == CTL);
}

static void active_fm(void)
{
    voice_t v;
    int32_t out[CTL + 1], ref[CTL];
    uint32_t base = pitch_inc(60 * 16), mod_inc, i;
    int different = 0, bounded = 1;

    memset(&v, 0, sizeof v);
    memset(out, 0, sizeof out);
    v.ph[0] = 0x13572468u;
    v.ph[1] = 0x24681357u;
    v.ph[2] = 0x31415926u;
    mod_inc = metal_core_ratio_increment(base, 201195u);
    out[CTL] = 0x1234ABCD;
    {
        voice_t baseline;
        memset(&baseline, 0, sizeof baseline);
        baseline.ph[0] = 0x13572468u;
        baseline.ph[1] = 0x24681357u;
        metal_core_osc_pair_block(&baseline, ref, CTL, &(vmod_t){.inc = base, .pitch16 = 60 * 16}, MC_OSC_SAW, 0, 16384);
    }

    metal_core_complex_fm_pair_block(&v, out, CTL, base, base, 201195u,
                                     MC_OSC_SAW, MC_MOD_SINE, 26000, 32767,
                                     MC_FM_CHAR_METALLIC, 16384);
    for (i = 0; i < CTL; i++) {
        if (out[i] < -65535 || out[i] > 65535)
            bounded = 0;
        if (out[i] != ref[i])
            different = 1;
    }
    check("active FM changes the carrier waveform", different);
    check("active FM output remains within pre-fold headroom", bounded);
    check("OSC1 and OSC2 phases advance", v.ph[0] != 0x13572468u && v.ph[1] == 0x24681357u + CTL * base);
    check("wcMod phase advances at MOD RATIO", v.ph[2] == 0x31415926u + CTL * mod_inc);
    check("block does not write past CTL", out[CTL] == 0x1234ABCD);
}

static void extreme_registers(void)
{
    const uint32_t notes[] = {36u * 16u, 60u * 16u, 84u * 16u, 127u * 16u};
    uint32_t k, i;
    int ok = 1;
    for (k = 0; k < sizeof notes / sizeof notes[0]; k++) {
        voice_t v;
        int32_t out[CTL];
        uint32_t inc = pitch_inc(notes[k]);
        memset(&v, 0, sizeof v);
        memset(out, 0, sizeof out);
        metal_core_complex_fm_pair_block(&v, out, CTL, inc, inc, 16u << 16,
                                         MC_OSC_SAW, MC_MOD_SQUARE, 32767, 32767,
                                         MC_FM_CHAR_METALLIC, 32767);
        for (i = 0; i < CTL; i++)
            if (out[i] < -65535 || out[i] > 65535)
                ok = 0;
    }
    check("low/mid/high/extreme register FM remains bounded", ok);
}


static void feedback_zero_exact(void)
{
    voice_t a, b;
    metal_core_feedback_state_t state;
    int32_t ref[CTL], got[CTL];
    uint32_t base = pitch_inc(60 * 16), block, i;
    int same = 1;

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    memset(&state, 0, sizeof state);
    a.ph[0] = b.ph[0] = 0x10293847u;
    a.ph[1] = b.ph[1] = 0x56473829u;
    a.ph[2] = b.ph[2] = 0xABCDEF01u;
    for (block = 0; block < 12; block++) {
        metal_core_complex_fm_pair_block(&a, ref, CTL, base, base, 201195u,
                                         MC_OSC_SAW, MC_MOD_SINE, 24000, 32767,
                                         MC_FM_CHAR_METALLIC, 12000);
        metal_core_complex_fm_feedback_pair_block(
            &b, got, CTL, base, base, 201195u, MC_OSC_SAW, MC_MOD_SINE,
            24000, 32767, MC_FM_CHAR_METALLIC, 12000, 0, &state, 0);
        for (i = 0; i < CTL; i++)
            if (ref[i] != got[i])
                same = 0;
    }
    check("zero FM FEEDBACK is sample-exact against the no-feedback path",
          same && a.ph[0] == b.ph[0] && a.ph[1] == b.ph[1] &&
          a.ph[2] == b.ph[2]);
    check("zero feedback smoother remains exactly zero",
          state.feedback_q15[0] == 0);
}

static void feedback_route_and_delay(void)
{
    voice_t a, b;
    metal_core_feedback_state_t state;
    int32_t ref[CTL], got[CTL];
    uint32_t base = pitch_inc(48 * 16), block, i;
    int pre_delay_same = 1, first_return_same = 1, bounded = 1;

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    memset(&state, 0, sizeof state);
    a.ph[0] = b.ph[0] = 0x13572468u;
    a.ph[1] = b.ph[1] = 0x24681357u;
    a.ph[2] = b.ph[2] = 0x31415926u;

    /* First 128 samples are shorter than the 133-sample 3 ms ring at 44.1 kHz. */
    for (block = 0; block < 4; block++) {
        metal_core_complex_fm_pair_block(&a, ref, CTL, base, base, 201195u,
                                         MC_OSC_SAW, MC_MOD_SINE, 26000, 32767,
                                         MC_FM_CHAR_METALLIC, 16384);
        metal_core_complex_fm_feedback_pair_block(
            &b, got, CTL, base, base, 201195u, MC_OSC_SAW, MC_MOD_SINE,
            26000, 32767, MC_FM_CHAR_METALLIC, 16384, 26214, &state, 0);
        for (i = 0; i < CTL; i++)
            if (ref[i] != got[i])
                pre_delay_same = 0;
    }
    check("feedback route is silent before the full 3 ms delay",
          pre_delay_same && a.ph[2] == b.ph[2]);

    metal_core_complex_fm_pair_block(&a, ref, CTL, base, base, 201195u,
                                     MC_OSC_SAW, MC_MOD_SINE, 26000, 32767,
                                     MC_FM_CHAR_METALLIC, 16384);
    metal_core_complex_fm_feedback_pair_block(
        &b, got, CTL, base, base, 201195u, MC_OSC_SAW, MC_MOD_SINE,
        26000, 32767, MC_FM_CHAR_METALLIC, 16384, 26214, &state, 0);
    for (i = 0; i < (MC_FEEDBACK_DELAY_SAMPLES - 4u * CTL); i++)
        if (ref[i] != got[i])
            first_return_same = 0;
    check("first feedback appears only after the configured delay",
          first_return_same);

    for (block = 0; block < 80; block++) {
        metal_core_complex_fm_pair_block(&a, ref, CTL, base, base, 201195u,
                                         MC_OSC_SAW, MC_MOD_SINE, 26000, 32767,
                                         MC_FM_CHAR_METALLIC, 16384);
        metal_core_complex_fm_feedback_pair_block(
            &b, got, CTL, base, base, 201195u, MC_OSC_SAW, MC_MOD_SINE,
            26000, 32767, MC_FM_CHAR_METALLIC, 16384, 26214, &state, 0);
        for (i = 0; i < CTL; i++)
            if (got[i] < -65535 || got[i] > 65535)
                bounded = 0;
    }
    check("delayed feedback changes wcMod phase, not the delay topology",
          b.ph[2] != a.ph[2]);
    check("feedback path keeps pre-fold output within oscillator headroom",
          bounded);
    check("feedback amount remains bounded to the v98 0..0.8 range",
          state.feedback_q15[0] >= 0 && state.feedback_q15[0] <= 26214);
}

static void feedback_step_bounds(void)
{
    uint32_t base = pitch_inc(60 * 16);
    int32_t positive = metal_core_feedback_modulator_step(base, 32767, 26214);
    int32_t negative = metal_core_feedback_modulator_step(base, -32768, 26214);
    int32_t hi = metal_core_modulator_signed_step(base, INT32_MAX);
    int32_t lo = metal_core_modulator_signed_step(base, INT32_MIN);

    check("positive delayed feedback raises wcMod frequency", positive > 0);
    check("negative delayed feedback lowers wcMod frequency", negative < 0);
    check("wcMod feedback step clamps below positive Nyquist", hi == 0x7FFFFFFF);
    check("wcMod feedback step clamps below negative Nyquist", lo == -0x7FFFFFFF);
}

int main(void)
{
    signed_step();
    bypass_exact();
    active_fm();
    extreme_registers();
    feedback_zero_exact();
    feedback_route_and_delay();
    feedback_step_bounds();
    printf(failures ? "metal-complex-fm: %d FAILED\n" : "metal-complex-fm: all passed\n", failures);
    return failures != 0;
}
