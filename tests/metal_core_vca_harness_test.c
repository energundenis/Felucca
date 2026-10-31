/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — CP2.23 VCA-only host integration harness.
 * Exercises the pinned Felucca engine_t callback contract without registering
 * METAL CORE or changing the production audio graph.
 */
#define NVOICE 8
#define NPART 1
#define CTL 32u
#include "../firmware/engines/src/felucca_engine.h"
#include "../firmware/src/eng_metal_core_vca_render.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ENG_MEM_PART 12888u
#define HARNESS_NOTE_META_ACCENT 2u
#define HARNESS_SOURCE_SAMPLE 12000

typedef union {
    uint64_t align;
    uint8_t bytes[ENG_MEM_PART];
} harness_engine_mem_t;

static harness_engine_mem_t harness_mem[NPART];

struct track {
    voice_t v[NVOICE];
    uint8_t voice_source[NVOICE];
    uint8_t part_index;
};
typedef struct track track_t;

static int failures;

static void check(const char *name, int ok)
{
    printf("CP2.23 VCA harness: %-64s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
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

static eng_metal_vca_part_state_t *part_state(track_t *t)
{
    if (!t || t->part_index >= NPART)
        return 0;
    return (eng_metal_vca_part_state_t *)(void *)harness_mem[t->part_index].bytes;
}

static eng_metal_vca_t *voice_vca(track_t *t, voice_t *v)
{
    uint32_t i = voice_index(t, v);
    eng_metal_vca_part_state_t *s = part_state(t);
    return i < NVOICE && s ? &s->voice[i] : 0;
}

static void harness_note_on(struct track *track, voice_t *voice)
{
    track_t *t = (track_t *)track;
    uint32_t i = voice_index(t, voice);
    eng_metal_vca_t *vca = voice_vca(t, voice);
    int32_t peak, sustain, velocity;
    if (!vca || i >= NVOICE)
        return;
    velocity = ((int32_t)voice->vel * ENG_METAL_VCA_Q16_ONE + 63) / 127;
    eng_metal_vca_targets_q16(ENG_METAL_VCA_Q16_ONE, 32768,
                              velocity, 1,
                              (t->voice_source[i] & HARNESS_NOTE_META_ACCENT) != 0,
                              &peak, &sustain);
    eng_metal_vca_trigger(vca, peak, sustain,
                          221u, 441u, 13230u,
                          UINT32_MAX, 882u);
}

static void harness_render(struct track *track, voice_t *voice,
                           int32_t *out, uint32_t n, const vmod_t *m)
{
    track_t *t = (track_t *)track;
    eng_metal_vca_t *vca = voice_vca(t, voice);
    if (!vca || !m || !out)
        return;
    if (!voice->gate)
        eng_metal_vca_gate_off(vca);
    eng_metal_vca_render_constant(vca, out, n, HARNESS_SOURCE_SAMPLE,
                                  m->amp0, m->amp1);
}

static int harness_done(struct track *track, voice_t *voice)
{
    eng_metal_vca_t *vca = voice_vca((track_t *)track, voice);
    return !vca || eng_metal_vca_done(vca);
}

/* The real engine_t contract: ownenv + note_on/render/done + per-part mem. */
static const engine_t ENG_VCA_ONLY_HARNESS = {
    .name = "VCA TEST",
    .ownenv = 1u,
    .note_on = harness_note_on,
    .render = harness_render,
    .done = harness_done,
    .mem = (uint16_t)sizeof(eng_metal_vca_part_state_t)
};

_Static_assert(sizeof(eng_metal_vca_part_state_t) <= ENG_MEM_PART,
               "VCA-only state must fit the Felucca per-part engine memory");

static void init_voice(track_t *t, uint32_t i, uint8_t velocity, uint8_t metadata)
{
    memset(&t->v[i], 0, sizeof(t->v[i]));
    t->v[i].active = 1u;
    t->v[i].gate = 1u;
    t->v[i].vel = velocity;
    t->v[i].note = (uint8_t)(48u + i);
    t->voice_source[i] = metadata;
    ENG_VCA_ONLY_HARNESS.note_on(t, &t->v[i]);
}

static void render_voice(track_t *t, uint32_t i, int32_t *out,
                         int32_t amp0, int32_t amp1)
{
    vmod_t m;
    memset(&m, 0, sizeof(m));
    m.amp0 = amp0;
    m.amp1 = amp1;
    ENG_VCA_ONLY_HARNESS.render(t, &t->v[i], out, CTL, &m);
}

static void test_memory_and_voice_isolation(void)
{
    track_t t;
    eng_metal_vca_part_state_t *s;
    uint32_t i;
    memset(&t, 0, sizeof(t));
    memset(harness_mem, 0, sizeof(harness_mem));
    s = part_state(&t);
    check("engine_t.mem declares exactly eight VCA states (288 bytes)",
          ENG_VCA_ONLY_HARNESS.mem == sizeof(*s) &&
          ENG_VCA_ONLY_HARNESS.mem == 288u);
    check("per-part state fits the 12,888-byte ENG_MEM_PART region",
          sizeof(*s) <= ENG_MEM_PART);
    check("engine memory is suitably aligned",
          ((uintptr_t)s & 7u) == 0u);

    for (i = 0; i < NVOICE; ++i)
        init_voice(&t, i, 127u, 0u);
    for (i = 0; i < NVOICE; ++i) {
        uint32_t j;
        int unique = 1;
        for (j = 0; j < NVOICE; ++j)
            if (i != j && &s->voice[i] == &s->voice[j])
                unique = 0;
        check("each host voice owns a distinct VCA state", unique &&
              s->voice[i].phase == ENG_METAL_VCA_ATTACK);
    }
    {
        int32_t before[NVOICE];
        for (i = 0; i < NVOICE; ++i)
            before[i] = s->voice[i].level_q16;
        (void)eng_metal_vca_process(&s->voice[3]);
        for (i = 0; i < NVOICE; ++i)
            if (i != 3u)
                check("processing one voice leaves the other seven unchanged",
                      s->voice[i].level_q16 == before[i] &&
                      s->voice[i].phase == ENG_METAL_VCA_ATTACK);
    }
    check("invalid voice pointer is rejected",
          voice_vca(&t, (voice_t *)(uintptr_t)1u) == 0);
}

static void test_render_host_ramp_and_additive_output(void)
{
    track_t t;
    eng_metal_vca_part_state_t *s;
    int32_t out[CTL];
    uint32_t i;
    int monotonic = 1;
    memset(&t, 0, sizeof(t));
    memset(harness_mem, 0, sizeof(harness_mem));
    memset(out, 0, sizeof(out));
    init_voice(&t, 0u, 127u, 0u);
    for (i = 0; i < CTL; ++i)
        out[i] = 500;
    render_voice(&t, 0u, out, 0, 32767);
    s = part_state(&t);
    for (i = 1; i < CTL; ++i)
        if (out[i] < out[i - 1u])
            monotonic = 0;
    check("VCA render adds to existing output instead of overwriting it",
          out[0] >= 500 && out[CTL - 1u] > 500);
    check("VCA attack is multiplied by the host amp0-to-amp1 ramp",
          monotonic && out[CTL - 1u] > out[0] &&
          s->voice[0].level_q16 > 0);
}

static void test_gate_retrigger_release_and_done(void)
{
    track_t t;
    eng_metal_vca_part_state_t *s;
    int32_t out[CTL];
    int32_t held;
    uint32_t blocks = 0;
    int saw_done_after_render = 0;
    memset(&t, 0, sizeof(t));
    memset(harness_mem, 0, sizeof(harness_mem));
    memset(out, 0, sizeof(out));
    init_voice(&t, 0u, 100u, HARNESS_NOTE_META_ACCENT);
    render_voice(&t, 0u, out, 32767, 32767);
    s = part_state(&t);
    held = s->voice[0].level_q16;

    t.v[0].gate = 0u;
    render_voice(&t, 0u, out, 32767, 32767);
    check("Gate-off at block boundary enters Release and descends",
          s->voice[0].phase == ENG_METAL_VCA_RELEASE &&
          s->voice[0].level_q16 < held);

    held = s->voice[0].level_q16;
    t.v[0].gate = 1u;
    ENG_VCA_ONLY_HARNESS.note_on(&t, &t.v[0]);
    check("Retrigger during Release preserves the instantaneous VCA level",
          s->voice[0].level_q16 == held &&
          s->voice[0].phase == ENG_METAL_VCA_ATTACK);
    render_voice(&t, 0u, out, 32767, 32767);
    check("Retrigger advances from the held level without a zero reset",
          s->voice[0].level_q16 >= held);

    t.v[0].gate = 0u;
    render_voice(&t, 0u, out, 32767, 32767);
    check("second Gate-off keeps the VCA in Release",
          s->voice[0].phase == ENG_METAL_VCA_RELEASE);
    check("done() remains false while Release is active",
          !ENG_VCA_ONLY_HARNESS.done(&t, &t.v[0]));

    while (blocks++ < 1000u && !ENG_VCA_ONLY_HARNESS.done(&t, &t.v[0]))
        render_voice(&t, 0u, out, 32767, 32767);
    saw_done_after_render = ENG_VCA_ONLY_HARNESS.done(&t, &t.v[0]);
    check("Release reaches idle and done() retires at the next control tick",
          saw_done_after_render && s->voice[0].phase == ENG_METAL_VCA_IDLE &&
          blocks < 1000u);
}

int main(void)
{
    test_memory_and_voice_isolation();
    test_render_host_ramp_and_additive_output();
    test_gate_retrigger_release_and_done();
    printf("CP2.23 VCA harness: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
