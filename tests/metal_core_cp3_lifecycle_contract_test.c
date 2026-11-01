/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — CP3 lifecycle contract matrix.
 * Test-only use of Felucca's real track_t / voice_t and per-part VCA state.
 * This does not emulate host voice allocation or register an engine.
 */
#include "../firmware/src/core.h"
#include "../firmware/src/eng_metal_core_vca_render.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_PARTS 4u
#define TEST_ACCENT 2u
#define TEST_VOICE_COUNT NVOICE

typedef union {
    uint64_t align;
    uint8_t bytes[sizeof(eng_metal_vca_part_state_t)];
} part_mem_t;

static track_t tracks[TEST_PARTS];
static part_mem_t memories[TEST_PARTS];
static unsigned failures;
static unsigned checks;

static void check(const char *name, int ok)
{
    ++checks;
    printf("CP3 lifecycle: %-64s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static unsigned track_index(const track_t *t)
{
    uintptr_t base = (uintptr_t)&tracks[0];
    uintptr_t ptr = (uintptr_t)t;
    uintptr_t offset;
    if (!t || ptr < base || ptr >= base + sizeof(tracks))
        return TEST_PARTS;
    offset = ptr - base;
    if (offset % sizeof(track_t))
        return TEST_PARTS;
    return (unsigned)(offset / sizeof(track_t));
}

static unsigned voice_index(const track_t *t, const voice_t *v)
{
    uintptr_t base, ptr, offset;
    if (!t || !v)
        return TEST_VOICE_COUNT;
    base = (uintptr_t)&t->v[0];
    ptr = (uintptr_t)v;
    if (ptr < base || ptr >= base + sizeof(t->v))
        return TEST_VOICE_COUNT;
    offset = ptr - base;
    if (offset % sizeof(voice_t))
        return TEST_VOICE_COUNT;
    return (unsigned)(offset / sizeof(voice_t));
}

static eng_metal_vca_t *vca_for(track_t *t, voice_t *v)
{
    unsigned p = track_index(t);
    unsigned i = voice_index(t, v);
    eng_metal_vca_part_state_t *s;
    if (p >= TEST_PARTS || i >= TEST_VOICE_COUNT)
        return 0;
    s = (eng_metal_vca_part_state_t *)(void *)memories[p].bytes;
    return &s->voice[i];
}

static void trigger(track_t *t, unsigned i, unsigned velocity, unsigned metadata)
{
    int32_t peak, sustain;
    uint32_t velocity_q16;
    t->v[i].active = 1u;
    t->v[i].gate = 1u;
    t->v[i].vel = (uint8_t)velocity;
    t->voice_source[i] = (uint8_t)metadata;
    velocity_q16 = ((uint32_t)velocity * ENG_METAL_VCA_Q16_ONE + 63u) / 127u;
    eng_metal_vca_targets_q16(ENG_METAL_VCA_Q16_ONE, 32768,
                              (int32_t)velocity_q16, 1,
                              (metadata & TEST_ACCENT) != 0,
                              &peak, &sustain);
    eng_metal_vca_trigger(vca_for(t, &t->v[i]), peak, sustain,
                           221u, 441u, 13230u, UINT32_MAX, 882u);
}

static void render_samples(eng_metal_vca_t *v, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; ++i)
        (void)eng_metal_vca_process(v);
}

static void test_mapping_and_part_isolation(void)
{
    unsigned p, i;
    int distinct = 1;
    memset(tracks, 0, sizeof(tracks));
    memset(memories, 0, sizeof(memories));
    for (p = 0; p < TEST_PARTS; ++p) {
        for (i = 0; i < TEST_VOICE_COUNT; ++i) {
            eng_metal_vca_t *v = vca_for(&tracks[p], &tracks[p].v[i]);
            if (!v || track_index(&tracks[p]) != p || voice_index(&tracks[p], &tracks[p].v[i]) != i)
                distinct = 0;
            if (v && ((uintptr_t)v & 3u))
                distinct = 0;
        }
    }
    check("all real host voice slots map to aligned independent VCA states", distinct);
    check("part memory regions are isolated",
          vca_for(&tracks[0], &tracks[0].v[0]) != vca_for(&tracks[1], &tracks[1].v[0]) &&
          vca_for(&tracks[1], &tracks[1].v[0]) != vca_for(&tracks[2], &tracks[2].v[0]) &&
          vca_for(&tracks[2], &tracks[2].v[0]) != vca_for(&tracks[3], &tracks[3].v[0]));
    check("invalid part and voice pointers are rejected",
          vca_for((track_t *)(uintptr_t)1u, &tracks[0].v[0]) == 0 &&
          vca_for(&tracks[0], (voice_t *)(uintptr_t)1u) == 0);
}

static void test_velocity_and_explicit_accent(void)
{
    track_t *t = &tracks[0];
    eng_metal_vca_t *plain, *accented;
    memset(t, 0, sizeof(*t));
    memset(&memories[0], 0, sizeof(memories[0]));
    trigger(t, 0u, 127u, 0u);
    trigger(t, 1u, 127u, TEST_ACCENT);
    plain = vca_for(t, &t->v[0]);
    accented = vca_for(t, &t->v[1]);
    check("velocity 127 without Accent remains unaccented",
          t->v[0].vel == 127u && t->voice_source[0] == 0u &&
          plain->target_q16 == ENG_METAL_VCA_Q16_ONE);
    check("explicit Accent changes target independently of MIDI velocity",
          t->v[1].vel == t->v[0].vel &&
          t->voice_source[1] == TEST_ACCENT &&
          accented->target_q16 > plain->target_q16);
}

static void test_retrigger_release_and_fresh_slot_reuse(void)
{
    track_t *t = &tracks[0];
    eng_metal_vca_t *v;
    int32_t held;
    unsigned blocks = 0u;
    memset(t, 0, sizeof(*t));
    memset(&memories[0], 0, sizeof(memories[0]));
    trigger(t, 0u, 100u, 0u);
    v = vca_for(t, &t->v[0]);
    render_samples(v, 64u);
    held = v->level_q16;
    eng_metal_vca_gate_off(v);
    render_samples(v, 32u);
    check("released-but-not-idle voice remains non-retirable",
          v->phase == ENG_METAL_VCA_RELEASE && !eng_metal_vca_done(v));
    held = v->level_q16;
    trigger(t, 0u, 100u, 0u);
    check("sounding retrigger cancels Release and preserves instantaneous level",
          v->phase == ENG_METAL_VCA_ATTACK && v->level_q16 == held);
    render_samples(v, 16u);
    check("retrigger resumes from held level without a zero-level reset",
          v->level_q16 >= held);
    eng_metal_vca_gate_off(v);
    while (blocks++ < 20000u && !eng_metal_vca_done(v))
        (void)eng_metal_vca_process(v);
    check("voice becomes retirable only after Release reaches idle",
          eng_metal_vca_done(v) && v->phase == ENG_METAL_VCA_IDLE);

    /* Host contract: a fresh slot must reset part-owned state before reuse. */
    eng_metal_vca_reset(v);
    trigger(t, 0u, 127u, 0u);
    check("fresh slot reuse starts from reset state, not stale Release level",
          v->phase == ENG_METAL_VCA_ATTACK && v->level_q16 == 0 &&
          v->target_q16 == ENG_METAL_VCA_Q16_ONE);
}

int main(void)
{
    test_mapping_and_part_isolation();
    test_velocity_and_explicit_accent();
    test_retrigger_release_and_fresh_slot_reuse();
    printf("CP3 lifecycle: %s (%u checks, %u failures)\n",
           failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
