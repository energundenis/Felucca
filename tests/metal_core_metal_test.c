/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — Metal ratio and ring modulation host tests.
 */
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/eng_metal_core_metal.h"

static int failures;
static void check(const char *name, int ok)
{
    printf("metal ring: %-70s %s\n", name, ok ? "PASS" : "FAIL");
    failures += !ok;
}

static void test_ratio(void)
{
    uint32_t inc = 0x01000000u;
    uint32_t got = eng_metal_ratio_increment(inc, ENG_METAL_RATIO_Q16_4_2);
    check("reference METAL ratio 4.20 maps to Q16 within 1 LSB",
          ENG_METAL_RATIO_Q16_4_2 == 275251u);
    check("ratio 1.0 preserves the phase increment",
          eng_metal_ratio_increment(inc, 65536u) == inc);
    check("ratio 4.20 scales increment by approximately 4.2",
          got >= 0x04333300u && got <= 0x04333400u);
    check("ratio is clamped at 16.0 before scaling",
          eng_metal_ratio_increment(inc, UINT32_MAX) ==
          eng_metal_ratio_increment(inc, ENG_METAL_RATIO_Q16_MAX));
}

static void test_ring(void)
{
    int32_t x = 12000;
    check("disabled METAL is bit-exact bypass",
          eng_metal_ring_apply_q15(x, -32768, 0u) == x);
    check("positive full-scale modulator preserves carrier level",
          eng_metal_ring_apply_q15(x, 32767, 1u) == 12000);
    check("negative full-scale modulator inverts carrier",
          eng_metal_ring_apply_q15(x, -32768, 1u) == -12000);
    check("zero modulator produces silence",
          eng_metal_ring_apply_q15(x, 0, 1u) == 0);
    check("zero carrier remains zero",
          eng_metal_ring_apply_q15(0, -32768, 1u) == 0);
    check("positive/negative product rounding is symmetric",
          eng_metal_ring_apply_q15(12345, -23456, 1u) ==
          -eng_metal_ring_apply_q15(12345, 23456, 1u));
}

static void test_bounds(void)
{
    int32_t sample, mod;
    int ok = 1;
    for (sample = -32768; sample <= 32767; sample += 257) {
        for (mod = -32768; mod <= 32767; mod += 509) {
            int32_t y = eng_metal_ring_apply_q15(sample, mod, 1u);
            if (y < -32768 || y > 32767)
                ok = 0;
        }
    }
    check("ring modulation stays bounded across deterministic Q15 grid", ok);
}

int main(void)
{
    test_ratio();
    test_ring();
    test_bounds();
    printf("metal ring: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
