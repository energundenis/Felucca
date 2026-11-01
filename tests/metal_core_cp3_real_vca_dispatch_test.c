/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — CP3 real host dispatcher + engine-owned VCA callback.
 *
 * The host-only registry seam lets this isolated process substitute a private
 * descriptor at engine slot 0. Production firmware builds keep ENGINES immutable.
 * Felucca's real voice.c/seq.c/track_render are compiled via hostsim.c.
 */
#define FELUCCA_CP3_TEST_MUTABLE_ENGINE_REGISTRY 1
#define main cp3_hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/eng_metal_core_vca_render.h"

#include <stdio.h>
#include <string.h>

#define CP3_TEST_ACCENT NOTE_META_ACCENT
#define CP3_TEST_SAMPLE 12000

typedef union {
    uint64_t align;
    uint8_t bytes[13344u];
} cp3_test_mem_t;

static cp3_test_mem_t cp3_test_mem[NPART];
static int cp3_vca_failures;

static void cp3_vca_check(const char *name, int ok)
{
    printf("CP3 real VCA dispatch: %-66s %s\n", name, ok ? "PASS" : "FAIL");
    cp3_vca_failures += !ok;
}

static uint32_t cp3_part_index(const track_t *t)
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

static uint32_t cp3_voice_index(const track_t *t, const voice_t *v)
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

static eng_metal_vca_part_state_t *cp3_part_state(track_t *t)
{
    uint32_t p = cp3_part_index(t);
    if (p >= NPART)
        return 0;
    return (eng_metal_vca_part_state_t *)(void *)cp3_test_mem[p].bytes;
}

static eng_metal_vca_t *cp3_voice_vca(track_t *t, voice_t *v)
{
    uint32_t i = cp3_voice_index(t, v);
    eng_metal_vca_part_state_t *s = cp3_part_state(t);
    return i < NVOICE && s ? &s->voice[i] : 0;
}

static void cp3_engine_note_on(struct track *track, voice_t *voice)
{
    track_t *t = (track_t *)track;
    uint32_t i = cp3_voice_index(t, voice);
    eng_metal_vca_t *vca = cp3_voice_vca(t, voice);
    int32_t peak, sustain, velocity;
    if (!vca || i >= NVOICE)
        return;
    /* voice_start sets host env to zero for a genuinely free/reclaimed slot;
     * a live retrigger keeps its instantaneous host level. Reset stale engine
     * state after Felucca has completed its one-block stage-4 steal fade. */
    if (voice->env == 0)
        eng_metal_vca_reset(vca);
    velocity = ((int32_t)voice->vel * ENG_METAL_VCA_Q16_ONE + 63) / 127;
    eng_metal_vca_targets_q16(ENG_METAL_VCA_Q16_ONE, 32768,
                              velocity, 1,
                              (t->voice_source[i] & CP3_TEST_ACCENT) != 0,
                              &peak, &sustain);
    eng_metal_vca_trigger(vca, peak, sustain,
                          221u, 441u, 13230u,
                          UINT32_MAX, 882u);
}

static void cp3_engine_render(struct track *track, voice_t *voice,
                              int32_t *out, uint32_t n, const vmod_t *m)
{
    track_t *t = (track_t *)track;
    eng_metal_vca_t *vca = cp3_voice_vca(t, voice);
    if (!vca || !out || !m)
        return;
    if (!voice->gate)
        eng_metal_vca_gate_off(vca);
    eng_metal_vca_render_constant(vca, out, n, CP3_TEST_SAMPLE,
                                  m->amp0, m->amp1);
}

static int cp3_engine_done(struct track *track, voice_t *voice)
{
    eng_metal_vca_t *vca = cp3_voice_vca((track_t *)track, voice);
    return !vca || eng_metal_vca_done(vca);
}

static const engine_t ENG_METAL_CORE_CP3_TEST = {
    .name = "MC CP3 VCA TEST",
    .poly = NVOICE,
    .ownenv = 1u,
    .note_on = cp3_engine_note_on,
    .render = cp3_engine_render,
    .done = cp3_engine_done,
    .mem = (uint16_t)sizeof(eng_metal_vca_part_state_t)
};

_Static_assert(sizeof(eng_metal_vca_part_state_t) == 288u,
               "CP3 test expects eight 36-byte VCA states");
_Static_assert(sizeof(eng_metal_vca_part_state_t) <= 12888u,
               "CP3 test VCA state must fit Felucca non-ADPCM part pool");

static void cp3_vca_init(void)
{
    uint32_t p, i;
    memset(trk, 0, sizeof(trk));
    memset(&song, 0, sizeof(song));
    memset(cp3_test_mem, 0, sizeof(cp3_test_mem));
    host_tracks_init();
    ENGINES[0] = &ENG_METAL_CORE_CP3_TEST;
    for (p = 0; p < NPART; ++p) {
        trk[p].engine = trk[p].eng_req = 0u;
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_MUTE] = 0;
        for (i = 0; i < 8u; ++i)
            trk[p].p[P_E0 + i] = 0;
        eng_mem_claim(&trk[p]);
    }
}

static void cp3_render_blocks(track_t *t, uint32_t blocks)
{
    int32_t out[CTL];
    uint32_t i;
    for (i = 0; i < blocks; ++i)
        (void)track_render(t, out, CTL);
}

static void test_real_dispatch_uses_engine_owned_vca(void)
{
    track_t *t = &trk[0];
    eng_metal_vca_part_state_t *s = cp3_part_state(t);
    uint32_t i, a = NVOICE, b = NVOICE;
    cp3_vca_init();
    t = &trk[0];
    s = cp3_part_state(t);

    cp3_vca_check("test descriptor is selected through the host engine registry",
              ENGINES[t->engine] == &ENG_METAL_CORE_CP3_TEST &&
              ENGINES[t->engine]->ownenv == 1u &&
              ENGINES[t->engine]->mem == 288u);

    trk_note_on_source(t, 60u, 100u,
                       NOTE_SRC_PERFORMANCE | CP3_TEST_ACCENT);
    trk_note_on_source(t, 64u, 100u, NOTE_SRC_PERFORMANCE);
    for (i = 0; i < NVOICE; ++i) {
        if (t->v[i].active && t->v[i].note == 60u) a = i;
        if (t->v[i].active && t->v[i].note == 64u) b = i;
    }
    cp3_vca_check("real trk_note_on_source reaches the METAL VCA note_on callback",
              a < NVOICE && b < NVOICE &&
              s->voice[a].phase == ENG_METAL_VCA_ATTACK &&
              s->voice[b].phase == ENG_METAL_VCA_ATTACK);
    cp3_vca_check("voice provenance independently controls explicit Accent",
              a < NVOICE && b < NVOICE &&
              t->voice_source[a] == (NOTE_SRC_PERFORMANCE | CP3_TEST_ACCENT) &&
              t->voice_source[b] == NOTE_SRC_PERFORMANCE &&
              s->voice[a].target_q16 > s->voice[b].target_q16);

    cp3_render_blocks(t, 4u);
    cp3_vca_check("real track_render bypasses host ADSR for ownenv engine",
              t->v[a].env == (1 << 24) && t->v[a].stage != 0u &&
              s->voice[a].phase == ENG_METAL_VCA_ATTACK);
}

static void test_gate_release_and_done_lifecycle(void)
{
    track_t *t = &trk[0];
    eng_metal_vca_part_state_t *s;
    uint32_t i, voice = NVOICE, blocks = 0;
    cp3_vca_init();
    t = &trk[0];
    s = cp3_part_state(t);
    trk_note_on_source(t, 57u, 96u, NOTE_SRC_PERFORMANCE);
    cp3_render_blocks(t, 5u);
    for (i = 0; i < NVOICE; ++i)
        if (t->v[i].active && t->v[i].note == 57u)
            voice = i;
    cp3_vca_check("VCA reaches sounding state via real track_render",
                  voice < NVOICE && s->voice[voice].level_q16 > 0);
    if (voice >= NVOICE)
        return;

    trk_note_off(t, 57u);
    cp3_render_blocks(t, 1u);
    cp3_vca_check("real Note Off is observed by engine callback at block render",
                  s->voice[voice].phase == ENG_METAL_VCA_RELEASE &&
                  !ENGINES[t->engine]->done(t, &t->v[voice]));

    while (blocks++ < 1000u && t->v[voice].active)
        cp3_render_blocks(t, 1u);
    cp3_vca_check("real track_render + engine done() retire after VCA idle",
                  !t->v[voice].active &&
                  s->voice[voice].phase == ENG_METAL_VCA_IDLE &&
                  blocks < 1000u);
}

static void test_engine_switch_replays_queued_vca_note(void)
{
    track_t *t;
    int32_t out[CTL];
    uint32_t i, voice = NVOICE;

    cp3_vca_init();
    t = &trk[0];
    /* Private host-test slot 1 models the destination engine descriptor. */
    ENGINES[1] = &ENG_METAL_CORE_CP3_TEST;
    trk_note_on_source(t, 45u, 100u, NOTE_SRC_PERFORMANCE);
    cp3_render_blocks(t, 3u);

    t->eng_req = 1u;
    engine_block(t); /* begin the real four-block old-engine fade */
    cp3_vca_check("real engine_block starts the engine-switch fade",
                  t->xf_on && t->xf == XF_BLOCKS && t->engine == 0u);

    trk_note_on_source(t, 72u, 93u,
                       NOTE_SRC_SEQUENCER | NOTE_META_ACCENT);
    cp3_vca_check("note during fade is queued, not sent to old engine",
                  t->xp_n == 1u && cp3_active_in_part(t) == 1u);

    for (i = 0; i < XF_BLOCKS; ++i) {
        memset(out, 0, sizeof(out));
        (void)track_render(t, out, CTL);
    }
    cp3_vca_check("old engine fade completes on the real render path",
                  t->xf_on && t->xf == 0u && t->engine == 0u);

    engine_block(t); /* switch, claim new engine memory, replay queued note */
    for (i = 0; i < NVOICE; ++i)
        if (t->v[i].active && t->v[i].note == 72u)
            voice = i;
    cp3_vca_check("real engine_block replays queued note on destination engine",
                  t->engine == 1u && !t->xf_on && t->xp_n == 0u &&
                  voice < NVOICE && t->v[voice].gate);
    cp3_vca_check("queued source and explicit Accent survive the real engine switch",
                  voice < NVOICE &&
                  t->voice_source[voice] ==
                    (NOTE_SRC_SEQUENCER | NOTE_META_ACCENT) &&
                  cp3_voice_vca(t, &t->v[voice])->phase == ENG_METAL_VCA_ATTACK);
}

static void test_cross_part_steal_and_reclaimed_vca_reset(void)
{
    track_t *a = &trk[0], *b = &trk[1];
    eng_metal_vca_t *stolen_vca;
    uint32_t i;
    int32_t out[CTL];

    cp3_vca_init();
    a = &trk[0];
    b = &trk[1];
    a->p[P_VOICE] = V_POLY;
    b->p[P_VOICE] = V_POLY;
    for (i = 0; i < NVOICE; ++i)
        trk_note_on_source(a, 40u + i, 100u, NOTE_SRC_PERFORMANCE);
    cp3_render_blocks(a, 8u);
    stolen_vca = cp3_voice_vca(a, &a->v[1]);
    cp3_vca_check("eight live voices fill Felucca's shared allocator budget",
                  voices_busy() == NVOICE && stolen_vca &&
                  stolen_vca->level_q16 > 0);

    trk_note_on_source(b, 80u, 100u, NOTE_SRC_PERFORMANCE);
    cp3_vca_check("ninth note steals oldest eligible non-bass voice across parts",
                  voices_busy() == NVOICE && a->v[0].gate && a->v[0].note == 40u &&
                  !a->v[1].gate && a->v[1].stage == 4u &&
                  b->v[0].active && b->v[0].note == 80u);

    memset(out, 0, sizeof(out));
    (void)track_render(a, out, CTL);
    cp3_vca_check("host stage-4 fade retires stolen voice after one render block",
                  !a->v[1].active && a->v[1].stage == 0u &&
                  stolen_vca->phase == ENG_METAL_VCA_RELEASE &&
                  stolen_vca->level_q16 > 0);

    trk_note_off(b, 80u);
    cp3_render_blocks(b, 1000u);
    cp3_vca_check("the new part's VCA completes Release and frees shared voice budget",
                  !b->v[0].active && voices_busy() == NVOICE - 1u);

    trk_note_on_source(a, 52u, 100u, NOTE_SRC_PERFORMANCE);
    cp3_vca_check("reclaimed host slot resets stale stolen VCA state before fresh Attack",
                  a->v[1].active && a->v[1].gate && a->v[1].note == 52u &&
                  stolen_vca->phase == ENG_METAL_VCA_ATTACK &&
                  stolen_vca->level_q16 == 0);
}

static void test_retrigger_from_release_level(void)
{
    track_t *t = &trk[0];
    eng_metal_vca_t *vca;
    int32_t held;
    cp3_vca_init();
    t = &trk[0];
    t->p[P_VOICE] = V_MONO;
    trk_note_on_source(t, 48u, 100u, NOTE_SRC_PERFORMANCE);
    cp3_render_blocks(t, 5u);
    vca = cp3_voice_vca(t, &t->v[0]);
    trk_note_off(t, 48u);
    cp3_render_blocks(t, 1u);
    held = vca->level_q16;
    trk_note_on_source(t, 55u, 100u, NOTE_SRC_PERFORMANCE);
    cp3_vca_check("real MONO retrigger during Release preserves instantaneous VCA level",
                  t->v[0].active && t->v[0].gate && t->v[0].note == 55u &&
                  vca->level_q16 == held &&
                  vca->phase == ENG_METAL_VCA_ATTACK);
}

int main(void)
{
    test_real_dispatch_uses_engine_owned_vca();
    test_gate_release_and_done_lifecycle();
    test_retrigger_from_release_level();
    test_cross_part_steal_and_reclaimed_vca_reset();
    test_engine_switch_replays_queued_vca_note();
    printf("CP3 real METAL VCA through Felucca host dispatcher: %s (%d failures)\n",
           cp3_vca_failures ? "FAIL" : "PASS", cp3_vca_failures);
    return cp3_vca_failures ? 1 : 0;
}
