/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — host-only fractional feedback delay candidate.
 *
 * This file is a test candidate, not production DSP and not included by the
 * firmware engine. audio.c sets DAC_TICKS=544 at 24 MHz, giving 44,117.647 Hz;
 * 3 ms = 132.35294 samples. Read-before-write uses a 133-sample ring.
 */
#ifndef TEST_METAL_CORE_FEEDBACK_DELAY_CANDIDATE_H
#define TEST_METAL_CORE_FEEDBACK_DELAY_CANDIDATE_H

#include <stdint.h>

#define TEST_METAL_FB_RING_SAMPLES 133u
#define TEST_METAL_FB_FRAC_Q15 11565u /* round((132.35294 - 132) * 32768) */

typedef struct {
    int32_t sample[TEST_METAL_FB_RING_SAMPLES];
    uint16_t write_index;
} test_metal_feedback_delay_t;

static inline void test_metal_feedback_delay_reset(test_metal_feedback_delay_t *d)
{
    uint32_t i;
    if (!d) return;
    for (i = 0; i < TEST_METAL_FB_RING_SAMPLES; ++i)
        d->sample[i] = 0;
    d->write_index = 0;
}

/* Read the 132.35294-sample delayed value before writing the current sample. */
static inline int32_t test_metal_feedback_delay_read(
    const test_metal_feedback_delay_t *d)
{
    uint32_t near_index;
    int32_t near_sample, far_sample;
    int64_t mixed;

    if (!d || d->write_index >= TEST_METAL_FB_RING_SAMPLES)
        return 0;

    near_index = d->write_index >= 132u
        ? (uint32_t)d->write_index - 132u
        : (uint32_t)d->write_index + 1u;
    near_sample = d->sample[near_index];
    far_sample = d->sample[d->write_index];
    mixed = (int64_t)near_sample * (32768u - TEST_METAL_FB_FRAC_Q15) +
            (int64_t)far_sample * TEST_METAL_FB_FRAC_Q15;
    mixed += mixed >= 0 ? 16384 : -16384;
    return (int32_t)(mixed >> 15);
}

static inline void test_metal_feedback_delay_write(
    test_metal_feedback_delay_t *d, int32_t input)
{
    if (!d || d->write_index >= TEST_METAL_FB_RING_SAMPLES)
        return;
    d->sample[d->write_index] = input;
    ++d->write_index;
    if (d->write_index == TEST_METAL_FB_RING_SAMPLES)
        d->write_index = 0;
}

#endif
