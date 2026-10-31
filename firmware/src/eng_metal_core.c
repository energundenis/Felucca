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

    if (ratio_q16 < (1u << 14))
        ratio_q16 = 1u << 14;
    else if (ratio_q16 > (16u << 16))
        ratio_q16 = 16u << 16;
    inc = metal_core_ratio_increment(m->inc, ratio_q16);
    ph = v->ph[2];
    for (i = 0; i < n; i++) {
        mod_out[i] = metal_core_modulator_sample(ph, inc, wave);
        ph += inc;
    }
    v->ph[2] = ph;
}


/* ------------------------------------------------ Complex FM character / drive curves --- */

enum {
    MC_FM_CHAR_SMOOTH = 0,
    MC_FM_CHAR_CLASSIC = 1,
    MC_FM_CHAR_METALLIC = 2
};

/* Bounded integer square root. Used only at control/block rate, never in the
 * per-sample loop. Input and result are unsigned Q15 values. */
static inline uint32_t metal_core_isqrt_u32(uint32_t x)
{
    uint32_t op = x, res = 0, one = 1u << 30;
    while (one > op)
        one >>= 2;
    while (one) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return res;
}

static inline int32_t metal_core_sqrt_q15(int32_t x)
{
    x = clamp(x, 0, 32767);
    return (int32_t)metal_core_isqrt_u32((uint32_t)x << 15);
}

/* v98 character curves, returned as extended Q15 (METALLIC reaches 1.70):
 *   SMOOTH   = 0.72 * sqrt(index)
 *   CLASSIC  = index
 *   METALLIC = 0.55 * index + 1.15 * index^2
 */
static inline int32_t metal_core_fm_character_q15(int32_t index_q15, uint32_t character)
{
    int32_t index = clamp(index_q15, 0, 32767);
    switch (character) {
    case MC_FM_CHAR_SMOOTH:
        return mulq15(23593, metal_core_sqrt_q15(index));   /* 0.72 */
    case MC_FM_CHAR_CLASSIC:
        return index;
    case MC_FM_CHAR_METALLIC:
    default:
        return mulq15(18022, index) + mulq15(37683, mulq15(index, index)); /* 0.55 + 1.15 x^2 */
    }
}

/* wcDrv() = WEST COAST DRIVE ^ 1.5. The result is Q15 and remains separate
 * from the WAVEFOLDER DRIVE control. */
static inline int32_t metal_core_west_coast_drive_q15(int32_t drive_q15)
{
    int32_t drive = clamp(drive_q15, 0, 32767);
    return mulq15(drive, metal_core_sqrt_q15(drive));
}

/* Effective Complex FM depth before the v98 FREQ x 2 deviation term. The
 * result is extended Q15 because METALLIC can exceed unity. */
static inline int32_t metal_core_complex_fm_depth_q15(int32_t index_q15,
                                                       int32_t drive_q15,
                                                       uint32_t character)
{
    int32_t curve = metal_core_fm_character_q15(index_q15, character);
    int32_t drive = metal_core_west_coast_drive_q15(drive_q15);
    return mulq15(curve, drive);
}


/* ------------------------------------------------ Audio-rate Complex FM carrier --- */

/* v98 applies FM by changing OSC1's instantaneous frequency. The signed
 * phase step allows the modulator to pull the carrier through zero and
 * reverse its direction. The magnitude is bounded below Nyquist so the
 * polyBLEP saw remains inside its valid increment range. */
static inline int32_t metal_core_fm_instantaneous_step(uint32_t base_inc,
                                                        uint64_t deviation_inc,
                                                        int32_t mod_sample_q15)
{
    int64_t deviation = ((int64_t)deviation_inc * mod_sample_q15) >> 15;
    int64_t step = (int64_t)base_inc + deviation;
    if (step > 0x7FFFFFFFLL)
        step = 0x7FFFFFFFLL;
    else if (step < -0x7FFFFFFFLL)
        step = -0x7FFFFFFFLL;
    return (int32_t)step;
}

/* Signed-frequency form of the main oscillator sampler. Waveform value is
 * still a function of phase; only the BLEP width uses the absolute step. */
static inline int32_t metal_core_osc_sample_signed(uint32_t phase, int32_t step, uint32_t wave)
{
    uint32_t inc = step < 0 ? (uint32_t)(-(int64_t)step) : (uint32_t)step;
    if (step >= 0 || wave != MC_OSC_SAW)
        return metal_core_osc_sample(phase, inc, wave);

    /* Reverse traversal needs a time-reversed BLEP correction. Reusing the
     * forward correction at |step| creates an asymmetric discontinuity when
     * FM pulls the instantaneous carrier frequency through zero. */
    return ((int32_t)(phase >> 16) - 32768) +
           blep(0xFFFFFFFFu - phase, inc);
}

/* Full v98 Complex FM -> OSC1 path plus the independent OSC2 direct-audio
 * source. This stage intentionally holds FM feedback at zero and does not
 * apply the FM trigger envelope; those are separate follow-up stages.
 *
 * All increments are supplied by the caller so FOLLOW/base-FREQ selection
 * and OSC2 cents conversion can be audited independently before engine
 * parameter IDs are assigned.
 */
static void metal_core_complex_fm_pair_block(voice_t *v, int32_t *pre_fold, uint32_t n,
                                             uint32_t osc1_inc, uint32_t osc2_inc,
                                             uint32_t mod_ratio_q16, uint32_t osc_wave,
                                             uint32_t mod_wave, int32_t fm_index_q15,
                                             int32_t west_coast_drive_q15, uint32_t character,
                                             int32_t osc2_level_q15)
{
    uint32_t i, ph1, ph2, phm, mod_inc;
    int32_t level, depth;
    uint64_t deviation_inc;

    if (!v || !pre_fold || !n)
        return;
    if (n > CTL)
        n = CTL;
    if (mod_ratio_q16 < (1u << 14))
        mod_ratio_q16 = 1u << 14;
    else if (mod_ratio_q16 > (16u << 16))
        mod_ratio_q16 = 16u << 16;

    mod_inc = metal_core_ratio_increment(osc1_inc, mod_ratio_q16);
    level = clamp(osc2_level_q15, 0, 32767);
    depth = metal_core_complex_fm_depth_q15(fm_index_q15, west_coast_drive_q15, character);
    deviation_inc = ((uint64_t)osc1_inc * 2u * (uint32_t)depth) >> 15;

    ph1 = v->ph[0];
    ph2 = v->ph[1];
    phm = v->ph[2];
    for (i = 0; i < n; i++) {
        int32_t mod = metal_core_modulator_sample(phm, mod_inc, mod_wave);
        int32_t step = metal_core_fm_instantaneous_step(osc1_inc, deviation_inc, mod);
        int32_t osc1 = metal_core_osc_sample_signed(ph1, step, osc_wave);
        int32_t osc2 = metal_core_osc_sample(ph2, osc2_inc, osc_wave);
        pre_fold[i] = osc1 + mulq15(osc2, level);
        ph1 += (uint32_t)step;
        ph2 += osc2_inc;
        phm += mod_inc;
    }
    v->ph[0] = ph1;
    v->ph[1] = ph2;
    v->ph[2] = phm;
}


/* ------------------------------------------------ 3 ms per-voice FM feedback delay --- */

/* The reference path uses a 3 ms wcMod feedback delay. Use the ceiling
 * sample count so the ring never undershoots 3 ms at the selected build rate.
 * Felucca-Engines supports up to 96 kHz for this memory layout. */
#define MC_FEEDBACK_DELAY_MS       3u
#define MC_FEEDBACK_DELAY_SAMPLES  ((FS * MC_FEEDBACK_DELAY_MS + 999u) / 1000u)

typedef struct {
    uint16_t write[NVOICE];
    int32_t sample[NVOICE][MC_FEEDBACK_DELAY_SAMPLES];
} metal_core_feedback_state_t;

/* The 96 kHz layout is 9,232 bytes for eight voices, below Felucca's
 * 12,888-byte per-part engine-memory budget. Deliberately fail compilation
 * above that supported memory envelope rather than silently overflowing it. */
typedef char metal_core_feedback_memory_fits[
    (sizeof(metal_core_feedback_state_t) <= 12888u) ? 1 : -1];

/* Reset one voice's delay history on a fresh voice start. The caller must
 * decide whether a legato retrigger should preserve or clear this state. */
static void metal_core_feedback_delay_reset(metal_core_feedback_state_t *state, uint32_t voice)
{
    uint32_t i;
    if (!state || voice >= NVOICE)
        return;
    state->write[voice] = 0;
    for (i = 0; i < MC_FEEDBACK_DELAY_SAMPLES; i++)
        state->sample[voice][i] = 0;
}

/* One-sample delay primitive. Returns the oldest sample, stores the new input,
 * and advances the ring. Feedback gain/routing remain external so this helper
 * cannot accidentally introduce an unbounded feedback loop by itself. */
static inline int32_t metal_core_feedback_delay_tick(metal_core_feedback_state_t *state,
                                                      uint32_t voice, int32_t input)
{
    uint32_t pos;
    int32_t delayed;
    if (!state || voice >= NVOICE)
        return 0;
    pos = state->write[voice];
    delayed = state->sample[voice][pos];
    state->sample[voice][pos] = input;
    pos++;
    if (pos >= MC_FEEDBACK_DELAY_SAMPLES)
        pos = 0;
    state->write[voice] = (uint16_t)pos;
    return delayed;
}
