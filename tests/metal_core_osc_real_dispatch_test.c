/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — OSC adapter through Felucca's real voice lifecycle.
 *
 * Compiles the actual production voice.c / track_render() path through
 * hostsim.c. A private test descriptor invokes the production OSC adapter;
 * no production engine ID or registry entry is added.
 */
#define FELUCCA_CP3_TEST_MUTABLE_ENGINE_REGISTRY 1
#define main cp3_osc_hostsim_main
#include "hostsim.c"
#undef main

#include "../firmware/src/eng_metal_core_osc_voice.h"

#include <stdio.h>
#include <string.h>

static int failures;
static uint32_t render_calls;
static uint32_t render_inc;
static uint32_t render_n;
static uint32_t render_start_osc1;
static uint32_t render_start_osc2;

static void check(const char *name, int ok)
{
    printf("CP3 real OSC dispatch: %-70s %s\n",
           name, ok ? "PASS" : "FAIL");
    failures += !ok;
}

static void cp3_osc_note_on(struct track *track, voice_t *voice)
{
    (void)track;
    eng_metal_osc_voice_note_on(voice);
}

static void cp3_osc_render(struct track *track, voice_t *voice,
                           int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i;
    (void)track;
    if (!voice || !out || !m)
        return;

    ++render_calls;
    render_inc = m->inc;
    render_n = n;
    render_start_osc1 = voice->ph[ENG_METAL_OSC1_PHASE_SLOT];
    render_start_osc2 = voice->ph[ENG_METAL_OSC2_PHASE_SLOT];

    for (i = 0; i < n; ++i) {
        int32_t osc1 = eng_metal_osc_voice1_next(
            voice, m, ENG_METAL_OSC_SAW, 0x80000000u);
        int32_t osc2 = eng_metal_osc_voice2_next(
            voice, m, ENG_METAL_OSC_TRI, 0x80000000u);
        out[i] += (osc1 + osc2) / 2;
    }
}

static const engine_t ENG_METAL_CORE_OSC_CP3_TEST = {
    .name = "MC OSC CP3 TEST",
    .poly = NVOICE,
    .note_on = cp3_osc_note_on,
    .render = cp3_osc_render
};

static void setup_host(void)
{
    uint32_t p;
    memset(trk, 0, sizeof(trk));
    memset(&song, 0, sizeof(song));
    host_tracks_init();
    ENGINES[0] = &ENG_METAL_CORE_OSC_CP3_TEST;
    for (p = 0; p < NPART; ++p) {
        trk[p].engine = trk[p].eng_req = 0u;
        trk[p].p[P_VOICE] = V_MONO;
        trk[p].p[P_MUTE] = 0;
        trk[p].p[P_GLIDE] = 0;
    }
    render_calls = 0;
    render_inc = 0;
    render_n = 0;
}

static void test_fresh_note_and_real_render_increment(void)
{
    track_t *t;
    voice_t *v;
    int32_t out[CTL];
    uint32_t expected1, expected2;

    setup_host();
    t = &trk[0];
    v = &t->v[0];
    v->ph[0] = 0x11111111u;
    v->ph[1] = 0x22222222u;
    v->ph[2] = 0x33333333u;

    trk_note_on_source(t, 60u, 100u, NOTE_SRC_PERFORMANCE);
    check("real host note dispatch selects the private OSC test descriptor",
          ENGINES[t->engine] == &ENG_METAL_CORE_OSC_CP3_TEST &&
          v->active && v->gate && v->note == 60u);
    check("fresh voice_start invokes OSC reset for phase slots 0 and 1 only",
          v->ph[0] == 0u && v->ph[1] == 0u &&
          v->ph[2] == 0x33333333u);

    memset(out, 0, sizeof(out));
    render_calls = 0;
    (void)track_render(t, out, CTL);
    expected1 = render_start_osc1 + render_inc * render_n;
    expected2 = render_start_osc2 + render_inc * render_n;
    check("real track_render invokes the OSC callback once with host pitch increment",
          render_calls == 1u && render_n == CTL && render_inc != 0u);
    check("both OSC phase slots advance by the actual host vmod_t.inc for every sample",
          v->ph[0] == expected1 && v->ph[1] == expected2 &&
          (v->ph[0] != 0u || v->ph[1] != 0u));
}

static void test_real_voice_start_retrigger_preserves_phases(void)
{
    track_t *t = &trk[0];
    voice_t *v = &t->v[0];
    const uint32_t phase0 = 0x10203040u;
    const uint32_t phase1 = 0x50607080u;
    const uint32_t phase2 = 0x90ABCDEFu;

    v->ph[0] = phase0;
    v->ph[1] = phase1;
    v->ph[2] = phase2;
    trk_note_on_source(t, 62u, 96u, NOTE_SRC_PERFORMANCE);

    check("real MONO retrigger passes through production voice_start()",
          v->active && v->gate && v->note == 62u);
    check("host voice_start restores OSC1, OSC2 and host phase slot 2 after note_on reset",
          v->ph[0] == phase0 && v->ph[1] == phase1 && v->ph[2] == phase2);
}

static void test_retirement_then_fresh_note_resets_stale_phases(void)
{
    track_t *t = &trk[0];
    voice_t *v = &t->v[0];
    int32_t out[CTL];
    uint32_t blocks = 0;

    trk_note_off(t, 62u);
    trk_note_off(t, 60u); /* release the older held note after MONO fallback */
    while (v->active && blocks < 1200u) {
        memset(out, 0, sizeof(out));
        (void)track_render(t, out, CTL);
        ++blocks;
    }
    check("real Note Off and track_render retire the released host voice",
          !v->active && blocks < 1200u);

    v->ph[0] = 0xA1A2A3A4u;
    v->ph[1] = 0xB1B2B3B4u;
    v->ph[2] = 0xC1C2C3C4u;
    trk_note_on_source(t, 67u, 100u, NOTE_SRC_PERFORMANCE);
    check("next fresh note resets stale OSC phases after host voice retirement",
          v->active && v->gate && v->note == 67u &&
          v->ph[0] == 0u && v->ph[1] == 0u &&
          v->ph[2] == 0xC1C2C3C4u);
}

int main(void)
{
    test_fresh_note_and_real_render_increment();
    test_real_voice_start_retrigger_preserves_phases();
    test_retirement_then_fresh_note_resets_stale_phases();
    printf("CP3 real OSC through Felucca voice lifecycle: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
