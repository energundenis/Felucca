/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — Felucca per-voice oscillator adapter.
 *
 * Uses voice_t.ph[0] and ph[1] as the independent OSC1 / OSC2 phase
 * accumulators. voice_start() preserves these slots across a sounding
 * retrigger (the host saves/restores ph[0..2]); a fresh/reused voice receives
 * the reset performed by the engine note_on callback.
 *
 * Include after core.h and the Felucca shared DSP definitions (dsp.c).
 * This adapter does not allocate state, alter voice_t ABI, or register an
 * engine. The caller supplies waveform and pulse width from the parameter map.
 */
#ifndef ENG_METAL_CORE_OSC_VOICE_H
#define ENG_METAL_CORE_OSC_VOICE_H

#include <stdint.h>
#include "eng_metal_core_osc.h"

#define ENG_METAL_OSC1_PHASE_SLOT 0u
#define ENG_METAL_OSC2_PHASE_SLOT 1u

/* Call from the engine's note_on callback. On a fresh/reused host voice,
 * phase starts at zero. On a sounding retrigger, Felucca voice_start()
 * restores the pre-callback phase slots after note_on returns, preserving
 * oscillator continuity without special retrigger flags.
 */
static inline void eng_metal_osc_voice_note_on(voice_t *v)
{
    if (!v)
        return;
    v->ph[ENG_METAL_OSC1_PHASE_SLOT] = 0u;
    v->ph[ENG_METAL_OSC2_PHASE_SLOT] = 0u;
}

/* Render one sample from OSC1 or OSC2 using the final host pitch increment.
 * m->inc already includes host glide, tune, bend and modulation processing.
 */
static inline int32_t eng_metal_osc_voice1_next(voice_t *v,
                                                 const vmod_t *m,
                                                 uint32_t waveform,
                                                 uint32_t pulse_width)
{
    if (!v || !m)
        return 0;
    return eng_metal_osc_next(&v->ph[ENG_METAL_OSC1_PHASE_SLOT],
                              m->inc, waveform, pulse_width);
}

static inline int32_t eng_metal_osc_voice2_next(voice_t *v,
                                                 const vmod_t *m,
                                                 uint32_t waveform,
                                                 uint32_t pulse_width)
{
    if (!v || !m)
        return 0;
    return eng_metal_osc_next(&v->ph[ENG_METAL_OSC2_PHASE_SLOT],
                              m->inc, waveform, pulse_width);
}

#endif /* ENG_METAL_CORE_OSC_VOICE_H */
