/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — CP3 real Felucca dispatcher/sequencer integration test.
 *
 * Includes the same production sources as hostsim.c: voice.c and seq.c are
 * compiled into this translation unit. The test calls their real static
 * functions; it does not reproduce allocator, mono-stack, or step logic.
 * No engine registration, firmware image, hardware access, or Flash writes.
 */
#define main cp3_hostsim_main
#include "hostsim.c"
#undef main

#include <stdio.h>
#include <string.h>

static int cp3_failures;

static void cp3_check(const char *name, int ok)
{
    printf("CP3 real dispatcher: %-66s %s\n", name, ok ? "PASS" : "FAIL");
    cp3_failures += !ok;
}

static void cp3_reset_part(track_t *t)
{
    memset(t->v, 0, sizeof(t->v));
    memset(t->voice_source, 0, sizeof(t->voice_source));
    memset(t->mono_stack, 0, sizeof(t->mono_stack));
    memset(t->mono_source, 0, sizeof(t->mono_source));
    t->nmono = 0;
    t->mono_note = 0;
    t->rr = 0;
    t->seq_n = 0;
    t->seq_hold = 0;
    t->slide_glide = 0;
    t->xp_n = 0;
    t->xf_on = 0;
    t->xf = 0;
    t->eng_req = t->engine;
    t->p[P_MUTE] = 0;
    t->p[P_GLIDE] = 0;
    t->p[P_GLMODE] = 0;
}

static void cp3_init(void)
{
    memset(trk, 0, sizeof(trk));
    memset(&song, 0, sizeof(song));
    host_tracks_init();
    host_preset(&trk[0], 0u, 0u); /* real ANALOG engine callback, no test engine */
    cp3_reset_part(&trk[0]);
    cp3_reset_part(&trk[1]);
    cp3_reset_part(&trk[2]);
    cp3_reset_part(&trk[3]);
}

static uint32_t cp3_active_in_part(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; ++i)
        n += t->v[i].active != 0u;
    return n;
}

static void test_poly_note_lifecycle(void)
{
    track_t *t = &trk[0];
    uint32_t i, found = NVOICE;
    int32_t out[CTL];

    cp3_reset_part(t);
    t->p[P_VOICE] = V_POLY;
    trk_note_on_source(t, 60u, 100u, NOTE_SRC_PERFORMANCE);
    for (i = 0; i < NVOICE; ++i)
        if (t->v[i].active && t->v[i].note == 60u)
            found = i;
    cp3_check("real POLY dispatcher allocates a host voice and calls engine note_on",
              found < NVOICE && t->v[found].gate && t->v[found].stage == 1u);
    cp3_check("performance provenance is stored on the allocated voice",
              found < NVOICE && t->voice_source[found] == NOTE_SRC_PERFORMANCE);

    trk_note_off(t, 60u);
    cp3_check("real trk_note_off clears Gate and enters host Release",
              found < NVOICE && !t->v[found].gate && t->v[found].stage == 3u);

    memset(out, 0, sizeof(out));
    for (i = 0; i < 1600u && cp3_active_in_part(t); ++i)
        (void)track_render(t, out, CTL);
    cp3_check("real track_render advances the host envelope to voice retirement",
              cp3_active_in_part(t) == 0u);
}

static void test_mono_and_legato(void)
{
    track_t *t = &trk[0];
    uint32_t age;

    cp3_reset_part(t);
    t->p[P_VOICE] = V_MONO;
    trk_note_on_source(t, 60u, 96u, NOTE_SRC_PERFORMANCE);
    age = t->v[0].age;
    trk_note_on_source(t, 64u, 110u, NOTE_SRC_PERFORMANCE);
    cp3_check("MONO second note uses the real retrigger path",
              t->v[0].active && t->v[0].gate && t->v[0].note == 64u &&
              t->v[0].age > age);
    trk_note_off(t, 64u);
    cp3_check("MONO Note Off falls back to the still-held note",
              t->v[0].gate && t->v[0].note == 60u && t->mono_note == 60u);
    trk_note_off(t, 60u);
    cp3_check("MONO final Note Off releases its voice", !t->v[0].gate);

    cp3_reset_part(t);
    t->p[P_VOICE] = V_LEGATO;
    trk_note_on_source(t, 60u, 96u, NOTE_SRC_PERFORMANCE);
    age = t->v[0].age;
    trk_note_on_source(t, 67u, 110u, NOTE_SRC_SEQUENCER);
    cp3_check("LEGATO pitch change uses real dispatcher without restarting voice",
              t->v[0].active && t->v[0].gate && t->v[0].note == 67u &&
              t->v[0].age == age &&
              t->voice_source[0] == NOTE_SRC_SEQUENCER);
    trk_note_off(t, 67u);
    cp3_check("LEGATO fallback preserves the original held note and voice age",
              t->v[0].gate && t->v[0].note == 60u &&
              t->v[0].age == age);
    trk_note_off(t, 60u);
    cp3_check("LEGATO final Note Off releases the voice", !t->v[0].gate);
}

static void test_unison_and_voice_budget(void)
{
    track_t *t = &trk[0];
    uint32_t n;

    cp3_reset_part(t);
    t->p[P_VOICE] = V_UNISON;
    trk_note_on_source(t, 48u, 100u, NOTE_SRC_PERFORMANCE);
    n = cp3_active_in_part(t);
    cp3_check("UNISON starts independent host voices through real mono_play",
              n > 1u && n <= NVOICE);
    cp3_check("UNISON voices retain the same note and independent ages",
              t->v[0].gate && t->v[0].note == 48u &&
              t->voice_source[0] == NOTE_SRC_PERFORMANCE);
    trk_note_off(t, 48u);
    cp3_check("UNISON Note Off releases every configured unison Gate",
              !t->v[0].gate && !t->v[1].gate);
}

static void test_sequencer_source_accent_and_gate(void)
{
    track_t *t = &trk[0];
    uint32_t i, found;

    cp3_reset_part(t);
    t->p[P_VOICE] = V_POLY;
    t->p[P_SLEN] = 1;
    t->p[P_SGATE] = 64;
    t->p[P_SQNT] = 1;
    t->step[0] = (step_t){{72u}, 1u, ST_NOTE, SF_ACCENT, 80u};
    seq_start();
    seq_tick(t, CTL); /* first step is dispatched by the actual sequencer */
    found = NVOICE;
    for (i = 0; i < NVOICE; ++i)
        if (t->v[i].active && t->v[i].note == 72u)
            found = i;
    cp3_check("real seq_tick -> seq_play -> seq_step starts the programmed note",
              t->seq_n == 1u && t->seq_notes[0] == 72u && found < NVOICE);
    cp3_check("explicit step Accent reaches the real voice provenance metadata",
              found < NVOICE &&
              t->voice_source[found] == (NOTE_SRC_SEQUENCER | NOTE_META_ACCENT));

    song.playing = 0;
    t->seq_off = CTL;
    seq_tick(t, CTL);
    cp3_check("real sequencer Gate expiry calls trk_note_off",
              t->seq_n == 0u && found < NVOICE && !t->v[found].gate);

    cp3_reset_part(t);
    t->p[P_VOICE] = V_POLY;
    t->p[P_SLEN] = 1;
    t->p[P_SGATE] = 64;
    t->p[P_SQNT] = 1;
    t->step[0] = (step_t){{76u}, 1u, ST_NOTE, 0u, 127u};
    seq_start();
    seq_tick(t, CTL);
    found = NVOICE;
    for (i = 0; i < NVOICE; ++i)
        if (t->v[i].active && t->v[i].note == 76u)
            found = i;
    cp3_check("MIDI-like max velocity alone does not invent sequencer Accent",
              found < NVOICE &&
              t->voice_source[found] == NOTE_SRC_SEQUENCER);
    seq_stop();
}

static void test_engine_switch_queue_provenance(void)
{
    track_t *t = &trk[0];
    cp3_reset_part(t);
    t->xf_on = 1u;
    t->xf = 2u;
    trk_note_on_source(t, 55u, 91u,
                       NOTE_SRC_SEQUENCER | NOTE_META_ACCENT);
    cp3_check("engine-switch queue stores note, source and explicit Accent",
              t->xp_n == 1u &&
              (t->xp_note[0] & 0x7fu) == 55u &&
              (t->xp_note[0] & 0x80u) != 0u &&
              (t->xp_vel[0] & 0x7fu) == 91u &&
              (t->xp_vel[0] & 0x80u) != 0u);
    trk_note_off(t, 55u);
    cp3_check("Note Off removes the queued note during engine switch",
              t->xp_n == 0u);
}

int main(void)
{
    cp3_init();
    test_poly_note_lifecycle();
    test_mono_and_legato();
    test_unison_and_voice_budget();
    test_sequencer_source_accent_and_gate();
    test_engine_switch_queue_provenance();
    seq_stop();
    printf("CP3 real Felucca dispatcher/sequencer integration: %s (%d failures)\n",
           cp3_failures ? "FAIL" : "PASS", cp3_failures);
    return cp3_failures ? 1 : 0;
}
