/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — host FOLLOW adapter tests using Felucca's real tables.
 */
#include "felucca_tables.h"
#include "../firmware/src/core.h"
#include "../firmware/engines/src/dsp.c"
#include "../firmware/src/eng_metal_core_follow_host.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal FOLLOW host: %-62s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static int near_inc(uint32_t a, uint32_t b, uint32_t tolerance)
{
    return a > b ? a - b <= tolerance : b - a <= tolerance;
}

static void test_real_host_pitch_table(void)
{
    track_t t;
    voice_t *v;
    vmod_t m;
    uint32_t c4, got, expected, previous = 0u, note;
    int monotonic = 1, matches = 1;

    memset(&t, 0, sizeof(t));
    memset(&m, 0, sizeof(m));
    c4 = pitch_inc(60 * 16);
    t.voice_source[0] = NOTE_SRC_PERFORMANCE;
    v = &t.v[0];

    for (note = 0; note <= 127u; note++) {
        v->note = (uint8_t)note;
        m.inc = pitch_inc((int32_t)note * 16);
        got = eng_metal_follow_host_render_inc(&t, v, &m, c4, 0);
        expected = m.inc;
        if (!got || (note && got <= previous))
            monotonic = 0;
        if (!near_inc(got, expected, 4u))
            matches = 0;
        previous = got;
    }

    check("MIDI 0..127 maps to non-zero, strictly increasing increments",
          monotonic);
    check("all 128 notes match Felucca's actual pitch_inc() table",
          matches);
}

static void test_source_and_follow_modes(void)
{
    track_t t;
    voice_t *seq, *perf;
    vmod_t m;
    uint32_t c4, c5, c3, got;

    memset(&t, 0, sizeof(t));
    memset(&m, 0, sizeof(m));
    c4 = pitch_inc(60 * 16);
    c5 = pitch_inc(72 * 16);
    c3 = pitch_inc(48 * 16);

    seq = &t.v[2];
    perf = &t.v[0];
    seq->note = 72u;
    perf->note = 72u;
    m.inc = c5;
    t.voice_source[2] = NOTE_SRC_SEQUENCER;
    t.voice_source[0] = NOTE_SRC_PERFORMANCE;

    got = eng_metal_follow_host_render_inc(&t, seq, &m, c4, 1);
    check("sequencer + FOLLOW ON tracks the absolute step pitch",
          near_inc(got, c5, 4u));
    got = eng_metal_follow_host_render_inc(&t, seq, &m, c4, 0);
    check("sequencer + FOLLOW OFF restores base FREQ",
          near_inc(got, c4, 4u));

    got = eng_metal_follow_host_render_inc(&t, perf, &m, c4, 0);
    check("performance source remains keyboard-pitched with FOLLOW OFF",
          near_inc(got, c5, 4u));
    got = eng_metal_follow_host_render_inc(&t, perf, &m, c4, 1);
    check("performance source remains keyboard-pitched with FOLLOW ON",
          near_inc(got, c5, 4u));

    perf->note = 48u;
    m.inc = c3;
    got = eng_metal_follow_host_render_inc(&t, perf, &m, c4, 0);
    check("performance low register is retained below C4",
          near_inc(got, c3, 4u));
}

static void test_host_pitch_offsets_and_invalid_source(void)
{
    track_t t;
    voice_t *v;
    vmod_t m;
    uint32_t c4, target, got;

    memset(&t, 0, sizeof(t));
    memset(&m, 0, sizeof(m));
    v = &t.v[1];
    v->note = 72u;
    t.voice_source[1] = NOTE_SRC_SEQUENCER;
    c4 = pitch_inc(60 * 16);
    target = pitch_inc(72 * 16);
    m.inc = target + (target >> 10); /* representative final host pitch offset */

    got = eng_metal_follow_host_render_inc(&t, v, &m, c4, 0);
    check("FOLLOW OFF retains the host's residual pitch offset",
          got > c4 && got < c4 + (c4 >> 8));

    t.voice_source[1] = 255u;
    check("unknown voice source is rejected",
          eng_metal_follow_host_render_inc(&t, v, &m, c4, 1) == 0u);
    check("null host inputs are rejected",
          eng_metal_follow_host_render_inc(0, v, &m, c4, 1) == 0u &&
          eng_metal_follow_host_render_inc(&t, 0, &m, c4, 1) == 0u &&
          eng_metal_follow_host_render_inc(&t, v, 0, c4, 1) == 0u);
}

int main(void)
{
    test_real_host_pitch_table();
    test_source_and_follow_modes();
    test_host_pitch_offsets_and_invalid_source();
    printf("metal FOLLOW host: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
