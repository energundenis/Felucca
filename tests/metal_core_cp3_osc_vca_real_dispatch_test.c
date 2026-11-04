/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — CP3 integrated OSC1 + VCA real-dispatch harness.
 *
 * Combines the existing production OSC phase adapter and VCA envelope in one
 * private engine descriptor, then exercises Felucca's real voice.c dispatcher
 * and track_render() via hostsim.c. No production engine registration.
 */
#define FELUCCA_CP3_TEST_MUTABLE_ENGINE_REGISTRY 1
#define main cp3_osc_vca_hostsim_main
#include "hostsim.c"
#undef main

#include "../firmware/src/eng_metal_core_osc_voice.h"
#include "../firmware/src/eng_metal_core_vca_render.h"

#include <stdio.h>
#include <string.h>

#define CP3_OSC_VCA_SAMPLE_RATE 44100u
#define CP3_OSC_VCA_ATTACK_US   5000u
#define CP3_OSC_VCA_RELEASE_US  12000u

static int failures;

static void check(const char *name, int ok)
{
    printf("CP3 OSC+VCA real dispatch: %-66s %s\n",
           name, ok ? "PASS" : "FAIL");
    failures += !ok;
}

static eng_metal_vca_part_state_t *part_state(track_t *t)
{
    uintptr_t base = (uintptr_t)&trk[0];
    uintptr_t ptr = (uintptr_t)t;
    uintptr_t offset;
    uint32_t p;
    if (!t || ptr < base || ptr >= base + sizeof(trk))
        return 0;
    offset = ptr - base;
    if (offset % sizeof(track_t))
        return 0;
    p = (uint32_t)(offset / sizeof(track_t));
    return (eng_metal_vca_part_state_t *)ENG_MEM(p);
}

static uint32_t voice_index(track_t *t, voice_t *v)
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

static eng_metal_vca_t *voice_state(track_t *t, voice_t *v)
{
    eng_metal_vca_part_state_t *s = part_state(t);
    uint32_t i = voice_index(t, v);
    return s && i < NVOICE ? &s->voice[i] : 0;
}

static void engine_note_on(struct track *track, voice_t *voice)
{
    track_t *t = (track_t *)track;
    eng_metal_vca_t *vca = voice_state(t, voice);
    int32_t peak, sustain;
    uint32_t attack = eng_metal_vca_us_to_samples(
        CP3_OSC_VCA_SAMPLE_RATE, CP3_OSC_VCA_ATTACK_US);
    uint32_t release = eng_metal_vca_us_to_samples(
        CP3_OSC_VCA_SAMPLE_RATE, CP3_OSC_VCA_RELEASE_US);

    eng_metal_osc_voice_note_on(voice);
    if (!vca)
        return;
    /* A fresh/reclaimed host slot starts from zero. A sounding retrigger
     * keeps the instantaneous VCA level (cancel-and-hold). */
    if (voice->env == 0)
        eng_metal_vca_reset(vca);
    eng_metal_vca_targets_q16(ENG_METAL_VCA_Q16_ONE, 32768,
                              ENG_METAL_VCA_Q16_ONE, 1, 0,
                              &peak, &sustain);
    eng_metal_vca_trigger(vca, peak, sustain,
                          attack ? attack : 1u,
                          attack ? attack : 1u,
                          attack ? attack : 1u,
                          UINT32_MAX, release ? release : 1u);
}

static void engine_render(struct track *track, voice_t *voice,
                          int32_t *out, uint32_t n, const vmod_t *m)
{
    track_t *t = (track_t *)track;
    eng_metal_vca_t *vca = voice_state(t, voice);
    uint32_t i;
    int32_t amp, da;
    if (!vca || !voice || !out || !m)
        return;
    if (!voice->gate)
        eng_metal_vca_gate_off(vca);

    amp = m->amp0;
    da = (m->amp1 - m->amp0) >> 5;
    for (i = 0; i < n; ++i) {
        int32_t osc1, osc2, source, level;
        amp += da;
        osc1 = eng_metal_osc_voice1_next(
            voice, m, ENG_METAL_OSC_SAW, 0x80000000u);
        osc2 = eng_metal_osc_voice2_next(
            voice, m, ENG_METAL_OSC_TRI, 0x80000000u);
        source = (osc1 + osc2) / 2;
        level = eng_metal_vca_process(vca);
        out[i] += eng_metal_vca_apply_host_amp(source, level, amp);
    }
}

static int engine_done(struct track *track, voice_t *voice)
{
    return eng_metal_vca_done(voice_state((track_t *)track, voice));
}

_Static_assert(sizeof(eng_metal_vca_part_state_t) <= ENG_MEM_PART,
               "combined OSC+VCA state must fit Felucca engine part memory");

static const engine_t ENG_METAL_CORE_OSC_VCA_CP3_TEST = {
    .name = "MC OSC+VCA CP3 TEST",
    .poly = NVOICE,
    .ownenv = 1u,
    .note_on = engine_note_on,
    .render = engine_render,
    .done = engine_done,
    .mem = (uint16_t)sizeof(eng_metal_vca_part_state_t)
};

static void setup_host(void)
{
    uint32_t p, i;
    memset(trk, 0, sizeof(trk));
    memset(&song, 0, sizeof(song));
    memset(eng_mem, 0, sizeof(eng_mem));
    memset(eng_mem_owner, 0, sizeof(eng_mem_owner));
    host_tracks_init();
    ENGINES[0] = &ENG_METAL_CORE_OSC_VCA_CP3_TEST;
    for (p = 0; p < NPART; ++p) {
        trk[p].engine = trk[p].eng_req = 0u;
        trk[p].p[P_VOICE] = V_MONO;
        trk[p].p[P_MUTE] = 0;
        for (i = 0; i < 8u; ++i)
            trk[p].p[P_E0 + i] = 0;
        eng_mem_claim(&trk[p]);
    }
}

static void test_combined_real_render_and_additive_output(void)
{
    track_t *t;
    voice_t *v;
    eng_metal_vca_t *vca;
    int32_t out[CTL];
    uint32_t i, nonzero = 0, phase_after1, phase_after2;
    const int32_t sentinel = 1234;

    setup_host();
    t = &trk[0];
    v = &t->v[0];
    trk_note_on_source(t, 60u, 100u, NOTE_SRC_PERFORMANCE);
    vca = voice_state(t, v);
    check("real dispatcher invokes combined descriptor and allocates voice",
          v->active && v->gate && vca &&
          vca->phase == ENG_METAL_VCA_ATTACK);
    check("descriptor declares exactly the VCA part-state footprint",
          ENGINES[t->engine]->mem == sizeof(eng_metal_vca_part_state_t) &&
          ENGINES[t->engine]->ownenv == 1u);

    for (i = 0; i < CTL; ++i)
        out[i] = sentinel;
    (void)track_render(t, out, CTL);
    phase_after1 = v->ph[ENG_METAL_OSC1_PHASE_SLOT];
    phase_after2 = v->ph[ENG_METAL_OSC2_PHASE_SLOT];
    for (i = 0; i < CTL; ++i)
        nonzero += out[i] != sentinel;
    check("real track_render advances independent OSC1/OSC2 phases and runs VCA together",
          phase_after1 != 0u && phase_after2 != 0u &&
          phase_after1 == phase_after2 && vca->level_q16 > 0 &&
          vca->phase == ENG_METAL_VCA_ATTACK);
    check("combined renderer adds signal to existing mix instead of overwriting",
          nonzero > 0u);

    /* Sounding retrigger must preserve oscillator phase and VCA level. */
    {
        uint32_t phase = v->ph[ENG_METAL_OSC1_PHASE_SLOT];
        int32_t level = vca->level_q16;
        trk_note_on_source(t, 64u, 100u, NOTE_SRC_PERFORMANCE);
        check("real sounding retrigger preserves oscillator phase",
              v->ph[ENG_METAL_OSC1_PHASE_SLOT] == phase);
        check("VCA retrigger starts from instantaneous held level",
              vca->level_q16 == level && vca->phase == ENG_METAL_VCA_ATTACK);
    }
}

static void test_real_gate_off_and_voice_retirement(void)
{
    track_t *t;
    voice_t *v;
    eng_metal_vca_t *vca;
    int32_t out[CTL];
    uint32_t blocks = 0;

    setup_host();
    t = &trk[0];
    trk_note_on_source(t, 57u, 96u, NOTE_SRC_PERFORMANCE);
    v = &t->v[0];
    vca = voice_state(t, v);
    while (blocks++ < 8u && vca->level_q16 == 0) {
        memset(out, 0, sizeof(out));
        (void)track_render(t, out, CTL);
    }
    trk_note_off(t, 57u);
    memset(out, 0, sizeof(out));
    (void)track_render(t, out, CTL);
    check("real Note Off reaches combined render callback as VCA Gate Off",
          vca->phase == ENG_METAL_VCA_RELEASE);
    while (blocks++ < 1200u && v->active) {
        memset(out, 0, sizeof(out));
        (void)track_render(t, out, CTL);
    }
    check("real track_render and engine done() retire voice after VCA idle",
          !v->active && vca->phase == ENG_METAL_VCA_IDLE && blocks < 1200u);
}

int main(void)
{
    test_combined_real_render_and_additive_output();
    test_real_gate_off_and_voice_retirement();
    printf("CP3 OSC+VCA integrated real-dispatch harness: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
