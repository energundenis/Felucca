/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated fixed-point VCA envelope.
 *
 * Host-testable sample-domain model. Not registered with Felucca and not
 * connected to an audio render callback.
 *
 * Level: Q16 (unity = 65536; headroom to preserve V98 Accent peak).
 * Coefficient: Q30, generated at trigger time. The sample loop uses one
 * 64-bit multiply and bounded integer operations; no float/division/alloc.
 */
#ifndef ENG_METAL_CORE_VCA_H
#define ENG_METAL_CORE_VCA_H

#include <stdint.h>

#define ENG_METAL_VCA_Q16_ONE       65536
#define ENG_METAL_VCA_Q16_FLOOR     7       /* round(0.0001 * 65536) */
#define ENG_METAL_VCA_Q16_MAX       77332   /* round(1.18 * 65536) */
#define ENG_METAL_VCA_Q30_ONE       1073741824u

#define ENG_METAL_VCA_IDLE          0u
#define ENG_METAL_VCA_ATTACK        1u
#define ENG_METAL_VCA_DECAY         2u
#define ENG_METAL_VCA_SUSTAIN       3u
#define ENG_METAL_VCA_RELEASE       4u

typedef struct {
    int32_t level_q16;
    int32_t target_q16;
    int32_t sustain_q16;
    uint32_t coefficient_q30;
    uint32_t decay_coefficient_q30;
    uint32_t release_coefficient_q30;
    uint32_t gate_remaining;
    uint32_t decay_remaining;
    uint8_t phase;
} eng_metal_vca_t;

/* Rounded conversion helper; call outside the sample loop. */
static inline uint32_t eng_metal_vca_us_to_samples(uint32_t sample_rate,
                                                    uint32_t time_us)
{
    if (!sample_rate || !time_us)
        return 0u;
    return (uint32_t)(((uint64_t)sample_rate * time_us + 500000u) /
                      1000000u);
}

/* First-order discrete approximation to exp(-1/tau_samples).
 * V98's minimum tau is 5 ms, so the 1/tau approximation error is small.
 * Coefficients are prepared once per event, never in process().
 */
static inline uint32_t eng_metal_vca_coefficient(uint32_t tau_samples)
{
    uint32_t coefficient;
    if (!tau_samples)
        tau_samples = 1u;
    coefficient = (uint32_t)((ENG_METAL_VCA_Q30_ONE +
                              (tau_samples / 2u)) / tau_samples);
    if (!coefficient)
        coefficient = 1u;
    if (coefficient > ENG_METAL_VCA_Q30_ONE)
        coefficient = ENG_METAL_VCA_Q30_ONE;
    return coefficient;
}

static inline int32_t eng_metal_vca_clamp_q16(int32_t value)
{
    if (value < 0) return 0;
    if (value > ENG_METAL_VCA_Q16_ONE) return ENG_METAL_VCA_Q16_ONE;
    return value;
}

static inline int32_t eng_metal_vca_mul_q16(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b + 32768) >> 16);
}

/* V98 target calculation. A missing velocity is represented by has_velocity=0. */
static inline void eng_metal_vca_targets_q16(int32_t level_q16,
                                             int32_t sustain_q16,
                                             int32_t velocity_q16,
                                             int has_velocity,
                                             int accent,
                                             int32_t *peak_q16,
                                             int32_t *sustain_target_q16)
{
    int32_t level = eng_metal_vca_clamp_q16(level_q16);
    int32_t sustain = eng_metal_vca_clamp_q16(sustain_q16);
    int32_t velocity = has_velocity ? velocity_q16 : 52429; /* 0.8 */
    int32_t peak, sustain_target;

    if (velocity < 6554) velocity = 6554; /* 0.1 */
    if (velocity > ENG_METAL_VCA_Q16_ONE)
        velocity = ENG_METAL_VCA_Q16_ONE;

    peak = eng_metal_vca_mul_q16(level, velocity);
    sustain_target = eng_metal_vca_mul_q16(peak, sustain);
    if (accent) {
        peak = eng_metal_vca_mul_q16(peak, 77332); /* 1.18 */
        sustain_target = eng_metal_vca_mul_q16(sustain_target, 70779); /* 1.08 */
    }
    if (sustain_target < ENG_METAL_VCA_Q16_FLOOR)
        sustain_target = ENG_METAL_VCA_Q16_FLOOR;
    if (peak > ENG_METAL_VCA_Q16_MAX)
        peak = ENG_METAL_VCA_Q16_MAX;

    if (peak_q16) *peak_q16 = peak;
    if (sustain_target_q16) *sustain_target_q16 = sustain_target;
}

static inline void eng_metal_vca_reset(eng_metal_vca_t *vca)
{
    if (!vca) return;
    vca->level_q16 = 0;
    vca->target_q16 = 0;
    vca->sustain_q16 = ENG_METAL_VCA_Q16_FLOOR;
    vca->coefficient_q30 = 0;
    vca->decay_coefficient_q30 = 0;
    vca->release_coefficient_q30 = 0;
    vca->gate_remaining = 0;
    vca->decay_remaining = 0;
    vca->phase = ENG_METAL_VCA_IDLE;
}

/* Start/retrigger a V98-style contour.
 * decay_start_samples is max(5 ms, effective Attack), measured from trigger.
 * Retrigger deliberately preserves level_q16 (cancel-and-hold).
 */
static inline void eng_metal_vca_trigger(eng_metal_vca_t *vca,
                                          int32_t peak_q16,
                                          int32_t sustain_q16,
                                          uint32_t attack_tau_samples,
                                          uint32_t decay_start_samples,
                                          uint32_t decay_tau_samples,
                                          uint32_t gate_samples,
                                          uint32_t release_tau_samples)
{
    if (!vca) return;
    if (peak_q16 < 0) peak_q16 = 0;
    if (peak_q16 > ENG_METAL_VCA_Q16_MAX)
        peak_q16 = ENG_METAL_VCA_Q16_MAX;
    if (sustain_q16 < ENG_METAL_VCA_Q16_FLOOR)
        sustain_q16 = ENG_METAL_VCA_Q16_FLOOR;
    if (sustain_q16 > ENG_METAL_VCA_Q16_MAX)
        sustain_q16 = ENG_METAL_VCA_Q16_MAX;

    vca->target_q16 = peak_q16;
    vca->sustain_q16 = sustain_q16;
    vca->coefficient_q30 = eng_metal_vca_coefficient(attack_tau_samples);
    vca->decay_coefficient_q30 = eng_metal_vca_coefficient(decay_tau_samples);
    vca->release_coefficient_q30 = eng_metal_vca_coefficient(release_tau_samples);
    vca->gate_remaining = gate_samples;
    vca->decay_remaining = decay_start_samples;
    vca->phase = ENG_METAL_VCA_ATTACK;
}

/* Explicit host Gate-off event (MIDI Note Off or sequencer step end).
 * Safe to call once at a render-block boundary; repeated calls during Release
 * do not restart or alter the Release trajectory. The next process() advances
 * the first Release sample from the instantaneous held level.
 */
static inline void eng_metal_vca_gate_off(eng_metal_vca_t *vca)
{
    if (!vca || vca->phase == ENG_METAL_VCA_IDLE ||
        vca->phase == ENG_METAL_VCA_RELEASE)
        return;
    vca->gate_remaining = 0u;
    vca->target_q16 = ENG_METAL_VCA_Q16_FLOOR;
    vca->coefficient_q30 = vca->release_coefficient_q30;
    vca->phase = ENG_METAL_VCA_RELEASE;
}

/* One sample. Gate-off is evaluated before Decay, so a coincident or later
 * Decay event can never reopen the envelope after Release has started.
 */
static inline int32_t eng_metal_vca_process(eng_metal_vca_t *vca)
{
    int32_t delta;
    uint32_t magnitude, step;
    uint64_t product;

    if (!vca || vca->phase == ENG_METAL_VCA_IDLE)
        return 0;

    /* CP1.16 Option B: Gate-off has priority over a pending Decay event. */
    if (vca->phase != ENG_METAL_VCA_RELEASE &&
        vca->gate_remaining == 0u) {
        vca->target_q16 = ENG_METAL_VCA_Q16_FLOOR;
        vca->coefficient_q30 = vca->release_coefficient_q30;
        vca->phase = ENG_METAL_VCA_RELEASE;
    }

    if (vca->phase == ENG_METAL_VCA_ATTACK &&
        vca->decay_remaining == 0u) {
        vca->target_q16 = vca->sustain_q16;
        vca->coefficient_q30 = vca->decay_coefficient_q30;
        vca->phase = ENG_METAL_VCA_DECAY;
    }

    delta = vca->target_q16 - vca->level_q16;
    if (delta) {
        magnitude = (uint32_t)(delta < 0 ? -delta : delta);
        product = (uint64_t)magnitude * vca->coefficient_q30;
        step = (uint32_t)((product + (1ull << 29)) >> 30);
        if (!step) step = 1u;
        if (step >= magnitude)
            vca->level_q16 = vca->target_q16;
        else if (delta > 0)
            vca->level_q16 += (int32_t)step;
        else
            vca->level_q16 -= (int32_t)step;
    }

    if (vca->phase == ENG_METAL_VCA_DECAY &&
        vca->level_q16 == vca->target_q16) {
        vca->phase = ENG_METAL_VCA_SUSTAIN;
    } else if (vca->phase == ENG_METAL_VCA_RELEASE &&
               vca->level_q16 <= ENG_METAL_VCA_Q16_FLOOR) {
        vca->level_q16 = 0;
        vca->target_q16 = 0;
        vca->phase = ENG_METAL_VCA_IDLE;
    }

    if (vca->phase != ENG_METAL_VCA_RELEASE) {
        if (vca->gate_remaining)
            --vca->gate_remaining;
        if (vca->phase == ENG_METAL_VCA_ATTACK &&
            vca->decay_remaining)
            --vca->decay_remaining;
    }

    return vca->level_q16;
}

static inline int eng_metal_vca_done(const eng_metal_vca_t *vca)
{
    return !vca || vca->phase == ENG_METAL_VCA_IDLE;
}

#endif
