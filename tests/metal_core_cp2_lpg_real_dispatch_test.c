/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — CP2 LPG + VCA lifecycle through real Felucca dispatch.
 *
 * Exercises the explicit LPG Gate Off helper from a private engine render
 * callback reached by the real voice.c track_render() path. LPG gain and VCA
 * envelopes have separate per-voice state and independent completion rules.
 */
#define FELUCCA_CP3_TEST_MUTABLE_ENGINE_REGISTRY 1
#define main cp2_lpg_dispatch_hostsim_main
#include "hostsim.c"
#undef main

#include "../firmware/src/eng_metal_core_lpg.h"
#include "../firmware/src/eng_metal_core_vca_render.h"

#include <stdio.h>
#include <string.h>

#define CP2_TEST_SAMPLE_RATE 44100u
#define CP2_TEST_LPG_PEAK_Q15 20000
#define CP2_TEST_VCA_ATTACK_US 5000u
#define CP2_TEST_VCA_RELEASE_US 12000u

typedef struct {
    eng_metal_vca_t vca;
    eng_metal_lpg_env_t lpg;
} cp2_lpg_vca_voice_state_t;

typedef struct {
    cp2_lpg_vca_voice_state_t voice[NVOICE];
} cp2_lpg_vca_part_state_t;

_Static_assert(sizeof(cp2_lpg_vca_part_state_t) <= ENG_MEM_PART,
               "CP2 LPG+VCA state must fit Felucca engine part memory");

static const eng_metal_lpg_timing_t CP2_TEST_LPG_TIMING = {
    66u, 200u, 100u, 50u, 80u, 8000u, 2000u
};

static int failures;

static void check(const char *name, int ok)
{
    printf("CP2 LPG real dispatch: %-68s %s\n",
           name, ok ? "PASS" : "FAIL");
    failures += !ok;
}

static uint32_t part_index(const track_t *t)
{
    uintptr_t base = (uintptr_t)&trk[0], ptr = (uintptr_t)t;
    uintptr_t offset;
    if (!t || ptr < base || ptr >= base + sizeof(trk))
        return NPART;
    offset = ptr - base;
    if (offset % sizeof(track_t))
        return NPART;
    return (uint32_t)(offset / sizeof(track_t));
}

static uint32_t voice_index(const track_t *t, const voice_t *v)
{
    uintptr_t base, ptr, offset;
    if (!t || !v)
        return NVOICE;
    base = (uintptr_t)&t->v[0];
    ptr = (uintptr_t)v;
    if (ptr < base || ptr >= base + sizeof(t->v))
        return NVOICE;
    offset = ptr - base;
    if (offset % sizeof(voice_t))
        return NVOICE;
    return (uint32_t)(offset / sizeof(voice_t));
}

static cp2_lpg_vca_voice_state_t *voice_state(track_t *t, voice_t *v)
{
    uint32_t p = part_index(t), i = voice_index(t, v);
    cp2_lpg_vca_part_state_t *s;
    if (p >= NPART || i >= NVOICE)
        return 0;
    s = (cp2_lpg_vca_part_state_t *)ENG_MEM(p);
    return &s->voice[i];
}

static void cp2_note_on(struct track *track, voice_t *voice)
{
    cp2_lpg_vca_voice_state_t *s =
        voice_state((track_t *)track, voice);
    int32_t peak, sustain;
    uint32_t attack, release;
    if (!s)
        return;
    if (voice->env == 0) {
        eng_metal_vca_reset(&s->vca);
        eng_metal_lpg_env_reset(&s->lpg);
    }
    attack = eng_metal_vca_us_to_samples(CP2_TEST_SAMPLE_RATE,
                                         CP2_TEST_VCA_ATTACK_US);
    release = eng_metal_vca_us_to_samples(CP2_TEST_SAMPLE_RATE,
                                          CP2_TEST_VCA_RELEASE_US);
    eng_metal_vca_targets_q16(ENG_METAL_VCA_Q16_ONE, 32768,
                              ENG_METAL_VCA_Q16_ONE, 1, 0,
                              &peak, &sustain);
    eng_metal_vca_trigger(&s->vca, peak, sustain,
                          attack, attack, attack, UINT32_MAX, release);
    eng_metal_lpg_env_trigger(&s->lpg, CP2_TEST_LPG_PEAK_Q15,
                              &CP2_TEST_LPG_TIMING);
}

static void cp2_render(struct track *track, voice_t *voice,
                       int32_t *out, uint32_t n, const vmod_t *m)
{
    cp2_lpg_vca_voice_state_t *s =
        voice_state((track_t *)track, voice);
    uint32_t i;
    int32_t amp, da;
    if (!s || !out || !m)
        return;
    if (!voice->gate) {
        eng_metal_vca_gate_off(&s->vca);
        eng_metal_lpg_env_gate_off(&s->lpg);
    }
    amp = m->amp0;
    da = (m->amp1 - m->amp0) >> 5;
    for (i = 0; i < n; ++i) {
        int32_t vca_sample, lpg_gain;
        amp += da;
        vca_sample = eng_metal_vca_process(&s->vca);
        lpg_gain = eng_metal_lpg_env_process(&s->lpg);
        vca_sample = eng_metal_vca_apply_host_amp(
            8000, vca_sample, amp);
        out[i] += (int32_t)(((int64_t)vca_sample * lpg_gain) >> 15);
    }
}

static int cp2_done(struct track *track, voice_t *voice)
{
    cp2_lpg_vca_voice_state_t *s =
        voice_state((track_t *)track, voice);
    return !s ||
           (eng_metal_vca_done(&s->vca) &&
            s->lpg.phase == ENG_METAL_LPG_ENV_IDLE);
}

static const engine_t CP2_LPG_VCA_TEST_ENGINE = {
    .name = "CP2 LPG+VCA TEST",
    .poly = NVOICE,
    .ownenv = 1u,
    .note_on = cp2_note_on,
    .render = cp2_render,
    .done = cp2_done,
    .mem = (uint16_t)sizeof(cp2_lpg_vca_part_state_t)
};

static void setup_host(void)
{
    uint32_t p, i;
    memset(trk, 0, sizeof(trk));
    memset(&song, 0, sizeof(song));
    memset(eng_mem, 0, sizeof(eng_mem));
    memset(eng_mem_owner, 0, sizeof(eng_mem_owner));
    host_tracks_init();
    ENGINES[0] = &CP2_LPG_VCA_TEST_ENGINE;
    for (p = 0; p < NPART; ++p) {
        trk[p].engine = trk[p].eng_req = 0u;
        trk[p].p[P_VOICE] = V_MONO;
        trk[p].p[P_MUTE] = 0;
        for (i = 0; i < 8u; ++i)
            trk[p].p[P_E0 + i] = 0;
        eng_mem_claim(&trk[p]);
    }
}

static void test_gate_off_keeps_lpg_and_vca_independent(void)
{
    track_t *t;
    voice_t *v;
    cp2_lpg_vca_voice_state_t *s;
    int32_t out[CTL];
    uint32_t i;

    setup_host();
    t = &trk[0];
    trk_note_on_source(t, 60u, 100u, NOTE_SRC_PERFORMANCE);
    v = &t->v[0];
    s = voice_state(t, v);
    for (i = 0; i < 8u; ++i) {
        memset(out, 0, sizeof(out));
        (void)track_render(t, out, CTL);
    }
    check("real note dispatch starts both separate envelopes",
          s && s->vca.phase == ENG_METAL_VCA_ATTACK &&
          s->lpg.phase == ENG_METAL_LPG_ENV_ATTACK &&
          s->vca.level_q16 > 0 && s->lpg.level_q15 > 0);
    check("engine memory owns distinct LPG and VCA state",
          sizeof(cp2_lpg_vca_voice_state_t) ==
              sizeof(eng_metal_vca_t) + sizeof(eng_metal_lpg_env_t) &&
          ENGINES[t->engine]->mem == sizeof(cp2_lpg_vca_part_state_t));

    trk_note_off(t, 60u);
    memset(out, 0, sizeof(out));
    (void)track_render(t, out, CTL);
    check("real Note Off reaches render callback and starts both releases",
          s->vca.phase == ENG_METAL_VCA_RELEASE &&
          s->lpg.phase == ENG_METAL_LPG_ENV_DECAY);
    check("LPG Gate Off preserves its instantaneous level",
          s->lpg.level_q15 > 0 &&
          s->lpg.target_q15 == ENG_METAL_LPG_ENV_FLOOR);

    /* Calling the render callback again while Gate remains low must not
     * restart either trajectory. */
    {
        uint32_t vca_remaining = s->vca.gate_remaining;
        int32_t lpg_target = s->lpg.target_q15;
        memset(out, 0, sizeof(out));
        (void)track_render(t, out, CTL);
        check("repeated real render does not restart Gate Off trajectories",
              s->vca.phase == ENG_METAL_VCA_RELEASE &&
              s->vca.gate_remaining == vca_remaining &&
              s->lpg.phase == ENG_METAL_LPG_ENV_DECAY &&
              s->lpg.target_q15 == lpg_target);
    }
}

static void test_voice_retires_only_when_both_envelopes_are_idle(void)
{
    track_t *t;
    voice_t *v;
    cp2_lpg_vca_voice_state_t *s;
    int32_t out[CTL];
    uint32_t blocks = 0;

    setup_host();
    t = &trk[0];
    trk_note_on_source(t, 57u, 96u, NOTE_SRC_PERFORMANCE);
    v = &t->v[0];
    s = voice_state(t, v);
    for (blocks = 0; blocks < 8u; ++blocks) {
        memset(out, 0, sizeof(out));
        (void)track_render(t, out, CTL);
    }
    trk_note_off(t, 57u);
    while (blocks++ < 1200u && v->active) {
        memset(out, 0, sizeof(out));
        (void)track_render(t, out, CTL);
    }
    check("real voice dispatcher waits for both LPG and VCA to finish",
          !v->active &&
          s->vca.phase == ENG_METAL_VCA_IDLE &&
          s->lpg.phase == ENG_METAL_LPG_ENV_IDLE &&
          blocks < 1200u);
}

int main(void)
{
    setup_host();
    test_gate_off_keeps_lpg_and_vca_independent();
    test_voice_retires_only_when_both_envelopes_are_idle();
    printf("CP2 LPG+VCA real-dispatch integration: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
