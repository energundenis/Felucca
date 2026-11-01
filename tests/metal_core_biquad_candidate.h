/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — host-only fixed-point DF2T biquad candidate.
 * Not production DSP; coefficient generation and state ABI are provisional.
 */
#ifndef TEST_METAL_CORE_BIQUAD_CANDIDATE_H
#define TEST_METAL_CORE_BIQUAD_CANDIDATE_H

#include <stdint.h>
#include <limits.h>

/* Q2.30 spans [-2, 2), enough for normalized a1 near -2. */
#define TEST_METAL_BIQUAD_Q30_ONE INT64_C(1073741824)
#define TEST_METAL_BIQUAD_Q30_HALF INT64_C(536870912)

typedef struct {
    int32_t b0, b1, b2;
    int32_t a1, a2;
} test_metal_biquad_coeff_q30_t;

typedef struct {
    int32_t z1, z2;
} test_metal_biquad_state_t;

/* Signed round-to-nearest, ties away from zero, without 64-bit division.
 * product magnitude is bounded by 2^62 for int32 x int32, so negation and
 * the half-unit bias remain inside int64_t. The shifts operate on positive
 * values only; no implementation-defined negative right shift is used.
 */
static inline int64_t test_metal_biquad_mul_q30(int32_t coefficient_q30,
                                                 int32_t sample)
{
    int64_t product = (int64_t)coefficient_q30 * sample;
    if (product >= 0)
        return (product + TEST_METAL_BIQUAD_Q30_HALF) >> 30;
    return -(((-product) + TEST_METAL_BIQUAD_Q30_HALF) >> 30);
}

static inline int32_t test_metal_biquad_sat32(int64_t value,
                                               uint32_t *saturation_count)
{
    if (value > INT32_MAX) {
        if (saturation_count) ++*saturation_count;
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        if (saturation_count) ++*saturation_count;
        return INT32_MIN;
    }
    return (int32_t)value;
}

/* Direct Form II Transposed, one mono channel, int32 signal/state scale. */
static inline int32_t test_metal_biquad_process_q30(
    const test_metal_biquad_coeff_q30_t *c,
    test_metal_biquad_state_t *s,
    int32_t input,
    uint32_t *saturation_count)
{
    int32_t output;
    int64_t y, z1, z2;

    if (!c || !s)
        return 0;

    y = test_metal_biquad_mul_q30(c->b0, input) + s->z1;
    output = test_metal_biquad_sat32(y, saturation_count);

    z1 = test_metal_biquad_mul_q30(c->b1, input) -
         test_metal_biquad_mul_q30(c->a1, output) + s->z2;
    z2 = test_metal_biquad_mul_q30(c->b2, input) -
         test_metal_biquad_mul_q30(c->a2, output);

    s->z1 = test_metal_biquad_sat32(z1, saturation_count);
    s->z2 = test_metal_biquad_sat32(z2, saturation_count);
    return output;
}

static inline void test_metal_biquad_reset(test_metal_biquad_state_t *s)
{
    if (!s) return;
    s->z1 = 0;
    s->z2 = 0;
}

#endif
