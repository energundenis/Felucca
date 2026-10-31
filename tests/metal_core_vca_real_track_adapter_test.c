/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — CP3.0 adapter probe using Felucca's real track_t.
 *
 * This remains a test-only descriptor: it is not included in engines.c and
 * does not register a user-selectable engine. Unlike the CP2.23 fixture, this
 * test uses the actual core.h track_t / voice_t / vmod_t structures.
 */
#include "../firmware/src/core.h"
#include "../firmware/src/eng_metal_core_vca_render.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_ENG_MEM_PART_MAX 13344u
#define TEST_NOTE_META_ACCENT 2u
#define TEST_SOURCE_SAMPLE 12000
#define TEST_CTL 32u /* Felucca engine render quantum. */

typedef union {
    uint64_t align;
    uint8_t bytes[TEST_ENG_MEM_PART_MAX];
} test_engine_mem_t;

static track_t test_tracks[NPART];
static test_engine_mem_t test_mem[NPART];
static int failures;

static void check(const char *name, int ok)
{
    printf("CP3.0 real-track adapter: %-60s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static uint32_t part_index(const track_t *t)
{
    uintptr_t base, ptr, offset;
    if (!t)
        return NPART;
    base = (uintptr_t)&test_tracks[0];
    ptr = (uintptr_t)t;
    if (ptr < base || ptr >= base + sizeof(test_tracks))
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

static eng_metal_vca_part_state_t *part_state(track_t *t)
{
    uint32_t p = part_index(t);
    if (p >= NPART)
        return 0;
    return (eng_metal_vca_part_state_t *)(void *)test_mem[p].bytes;
}

static eng_metal_vca_t *voice_vca(track_t *t, voice_t *voice)
{
    uint32_t i = voice_index(t, voice);
    eng_metal_vca_part_state_t *state = part_state(t);
    return i < NVOICE && state ? &state->voice[i] : 0;
}

static void test_note_on(struct track *track, voice_t *voice)
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
                              (t->voice_source[i] & TEST_NOTE_META_ACCENT) != 0,
                              &peak, &sustain);
    eng_metal_vca_trigger(vca, peak, sustain,
                          221u, 441u, 13230u,
                          UINT32_MAX, 882u);
}

static void test_render(struct track *track, voice_t *voice,
                        int32_t *out, uint32_t n, const vmod_t *m)
{
    track_t *t = (track_t *)track;
    eng_metal_vca_t *vca = voice_vca(t, voice);
    if (!vca || !out || !m)
        return;
    if (!voice->gate)
        eng_metal_vca_gate_off(vca);
    eng_metal_vca_render_constant(vca, out, n, TEST_SOURCE_SAMPLE,
                                  m->amp0, m->amp1);
}

static int test_done(struct track *track, voice_t *voice)
{
    eng_metal_vca_t *vca = voice_vca((track_t *)track, voice);
    return !vca || eng_metal_vca_done(vca);
}

static const engine_t ENG_METAL_CORE_CP3_ADAPTER = {
    .name = "MC CP3 TEST",
    .ownenv = 1u,
    .note_on = test_note_on,
    .render = test_render,
    .done = test_done,
    .mem = (uint16_t)sizeof(eng_metal_vca_part_state_t)
};

_Static_assert(sizeof(eng_metal_vca_part_state_t) == 288u,
               "eight VCA states must remain 288 bytes");
_Static_assert(sizeof(eng_metal_vca_part_state_t) <= 12888u,
               "VCA state must fit the non-ADPCM per-part pool");
_Static_assert(sizeof(eng_metal_vca_part_state_t) <= 13344u,
               "VCA state must fit the ADPCM per-part pool");

static void init_voice(track_t *t, uint32_t i, uint8_t velocity, uint8_t metadata)
{
    memset(&t->v[i], 0, sizeof(t->v[i]));
    t->v[i].active = 1u;
    t->v[i].gate = 1u;
    t->v[i].vel = velocity;
    t->v[i].note = (uint8_t)(48u + i);
    t->voice_source[i] = metadata;
    ENG_METAL_CORE_CP3_ADAPTER.note_on(t, &t->v[i]);
}

static void render_voice(track_t *t, uint32_t i, int32_t *out,
                         int32_t amp0, int32_t amp1)
{
    vmod_t m;
    memset(&m, 0, sizeof(m));
    m.amp0 = amp0;
    m.amp1 = amp1;
    ENG_METAL_CORE_CP3_ADAPTER.render(t, &t->v[i], out, TEST_CTL, &m);
}

int main(void)
{
    track_t *t = &test_tracks[0];
    eng_metal_vca_part_state_t *state;
    int32_t out[TEST_CTL];
    int32_t unaccented_peak;
    uint32_t i, blocks = 0;
    int monotonic = 1;

    memset(test_tracks, 0, sizeof(test_tracks));
    memset(test_mem, 0, sizeof(test_mem));
    memset(out, 0, sizeof(out));
    state = part_state(t);

    check("adapter compiles against the actual Felucca track_t and voice_t",
          sizeof(t->v) / sizeof(t->v[0]) == NVOICE &&
          sizeof(t->voice_source) == NVOICE);
    check("engine descriptor uses ownenv and eight-state 288-byte memory",
          ENG_METAL_CORE_CP3_ADAPTER.ownenv == 1u &&
          ENG_METAL_CORE_CP3_ADAPTER.mem == 288u);
    check("288-byte VCA state fits both 12,888 and 13,344-byte pools",
          sizeof(*state) <= 12888u && sizeof(*state) <= TEST_ENG_MEM_PART_MAX);
    check("real per-part test pool is 64-bit aligned",
          ((uintptr_t)state & 7u) == 0u);
    check("four Felucca parts map to independent VCA memory regions",
          part_state(&test_tracks[0]) != part_state(&test_tracks[1]) &&
          part_state(&test_tracks[1]) != part_state(&test_tracks[2]) &&
          part_state(&test_tracks[2]) != part_state(&test_tracks[3]));

    for (i = 0; i < NVOICE; ++i)
        init_voice(t, i, 100u, i == 0u ? TEST_NOTE_META_ACCENT : 0u);
    check("eight real host voice slots receive independent VCA states",
          state->voice[0].phase == ENG_METAL_VCA_ATTACK &&
          state->voice[7].phase == ENG_METAL_VCA_ATTACK &&
          &state->voice[0] != &state->voice[7]);
    unaccented_peak = state->voice[1].target_q16;
    check("Accent metadata is read separately from the real voice velocity",
          state->voice[0].target_q16 > unaccented_peak &&
          t->v[0].vel == t->v[1].vel);
    check("invalid voice pointer is rejected",
          voice_vca(t, (voice_t *)(uintptr_t)1u) == 0);

    for (i = 0; i < TEST_CTL; ++i)
        out[i] = 250;
    render_voice(t, 0u, out, 0, 32767);
    for (i = 1; i < TEST_CTL; ++i)
        if (out[i] < out[i - 1u])
            monotonic = 0;
    check("render is additive and preserves the host amp ramp",
          out[0] >= 250 && out[TEST_CTL - 1u] > out[0] && monotonic);

    t->v[0].gate = 0u;
    render_voice(t, 0u, out, 32767, 32767);
    check("real voice Gate-off enters Release at render-block boundary",
          state->voice[0].phase == ENG_METAL_VCA_RELEASE);
    check("done() remains false while Release is active",
          !ENG_METAL_CORE_CP3_ADAPTER.done(t, &t->v[0]));
    while (blocks++ < 1000u && !ENG_METAL_CORE_CP3_ADAPTER.done(t, &t->v[0]))
        render_voice(t, 0u, out, 32767, 32767);
    check("done() retires the voice only after VCA reaches idle",
          ENG_METAL_CORE_CP3_ADAPTER.done(t, &t->v[0]) &&
          state->voice[0].phase == ENG_METAL_VCA_IDLE && blocks < 1000u);

    printf("CP3.0 real-track adapter: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
