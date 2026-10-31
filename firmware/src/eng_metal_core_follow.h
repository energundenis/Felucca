/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated fixed-point FOLLOW pitch mapping.
 *
 * Reference-copy-derived note offsets only. This is not connected to the
 * engine or host note-source path; direct v98 verification remains open.
 */
#ifndef ENG_METAL_CORE_FOLLOW_H
#define ENG_METAL_CORE_FOLLOW_H

#include <stdint.h>

#define ENG_METAL_FOLLOW_Q24_ONE 16777216u
#define ENG_METAL_FOLLOW_NOTE_ROOT (-1)
#define ENG_METAL_FOLLOW_NOTE_MIN 0
#define ENG_METAL_FOLLOW_NOTE_MAX 11
#define ENG_METAL_FOLLOW_OCTAVE_TENTHS_MIN (-20)
#define ENG_METAL_FOLLOW_OCTAVE_TENTHS_MAX 20
#define ENG_METAL_FOLLOW_FREQ_MIN_HZ 30u
#define ENG_METAL_FOLLOW_FREQ_MAX_HZ 500u

/* 2^(semitone / 12), Q24. Generated offline; no floating point at runtime. */
static const uint32_t eng_metal_follow_semitone_q24[12] = {
    16777216u, 17774841u, 18831788u, 19951585u,
    21137968u, 22394897u, 23726566u, 25137421u,
    26632170u, 28215802u, 29893600u, 31671166u
};

/* 2^(octave_tenths / 10), Q24; index 20 represents zero octaves. */
static const uint32_t eng_metal_follow_octave_q24[41] = {
     4194304u,  4495344u,  4817990u,  5163794u,  5534417u,
     5931642u,  6357376u,  6813667u,  7302707u,  7826848u,
     8388608u,  8990687u,  9635980u, 10327588u, 11068835u,
    11863283u, 12714752u, 13627334u, 14605415u, 15653696u,
    16777216u, 17981375u, 19271960u, 20655176u, 22137669u,
    23726566u, 25429504u, 27254668u, 29210830u, 31307392u,
    33554432u, 35962750u, 38543921u, 41310351u, 44275338u,
    47453133u, 50859008u, 54509336u, 58421659u, 62614784u,
    67108864u
};

/* Returns the reference-copy frequency multiplier in Q24.
 * note=-1 selects ROOT and uses the supplied OCTAVE value.
 * note=0..11 selects a chromatic semitone offset and uses NOTE OCTAVE.
 * octave_tenths is an integer in 0.1-octave units (UI step: 2 tenths).
 * Inputs outside the documented octave range are clamped; invalid notes
 * return zero so the caller can reject the event rather than wrap it.
 */
static inline uint32_t eng_metal_follow_ratio_q24(int32_t note,
                                                  int32_t octave_tenths)
{
    uint32_t octave;
    uint64_t product;

    if (note < ENG_METAL_FOLLOW_NOTE_ROOT ||
        note > ENG_METAL_FOLLOW_NOTE_MAX)
        return 0u;

    if (octave_tenths < ENG_METAL_FOLLOW_OCTAVE_TENTHS_MIN)
        octave_tenths = ENG_METAL_FOLLOW_OCTAVE_TENTHS_MIN;
    if (octave_tenths > ENG_METAL_FOLLOW_OCTAVE_TENTHS_MAX)
        octave_tenths = ENG_METAL_FOLLOW_OCTAVE_TENTHS_MAX;

    octave = eng_metal_follow_octave_q24[octave_tenths + 20];
    if (note == ENG_METAL_FOLLOW_NOTE_ROOT)
        return octave;

    product = (uint64_t)eng_metal_follow_semitone_q24[note] * octave;
    return (uint32_t)((product + (1ull << 23)) >> 24);
}

/* Convert base FREQ (Hz) and the selected reference-copy note offset into a
 * Q0.32 phase increment (cycles/sample). Division is event-rate, not per-sample.
 * A zero sample rate or invalid note returns zero.
 */
static inline uint32_t eng_metal_follow_phase_inc(uint32_t base_freq_hz,
                                                   uint32_t sample_rate,
                                                   int32_t note,
                                                   int32_t octave_tenths)
{
    uint32_t ratio;
    uint64_t numerator, result;

    if (!sample_rate)
        return 0u;
    ratio = eng_metal_follow_ratio_q24(note, octave_tenths);
    if (!ratio)
        return 0u;

    if (base_freq_hz < ENG_METAL_FOLLOW_FREQ_MIN_HZ)
        base_freq_hz = ENG_METAL_FOLLOW_FREQ_MIN_HZ;
    if (base_freq_hz > ENG_METAL_FOLLOW_FREQ_MAX_HZ)
        base_freq_hz = ENG_METAL_FOLLOW_FREQ_MAX_HZ;

    /* ratio is Q24; multiplying by 2^8 gives the Q32 phase numerator.
     * Divide only once to avoid amplifying an earlier base-increment truncation. */
    numerator = (uint64_t)base_freq_hz * ratio * 256u;
    result = (numerator + sample_rate / 2u) / sample_rate;
    return result > UINT32_MAX ? UINT32_MAX : (uint32_t)result;
}

/* Source-aware selector for the later host integration.
 * note_inc is the tracked sequencer increment for sequencer events and the
 * host-resolved pitch increment for performance events.
 * FOLLOW only bypasses step-sequencer pitch; live keyboard/MIDI stays tracked.
 */
static inline uint32_t eng_metal_follow_select_inc(int follow_on,
                                                    int sequencer_source,
                                                    uint32_t base_inc,
                                                    uint32_t note_inc)
{
    return sequencer_source && !follow_on ? base_inc : note_inc;
}

/* Convert Felucca's absolute MIDI note to the reference-copy NOTE / NOTE OCTAVE pair.
 * The host keyboard convention defines MIDI 60 as C4. That note is the zero-offset
 * anchor for the reference-copy's relative FREQ mapping; P_ROOT is a scale setting,
 * not this pitch anchor. Out-of-range octaves are clamped to the helper's -2..+2 range.
 * Returns 0 for invalid MIDI input or null outputs.
 */
static inline int eng_metal_follow_from_midi(uint32_t midi_note,
                                              int32_t *note,
                                              int32_t *octave_tenths)
{
    int32_t relative, octave, semitone;

    if (midi_note > 127u || !note || !octave_tenths)
        return 0;

    relative = (int32_t)midi_note - 60;
    /* Floor division is required so negative MIDI offsets map to NOTE 0..11. */
    octave = relative >= 0 ? relative / 12 : -((-relative + 11) / 12);
    semitone = relative - octave * 12;

    if (octave < ENG_METAL_FOLLOW_OCTAVE_TENTHS_MIN / 10)
        octave = ENG_METAL_FOLLOW_OCTAVE_TENTHS_MIN / 10;
    if (octave > ENG_METAL_FOLLOW_OCTAVE_TENTHS_MAX / 10)
        octave = ENG_METAL_FOLLOW_OCTAVE_TENTHS_MAX / 10;

    *note = semitone;
    *octave_tenths = octave * 10;
    return 1;
}

/* Convert a host pitch increment into a Q24 ratio. Both increments are Q0.32
 * cycles/sample at the same sample rate. This captures pitch bend, tune,
 * glide, LFO, envelope, fine pitch and modulation-matrix effects already
 * applied by Felucca. Zero reference is rejected.
 */
static inline uint32_t eng_metal_follow_inc_ratio_q24(uint32_t reference_inc,
                                                       uint32_t final_inc)
{
    uint64_t numerator, ratio;

    if (!reference_inc)
        return 0u;
    numerator = ((uint64_t)final_inc << 24) + (reference_inc / 2u);
    ratio = numerator / reference_inc;
    return ratio > UINT32_MAX ? UINT32_MAX : (uint32_t)ratio;
}

/* Select the pitch basis while retaining all independent host pitch effects.
 *
 * PERFORMANCE: final host pitch relative to MIDI C4 reference.
 * SEQUENCER + FOLLOW ON: same absolute note mapping relative to C4.
 * SEQUENCER + FOLLOW OFF: final host pitch relative to the target step note,
 *                         so glide/tune/bend/modulation remain active.
 *
 * base_inc is the Metal Core base-FREQ phase increment (Q0.32).
 * note_inc is pitch_inc(v->note * 16); final_inc is the final vmod_t.inc
 * after host glide, tune, bend, LFO, envelope and modulation matrix.
 * All three host increments must use the same sample rate.
 *
 * The result is clamped at Nyquist to prevent phase increments above 0.5
 * cycles/sample from wrapping into an aliased lower frequency.
 */
static inline uint32_t eng_metal_follow_host_phase_inc(uint32_t base_inc,
                                                        uint32_t sample_rate,
                                                        int follow_on,
                                                        int sequencer_source,
                                                        uint32_t reference_inc,
                                                        uint32_t note_inc,
                                                        uint32_t final_inc)
{
    uint32_t denominator, ratio;
    uint64_t scaled, nyquist;

    if (!sample_rate)
        return 0u;

    denominator = (sequencer_source && !follow_on) ? note_inc : reference_inc;
    ratio = eng_metal_follow_inc_ratio_q24(denominator, final_inc);
    if (!ratio)
        return 0u;

    scaled = ((uint64_t)base_inc * ratio + (1ull << 23)) >> 24;
    nyquist = (uint64_t)sample_rate << 31;
    /* Q0.32 phase increment at Nyquist is exactly 0x80000000. */
    if (scaled > 0x80000000ull)
        scaled = 0x80000000ull;
    (void)nyquist;
    return (uint32_t)scaled;
}

#endif
