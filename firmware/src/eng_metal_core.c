/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — Stage 1: isolated main oscillator subsystem.
 *
 * This file is not included by engines.c and is not a registered/playable
 * engine yet. The functions below are fixed-point DSP primitives, developed
 * and tested independently before parameter IDs, UI pages, or engine
 * registration are changed.
 *
 * Integration contract:
 *   - Use Felucca's track_t / voice_t / vmod_t / engine_t and allocator.
 *   - Initial voice mode remains monophonic, selected through existing track
 *     mode; do not change host voice allocation.
 *   - A future render callback must add final voice output to out[], never
 *     overwrite the host mix.
 *   - Stereo remains the host's existing part/mix responsibility.
 *   - Per-part state must fit ENG_MEM_PART; per-voice state uses voice_t slots.
 *   - DSP is fixed-point and bounded to CTL samples per render block.
 *
 * Frozen v98 oscillator roles:
 *   OSC 1 (wcOsc): main carrier.
 *   OSC 2 (wcOsc2): second direct-audio source mixed into the shared pre-fold
 *     input. It is NOT the Complex FM modulator.
 *   wcMod, wcMMod, and wcRingOsc are separate modulation sources and are
 *     intentionally not implemented in this first stage.
 *
 * FOLLOW and METAL are independent switches in the OSCILLATORS / SYNTH VOICE
 * block (not the FOLD page). Their behavior is not implemented in this stage.
 *
 * Parameter IDs and persistence are deliberately not assigned here. The
 * approved scope requires the control map and storage design to be frozen
 * before engine registration.
 */

enum {
    MC_OSC_SINE = 0,
    MC_OSC_TRIANGLE = 1,
    MC_OSC_SAW = 2
};

/* v98 waveform order: SINE / TRI / SAW. Saw uses Felucca's polyBLEP primitive. */
static inline int32_t metal_core_osc_sample(uint32_t phase, uint32_t inc, uint32_t wave)
{
    switch (wave) {
    case MC_OSC_TRIANGLE:
        return osc_tri(phase);
    case MC_OSC_SAW:
        return osc_saw(phase, inc);
    case MC_OSC_SINE:
    default:
        return osc_sine(phase);
    }
}

/* OSC2 has its own cents detune, while inheriting host pitch bend / UNISON
 * fine tuning. At zero detune use the exact host increment without recalculation. */
static inline uint32_t metal_core_osc2_increment(const vmod_t *m, uint32_t detune_cents)
{
    if (!detune_cents)
        return m->inc;
    return cents_inc(m->pitch16, (int32_t)detune_cents, m->fine);
}

/* Render the two audible oscillators into the shared pre-fold signal.
 *
 * n is bounded to CTL. The caller supplies v98 control values; stable
 * parameter IDs and persistence are intentionally outside this subsystem.
 *
 * OSC2 phase advances even at level zero, preserving phase continuity when
 * its retained level is raised later.
 *
 * Output is the raw Q15 oscillator sum (up to approximately +/-65534),
 * before TIMBRE and both wavefolder stages. No voice amplitude, filter,
 * envelope, or final output gain is applied here.
 */
static void metal_core_osc_pair_block(voice_t *v, int32_t *pre_fold, uint32_t n,
                                      const vmod_t *m, uint32_t wave,
                                      uint32_t osc2_detune_cents, int32_t osc2_level_q15)
{
    uint32_t i, ph1, ph2, inc1, inc2;
    int32_t level;

    if (!v || !pre_fold || !m || !n)
        return;
    if (n > CTL)
        n = CTL;

    inc1 = m->inc;
    inc2 = metal_core_osc2_increment(m, osc2_detune_cents);
    level = clamp(osc2_level_q15, 0, 32767);
    ph1 = v->ph[0];
    ph2 = v->ph[1];

    for (i = 0; i < n; i++) {
        int32_t osc1 = metal_core_osc_sample(ph1, inc1, wave);
        int32_t osc2 = metal_core_osc_sample(ph2, inc2, wave);
        pre_fold[i] = osc1 + mulq15(osc2, level);
        ph1 += inc1;
        ph2 += inc2;
    }

    v->ph[0] = ph1;
    v->ph[1] = ph2;
}


/* ------------------------------------------------ Complex Oscillator source (wcMod) --- */

enum {
    MC_MOD_SINE = 0,
    MC_MOD_TRIANGLE = 1,
    MC_MOD_SAW = 2,
    MC_MOD_SQUARE = 3
};

/* wcMod's independent waveform set. Felucca's pulse primitive is half-scale
 * by design; double it here so the Complex Oscillator's SQUARE is full Q15. */
static inline int32_t metal_core_modulator_sample(uint32_t phase, uint32_t inc, uint32_t wave)
{
    switch (wave) {
    case MC_MOD_TRIANGLE:
        return osc_tri(phase);
    case MC_MOD_SAW:
        return osc_saw(phase, inc);
    case MC_MOD_SQUARE:
        return clamp(osc_pulse(phase, inc, 0x80000000u) * 2, -32768, 32767);
    case MC_MOD_SINE:
    default:
        return osc_sine(phase);
    }
}

/* MOD RATIO is 0.25..16.0 in Q16. Compute once per block, not per sample.
 * The phase increment is capped just below Nyquist so an extreme ratio cannot
 * wrap the unsigned phase step and reverse the oscillator direction. */
static inline uint32_t metal_core_ratio_increment(uint32_t base_inc, uint32_t ratio_q16)
{
    uint64_t scaled = ((uint64_t)base_inc * ratio_q16) >> 16;
    return scaled > 0x7FFFFFFFu ? 0x7FFFFFFFu : (uint32_t)scaled;
}

/* Render wcMod into a Q15 scratch block. This stage creates only the
 * independent modulator signal; FM / AM routing, character/index scaling,
 * feedback, and the FM trigger envelope are deliberately separate stages. */
static void metal_core_modulator_block(voice_t *v, int32_t *mod_out, uint32_t n,
                                       const vmod_t *m, uint32_t ratio_q16, uint32_t wave)
{
    uint32_t i, ph, inc;

    if (!v || !mod_out || !m || !n)
        return;
    if (n > CTL)
        n = CTL;

    inc = metal_core_ratio_increment(m->inc, ratio_q16);
    ph = v->ph[2];
    for (i = 0; i < n; i++) {
        mod_out[i] = metal_core_modulator_sample(ph, inc, wave);
        ph += inc;
    }
    v->ph[2] = ph;
}
