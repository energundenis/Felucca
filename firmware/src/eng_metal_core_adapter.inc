/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — Felucca callback/lifecycle adapter prototype.
 *
 * Included only by engines.c after eng_metal_core.c. The descriptor below is
 * deliberately NOT added to ENGINES[]: this validates the real engine_t API,
 * memory allocator contract, and callback order without registering a sound.
 *
 * Temporary audible-path constants are test scaffolding, not the V1 parameter
 * map. No E0..E7 meanings are assigned here.
 */
ENG_MEM_FITS("METAL CORE", sizeof(metal_core_part_state_t));

static metal_core_part_state_t *metal_core_adapter_state(track_t *t)
{
    uint32_t part;
    if (!t || t < &trk[0] || t >= &trk[NPART])
        return 0;
    part = (uint32_t)(t - trk);
    return (metal_core_part_state_t *)ENG_MEM(part);
}

static void metal_core_adapter_block(track_t *t)
{
    metal_core_part_state_t *s = metal_core_adapter_state(t);
    if (!s)
        return;
    /* ENG_MEM is zeroed at boot and when eng_mem_claim changes owner.
     * ratio_q16 is a nonzero initialized-state sentinel. */
    if (!s->ratio_q16)
        metal_core_part_state_init(s);
    metal_core_part_begin_block(s);
}

static void metal_core_adapter_render(track_t *t, voice_t *v,
                                      int32_t *out, uint32_t n,
                                      const vmod_t *m)
{
    metal_core_part_state_t *s = metal_core_adapter_state(t);
    int32_t voice[CTL] = {0};
    int32_t amp, da;
    uint32_t i;
    (void)v; /* the V1 engine is monophonic; continuous phases live per part */
    if (!s || !out || !m || !n || s->advanced_this_block)
        return;
    if (n > CTL)
        n = CTL;

    /* Fixed probe settings only. The approved EDIT map is not applied here. */
    metal_core_part_render(s, voice, n, m->inc, m->inc, pitch_inc(60 * 16),
        1u << 16, MC_OSC_SAW, MC_MOD_SINE, 8192, 16384,
        MC_FM_CHAR_METALLIC, 8192, 0);

    amp = m->amp0;
    da = (m->amp1 - m->amp0) / (int32_t)n;
    for (i = 0; i < n; i++) {
        out[i] += mulq15(voice[i], amp);
        amp += da;
    }
}

static uint32_t metal_core_adapter_post(track_t *t, int32_t *out, uint32_t n)
{
    metal_core_part_state_t *s = metal_core_adapter_state(t);
    (void)out; /* the free-running oscillator is silent when no voice is active */
    if (s)
        metal_core_part_advance_silent(s, n);
    return 0;  /* no audible effect tail; state advancement alone must not keep mix_part alive */
}

/* Compile-time integration probe only. Not present in ENGINES[] and not selectable. */
static const engine_t ENG_METAL_CORE_ADAPTER __attribute__((unused)) = {
    .name = "METAL CORE ADAPTER TEST",
    .poly = 1,
    .render = metal_core_adapter_render,
    .block = metal_core_adapter_block,
    .mem = sizeof(metal_core_part_state_t),
    .post = metal_core_adapter_post,
};
