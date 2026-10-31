/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated LPG peak math tests (host-only).
 *
 * Tests the selected Option C transfer function, not an engine render.
 */
#include "../firmware/src/eng_metal_core_math.h"

#include <stdio.h>
#include <stdint.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal LPG: %-64s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

int main(void)
{
    int32_t p0 = eng_metal_lpg_peak_q15(0, 32767);
    int32_t p1 = eng_metal_lpg_peak_q15(32767, 32767);
    int32_t pm = eng_metal_lpg_peak_q15(16384, 32767);
    int32_t half = eng_metal_lpg_peak_q15(16384, 16384);
    int32_t zero = eng_metal_lpg_peak_q15(16384, 0);
    int32_t below = eng_metal_lpg_peak_q15(-100, 32767);
    int32_t above = eng_metal_lpg_peak_q15(40000, 40000);
    int32_t v25 = eng_metal_lpg_peak_q15(16384, 8192);
    int32_t v50 = eng_metal_lpg_peak_q15(16384, 16384);
    int32_t v75 = eng_metal_lpg_peak_q15(16384, 24575);

    check("RESPONSE=0 preserves 0.25 base peak at full module volume",
          p0 >= 8191 && p0 <= 8193);
    check("RESPONSE=1 reaches full-scale peak at full module volume",
          p1 == 32767);
    check("RESPONSE midpoint lies between the response endpoints",
          pm > p0 && pm < p1);
    check("MODULE VOLUME=0 mutes the LPG peak",
          zero == 0);
    check("MODULE VOLUME=0.5 halves the computed envelope peak",
          half >= pm / 2 - 1 && half <= pm / 2 + 1);
    check("negative and over-range inputs are safely clamped",
          below == p0 && above == 32767);
    check("module volume is monotonic across 25/50/75 percent",
          v25 < v50 && v50 < v75);
    check("module volume does not alter the RESPONSE contour ordering",
          eng_metal_lpg_peak_q15(0, 16384) <
          eng_metal_lpg_peak_q15(16384, 16384) &&
          eng_metal_lpg_peak_q15(16384, 16384) <
          eng_metal_lpg_peak_q15(32767, 16384));

    printf("metal LPG: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
