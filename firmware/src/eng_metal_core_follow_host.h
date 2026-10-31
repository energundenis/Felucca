/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — Felucca host FOLLOW adapter.
 *
 * Include after core.h and the generated tables / dsp.c. This adapter reads
 * the host's per-voice note provenance and final vmod pitch increment. It is
 * deliberately not called by an engine render callback yet.
 */
#ifndef ENG_METAL_CORE_FOLLOW_HOST_H
#define ENG_METAL_CORE_FOLLOW_HOST_H

#include <stdint.h>
#include "eng_metal_core_follow.h"

/* The Felucca engine callback contract guarantees v is one of t->v[0..NVOICE).
 * v->note is the target MIDI note (before glide); m->inc is the final host
 * increment after glide, tune, bend, LFO, envelope, fine pitch and matrix.
 *
 * PERFORMANCE and SEQUENCER+FOLLOW ON are anchored to MIDI C4. SEQUENCER
 * +FOLLOW OFF removes the absolute step note but retains the host pitch delta.
 * All pitch_inc() values are from the same generated table at FS.
 *
 * The 64-bit division in the Q24 helper is block-rate, not sample-rate. It
 * must be measured on the target before this function is called by render.
 */
static inline uint32_t eng_metal_follow_host_render_inc(const track_t *t,
                                                         const voice_t *v,
                                                         const vmod_t *m,
                                                         uint32_t base_inc,
                                                         int follow_on)
{
    uint32_t voice_index, reference_inc, note_inc;
    uint8_t source;
    int sequencer_source;

    if (!t || !v || !m)
        return 0u;

    /* Valid by the Felucca engine_t.render(t, v, ...) callback contract. */
    voice_index = (uint32_t)(v - t->v);
    if (voice_index >= NVOICE)
        return 0u;

    source = t->voice_source[voice_index];
    if (source != NOTE_SRC_PERFORMANCE && source != NOTE_SRC_SEQUENCER)
        return 0u;

    sequencer_source = source == NOTE_SRC_SEQUENCER;
    reference_inc = pitch_inc(60 * 16);              /* host C4 anchor */
    note_inc = pitch_inc((int32_t)v->note * 16);     /* target step/key note */

    return eng_metal_follow_host_phase_inc(base_inc, FS, follow_on,
                                            sequencer_source, reference_inc,
                                            note_inc, m->inc);
}

#endif /* ENG_METAL_CORE_FOLLOW_HOST_H */
