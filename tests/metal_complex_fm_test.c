/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — audio-rate Complex FM carrier tests.
 */
#define NVOICE 8
#define NPART 4
#include "../firmware/engines/src/felucca_engine.h"
#include "felucca_tables.h"
#include "../firmware/engines/src/dsp.c"
#include "../firmware/src/eng_metal_core.c"

/* Minimal host track/memory shim for the real engine_t callback adapter. */
typedef struct track track_t;
struct track {
    int16_t p[64];
    voice_t v[NVOICE];
};
static track_t trk[NPART];
static union {
    uint64_t align;
    uint8_t b[12888];
} adapter_mem[NPART];
#define ENG_MEM(p) ((void *)adapter_mem[p].b)
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= 12888u, name " state over adapter memory")
#include "../firmware/src/eng_metal_core_adapter.inc"


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
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    memset(&state, 0, sizeof state);
    a.ph[0] = b.ph[0] = 0x10293847u;
    a.ph[1] = b.ph[1] = 0x56473829u;
    a.ph[2] = b.ph[2] = 0xABCDEF01u;
    for (block = 0; block < 12; block++) {
        metal_core_complex_fm_pair_block(&a, ref, CTL, base, base, 201195u,
            MC_OSC_SAW, MC_MOD_SINE, 24000, 32767, MC_FM_CHAR_METALLIC, 12000);
        metal_core_complex_fm_feedback_pair_block(&b, got, CTL, base, base, base,
            201195u, MC_OSC_SAW, MC_MOD_SINE, 24000, 32767,
            MC_FM_CHAR_METALLIC, 12000, 0, &state, 0);
        for (i = 0; i < CTL; i++) if (ref[i] != got[i]) same = 0;
    }
    check("zero FM FEEDBACK is sample-exact against the no-feedback path",
        same && a.ph[0] == b.ph[0] && a.ph[1] == b.ph[1] && a.ph[2] == b.ph[2]);
    check("zero feedback smoother remains exactly zero", state.feedback_q15[0] == 0);
}

static void feedback_route_and_delay(void)
{
    voice_t a, b;
    metal_core_feedback_state_t state;
    int32_t ref[CTL], got[CTL];
    uint32_t base = pitch_inc(48 * 16), block, i;
    uint32_t full_blocks = MC_FEEDBACK_DELAY_SAMPLES / CTL;
    uint32_t partial = MC_FEEDBACK_DELAY_SAMPLES % CTL;
    int pre_delay_same = 1, boundary_same = 1, bounded = 1;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    memset(&state, 0, sizeof state);
    a.ph[0] = b.ph[0] = 0x13572468u;
    a.ph[1] = b.ph[1] = 0x24681357u;
    a.ph[2] = b.ph[2] = 0x31415926u;

    for (block = 0; block < full_blocks; block++) {
        metal_core_complex_fm_pair_block(&a, ref, CTL, base, base, 201195u,
            MC_OSC_SAW, MC_MOD_SINE, 26000, 32767, MC_FM_CHAR_METALLIC, 16384);
        metal_core_complex_fm_feedback_pair_block(&b, got, CTL, base, base, base,
            201195u, MC_OSC_SAW, MC_MOD_SINE, 26000, 32767,
            MC_FM_CHAR_METALLIC, 16384, 26214, &state, 0);
        for (i = 0; i < CTL; i++) if (ref[i] != got[i]) pre_delay_same = 0;
    }
    check("feedback route is silent before the full 3 ms delay",
        pre_delay_same && a.ph[2] == b.ph[2]);

    metal_core_complex_fm_pair_block(&a, ref, CTL, base, base, 201195u,
        MC_OSC_SAW, MC_MOD_SINE, 26000, 32767, MC_FM_CHAR_METALLIC, 16384);
    metal_core_complex_fm_feedback_pair_block(&b, got, CTL, base, base, base,
        201195u, MC_OSC_SAW, MC_MOD_SINE, 26000, 32767,
        MC_FM_CHAR_METALLIC, 16384, 26214, &state, 0);
    for (i = 0; i < partial; i++) if (ref[i] != got[i]) boundary_same = 0;
    check("first feedback appears only after the configured delay", boundary_same);

    for (block = 0; block < 80; block++) {
        metal_core_complex_fm_pair_block(&a, ref, CTL, base, base, 201195u,
            MC_OSC_SAW, MC_MOD_SINE, 26000, 32767, MC_FM_CHAR_METALLIC, 16384);
        metal_core_complex_fm_feedback_pair_block(&b, got, CTL, base, base, base,
            201195u, MC_OSC_SAW, MC_MOD_SINE, 26000, 32767,
            MC_FM_CHAR_METALLIC, 16384, 26214, &state, 0);
        for (i = 0; i < CTL; i++)
            if (got[i] < -65535 || got[i] > 65535) bounded = 0;
    }
    check("delayed feedback changes wcMod phase", b.ph[2] != a.ph[2]);
    check("feedback output remains within pre-fold oscillator headroom", bounded);
    check("feedback amount remains within v98 0..0.8 range",
        state.feedback_q15[0] >= 0 && state.feedback_q15[0] <= 26214);
}

static void feedback_step_bounds(void)
{
    uint32_t base = pitch_inc(60 * 16);
    int32_t positive = metal_core_feedback_modulator_step(base, 32767, 26214);
    int32_t negative = metal_core_feedback_modulator_step(base, -32768, 26214);
    uint32_t tracked = pitch_inc(72 * 16);
    int32_t panel_scale = metal_core_feedback_modulator_step(base, 16384, 26214);
    int32_t tracked_scale = metal_core_feedback_modulator_step(tracked, 16384, 26214);
    check("positive delayed feedback raises wcMod frequency", positive > 0);
    check("negative delayed feedback lowers wcMod frequency", negative < 0);
    check("feedback scaling uses panel FREQ, independently of FOLLOW pitch",
          panel_scale < tracked_scale);
    check("wcMod feedback step clamps below positive Nyquist",
        metal_core_modulator_signed_step(base, INT32_MAX) == 0x7FFFFFFF);
    {
        int32_t negative_step = metal_core_modulator_signed_step(base, -0x7FFFFFFF);
        check("wcMod negative feedback remains within signed Nyquist bound",
              negative_step < 0 && negative_step >= -0x7FFFFFFF);
    }
}


static void part_state_lifecycle(void)
{
    metal_core_part_state_t active, split;
    int32_t aout[CTL], bout[CTL], discard[CTL];
    uint32_t base = pitch_inc(60 * 16), i;
    int same, duplicate_guard;
    memset(aout, 0, sizeof aout);
    memset(bout, 0, sizeof bout);
    memset(discard, 0, sizeof discard);
    memset(&active, 0, sizeof active);
    memset(&split, 0, sizeof split);
    metal_core_part_state_init(&active);
    metal_core_part_state_init(&split);

    metal_core_part_begin_block(&active);
    metal_core_part_begin_block(&split);
    metal_core_part_render(&active, aout, CTL, base, base, base, 201195u,
        MC_OSC_SAW, MC_MOD_SINE, 26000, 32767, MC_FM_CHAR_METALLIC,
        16384, 26214);
    metal_core_part_render(&split, bout, CTL, base, base, base, 201195u,
        MC_OSC_SAW, MC_MOD_SINE, 26000, 32767, MC_FM_CHAR_METALLIC,
        16384, 26214);
    same = memcmp(&active, &split, sizeof active) == 0 &&
           memcmp(aout, bout, sizeof aout) == 0;
    check("per-part state starts with identical active rendering", same);

    metal_core_part_begin_block(&active);
    metal_core_part_begin_block(&split);
    metal_core_part_render(&active, discard, CTL, base, base, base, 201195u,
        MC_OSC_SAW, MC_MOD_SINE, 26000, 32767, MC_FM_CHAR_METALLIC,
        16384, 26214);
    metal_core_part_advance_silent(&split, CTL);
    same = memcmp(&active, &split, sizeof active) == 0;
    check("silent gap advances exactly the same shared oscillator/feedback state", same);

    {
        metal_core_part_state_t snapshot = split;
        metal_core_part_advance_silent(&split, CTL);
        duplicate_guard = memcmp(&snapshot, &split, sizeof split) == 0;
    }
    check("duplicate post/block advancement cannot double-advance one block",
          duplicate_guard);

    memset(aout, 0, sizeof aout);
    memset(bout, 0, sizeof bout);
    metal_core_part_begin_block(&active);
    metal_core_part_begin_block(&split);
    metal_core_part_render(&active, aout, CTL, base, base, base, 201195u,
        MC_OSC_SAW, MC_MOD_SINE, 26000, 32767, MC_FM_CHAR_METALLIC,
        16384, 26214);
    metal_core_part_render(&split, bout, CTL, base, base, base, 201195u,
        MC_OSC_SAW, MC_MOD_SINE, 26000, 32767, MC_FM_CHAR_METALLIC,
        16384, 26214);
    check("reactivation after silent gap resumes phase and feedback history exactly",
          memcmp(&active, &split, sizeof active) == 0 &&
          memcmp(aout, bout, sizeof aout) == 0);

    /* A note retrigger changes pitch/control values but never clears phases or delay. */
    {
        uint32_t phase_before[3], write_before = split.delay_write;
        memcpy(phase_before, split.phase, sizeof phase_before);
        metal_core_part_begin_block(&split);
        metal_core_part_render(&split, bout, CTL, pitch_inc(67 * 16),
            pitch_inc(67 * 16), base, 201195u, MC_OSC_SAW, MC_MOD_SINE,
            26000, 32767, MC_FM_CHAR_METALLIC, 16384, 26214);
        check("legato/retrigger pitch update preserves phase and delay position",
              split.phase[0] != phase_before[0] &&
              split.phase[1] != phase_before[1] &&
              split.phase[2] != phase_before[2] &&
              split.delay_write == (write_before + CTL) % MC_FEEDBACK_DELAY_SAMPLES);
    }

    check("monophonic per-part DSP state fits engine memory with margin",
          sizeof(metal_core_part_state_t) < 2000u &&
          sizeof(metal_core_part_state_t) <= 12888u);
    check("per-part state remains independent of NVOICE delay banks",
          sizeof(metal_core_part_state_t) <
          sizeof(metal_core_feedback_state_t) / 2u);
    (void)i;
}


static void adapter_callbacks(void)
{
    metal_core_part_state_t *s;
    vmod_t m;
    int32_t out[CTL];
    uint32_t base = pitch_inc(60 * 16), phase_before;
    uint16_t write_before;

    memset(trk, 0, sizeof trk);
    memset(adapter_mem, 0, sizeof adapter_mem);
    memset(out, 0, sizeof out);
    memset(&m, 0, sizeof m);
    m.inc = base;
    m.pitch16 = 60 * 16;
    m.amp0 = m.amp1 = 32767;
    s = (metal_core_part_state_t *)ENG_MEM(0);

    check("unregistered adapter exposes the actual engine_t callbacks",
          ENG_METAL_CORE_ADAPTER.block == metal_core_adapter_block &&
          ENG_METAL_CORE_ADAPTER.render == metal_core_adapter_render &&
          ENG_METAL_CORE_ADAPTER.post == metal_core_adapter_post);
    check("adapter declares one-voice monophonic cap and exact part memory",
          ENG_METAL_CORE_ADAPTER.poly == 1 &&
          ENG_METAL_CORE_ADAPTER.mem == sizeof(metal_core_part_state_t));

    ENG_METAL_CORE_ADAPTER.block(&trk[0]);
    check("block hook initializes zeroed host memory and resets guard",
          s->ratio_q16 == (1u << 16) && !s->advanced_this_block);

    ENG_METAL_CORE_ADAPTER.render(&trk[0], &trk[0].v[0], out, CTL, &m);
    check("render hook advances per-part phases and delay once",
          s->advanced_this_block && s->delay_write == CTL &&
          s->phase[0] != 0 && s->phase[1] != 0 && s->phase[2] != 0);

    phase_before = s->phase[2];
    write_before = s->delay_write;
    ENG_METAL_CORE_ADAPTER.post(&trk[0], out, CTL);
    check("post hook does not double-advance after active rendering",
          s->phase[2] == phase_before && s->delay_write == write_before);

    ENG_METAL_CORE_ADAPTER.block(&trk[0]);
    phase_before = s->phase[2];
    write_before = s->delay_write;
    ENG_METAL_CORE_ADAPTER.post(&trk[0], out, CTL);
    check("post hook advances free-running state when no voice rendered",
          s->advanced_this_block && s->phase[2] != phase_before &&
          s->delay_write == (write_before + CTL) % MC_FEEDBACK_DELAY_SAMPLES);

    ENG_METAL_CORE_ADAPTER.block(&trk[0]);
    phase_before = s->phase[2];
    ENG_METAL_CORE_ADAPTER.render(&trk[0], &trk[0].v[0], out, CTL, &m);
    check("render resumes the same part state after an idle block",
          s->phase[2] != phase_before &&
          s->delay_write == (write_before + 2u * CTL) % MC_FEEDBACK_DELAY_SAMPLES);
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
    part_state_lifecycle();
    adapter_callbacks();
    printf(failures ? "metal-complex-fm: %d FAILED\n" : "metal-complex-fm: all passed\n", failures);
    return failures != 0;
}
