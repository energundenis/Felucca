/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — host-only fixed-point biquad candidate tests.
 * The double model is an oracle only; it is never part of target DSP.
 */
#include "metal_core_biquad_candidate.h"

#include <math.h>
#include <stdio.h>

#define FS 44117.64705882353
#define PI 3.14159265358979323846
#define Q30_SCALE 1073741824.0

static int failures;
static unsigned checks;

static void check(const char *name, int ok)
{
    ++checks;
    printf("metal biquad: %-66s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static int32_t q30(double value)
{
    return (int32_t)llround(value * Q30_SCALE);
}

/* Host-only RBJ low-pass coefficient generator and double-precision oracle. */
static int make_lowpass(double cutoff_hz, double q,
                        test_metal_biquad_coeff_q30_t *out,
                        double oracle[5])
{
    double w0, cs, sn, alpha, a0;
    double b0, b1, b2, a1, a2;
    if (!out || !oracle || !(cutoff_hz > 0.0) ||
        !(cutoff_hz < FS * 0.5) || !(q >= 0.5) || q > 20.0)
        return 0;
    w0 = 2.0 * PI * cutoff_hz / FS;
    cs = cos(w0);
    sn = sin(w0);
    alpha = sn / (2.0 * q);
    a0 = 1.0 + alpha;
    b0 = ((1.0 - cs) * 0.5) / a0;
    b1 = (1.0 - cs) / a0;
    b2 = b0;
    a1 = (-2.0 * cs) / a0;
    a2 = (1.0 - alpha) / a0;
    oracle[0] = b0; oracle[1] = b1; oracle[2] = b2;
    oracle[3] = a1; oracle[4] = a2;
    out->b0 = q30(b0); out->b1 = q30(b1); out->b2 = q30(b2);
    out->a1 = q30(a1); out->a2 = q30(a2);
    return 1;
}

static double oracle_process(const double c[5], double *z1, double *z2,
                             double input)
{
    double y = c[0] * input + *z1;
    double next_z1 = c[1] * input - c[3] * y + *z2;
    double next_z2 = c[2] * input - c[4] * y;
    *z1 = next_z1;
    *z2 = next_z2;
    return y;
}

static int run_impulse_case(double cutoff, double q, double *max_error_out)
{
    test_metal_biquad_coeff_q30_t c;
    test_metal_biquad_state_t s = {0, 0};
    double oc[5], oz1 = 0.0, oz2 = 0.0, max_error = 0.0;
    uint32_t saturation_count = 0;
    int32_t tail_peak = 0;
    int i;
    if (!make_lowpass(cutoff, q, &c, oc)) return 0;
    for (i = 0; i < 65536; ++i) {
        int32_t x = i == 0 ? 5000000 : 0;
        int32_t y = test_metal_biquad_process_q30(&c, &s, x,
                                                  &saturation_count);
        double oy = oracle_process(oc, &oz1, &oz2, (double)x);
        double error = fabs((double)y - oy);
        int32_t magnitude = y < 0 ? (y == INT32_MIN ? INT32_MAX : -y) : y;
        if (error > max_error) max_error = error;
        if (i >= 63488 && magnitude > tail_peak) tail_peak = magnitude;
        if (s.z1 == INT32_MAX || s.z1 == INT32_MIN ||
            s.z2 == INT32_MAX || s.z2 == INT32_MIN)
            return 0;
    }
    if (max_error_out) *max_error_out = max_error;
    return saturation_count == 0 && tail_peak < 1024 &&
           fabs(oz1) < 1.0 && fabs(oz2) < 1.0;
}

static int run_sine_case(double cutoff, double q, double test_hz,
                         double *max_error_out)
{
    test_metal_biquad_coeff_q30_t c;
    test_metal_biquad_state_t s = {0, 0};
    double oc[5], oz1 = 0.0, oz2 = 0.0, max_error = 0.0;
    uint32_t saturation_count = 0;
    int i;
    if (!make_lowpass(cutoff, q, &c, oc)) return 0;
    for (i = 0; i < 32768; ++i) {
        int32_t x = (int32_t)llround(5000000.0 *
                                     sin(2.0 * PI * test_hz * i / FS));
        int32_t y = test_metal_biquad_process_q30(&c, &s, x,
                                                  &saturation_count);
        double oy = oracle_process(oc, &oz1, &oz2, (double)x);
        double error = fabs((double)y - oy);
        if (error > max_error) max_error = error;
    }
    if (max_error_out) *max_error_out = max_error;
    return saturation_count == 0;
}

static int coefficients_stable(const test_metal_biquad_coeff_q30_t *c)
{
    double a1 = (double)c->a1 / Q30_SCALE;
    double a2 = (double)c->a2 / Q30_SCALE;
    return fabs(a2) < 1.0 && (1.0 + a1 + a2) > 0.0 &&
           (1.0 - a1 + a2) > 0.0;
}

static int64_t reference_mul_q30(int32_t coefficient, int32_t sample)
{
    int64_t product = (int64_t)coefficient * sample;
    if (product >= 0)
        return (product + TEST_METAL_BIQUAD_Q30_HALF) /
               TEST_METAL_BIQUAD_Q30_ONE;
    return (product - TEST_METAL_BIQUAD_Q30_HALF) /
           TEST_METAL_BIQUAD_Q30_ONE;
}

static void test_q30_rounding_equivalence(void)
{
    static const int32_t edge[] = {
        INT32_MIN, INT32_MIN + 1, -1073741824, -1, 0, 1,
        1073741823, INT32_MAX
    };
    uint32_t state = 0x6D2B79F5u;
    unsigned i, j;
    int equivalent = 1;

    for (i = 0; i < sizeof(edge) / sizeof(edge[0]); ++i) {
        for (j = 0; j < sizeof(edge) / sizeof(edge[0]); ++j) {
            int64_t expected = reference_mul_q30(edge[i], edge[j]);
            int64_t actual = test_metal_biquad_mul_q30(edge[i], edge[j]);
            if (actual != expected)
                equivalent = 0;
        }
    }

    for (i = 0; i < 20000u; ++i) {
        int32_t coefficient, sample;
        int64_t expected, actual;
        state = state * 1664525u + 1013904223u;
        coefficient = (int32_t)((int64_t)state - 2147483648LL);
        state = state * 1664525u + 1013904223u;
        sample = (int32_t)((int64_t)state - 2147483648LL);
        expected = reference_mul_q30(coefficient, sample);
        actual = test_metal_biquad_mul_q30(coefficient, sample);
        if (actual != expected)
            equivalent = 0;
    }

    check("Q30 rounded-shift multiply matches division oracle on edge and 20,000 deterministic vectors",
          equivalent);
}

int main(void)
{
    test_q30_rounding_equivalence();
    static const double cutoffs[] = {100.0, 1200.0, 8000.0, 12000.0};
    static const double qs[] = {0.5, 0.7, 1.0, 2.0, 5.0, 10.0, 20.0};
    size_t ci, qi;
    double worst_impulse = 0.0, worst_sine = 0.0;
    int all_impulses = 1, all_sines = 1;

    check("candidate coefficient/state layouts are explicit and separate",
          sizeof(test_metal_biquad_coeff_q30_t) == 20 &&
          sizeof(test_metal_biquad_state_t) == 8);
    {
        int stable = 1;
        for (ci = 0; ci < sizeof(cutoffs)/sizeof(cutoffs[0]); ++ci) {
            for (qi = 0; qi < sizeof(qs)/sizeof(qs[0]); ++qi) {
                test_metal_biquad_coeff_q30_t c;
                double oracle[5];
                if (cutoffs[ci] < FS * 0.5) {
                    if (!make_lowpass(cutoffs[ci], qs[qi], &c, oracle)) {
                        stable = 0;
                        continue;
                    }
                    stable &= coefficients_stable(&c);
                }
            }
        }
        check("all 28 quantized denominator coefficient sets pass Jury stability", stable);
    }

    for (ci = 0; ci < sizeof(cutoffs)/sizeof(cutoffs[0]); ++ci) {
        if (cutoffs[ci] >= FS * 0.5) continue;
        for (qi = 0; qi < sizeof(qs)/sizeof(qs[0]); ++qi) {
            double e = 0.0;
            int ok = run_impulse_case(cutoffs[ci], qs[qi], &e);
            if (e > worst_impulse) worst_impulse = e;
            all_impulses &= ok;
        }
    }
    check("28 impulse responses remain bounded, decay to a sub-1024-LSB tail, and do not saturate",
          all_impulses);
    check("worst impulse error versus double oracle is below 0.15% of input amplitude",
          worst_impulse < 7500.0);

    for (ci = 0; ci < sizeof(cutoffs)/sizeof(cutoffs[0]); ++ci) {
        double hz[3];
        if (cutoffs[ci] >= FS * 0.5) continue;
        hz[0] = fmax(20.0, cutoffs[ci] * 0.25);
        hz[1] = cutoffs[ci];
        hz[2] = fmin(FS * 0.45, cutoffs[ci] * 2.0);
        for (qi = 0; qi < sizeof(qs)/sizeof(qs[0]); ++qi) {
            size_t hi;
            for (hi = 0; hi < 3; ++hi) {
                double e = 0.0;
                int ok = run_sine_case(cutoffs[ci], qs[qi], hz[hi], &e);
                if (e > worst_sine) worst_sine = e;
                all_sines &= ok;
            }
        }
    }
    check("84 sine sweeps remain bounded and do not saturate", all_sines);
    check("worst sine error is below 0.15% of input amplitude",
          worst_sine < 7500.0);

    {
        test_metal_biquad_coeff_q30_t main_c, lpg_c;
        test_metal_biquad_state_t main_s = {0, 0}, lpg_s = {0, 0};
        test_metal_biquad_state_t lpg_before;
        double main_oracle[5], lpg_oracle[5];
        uint32_t main_sat = 0, lpg_sat = 0;
        int n;
        make_lowpass(1200.0, 2.0, &main_c, main_oracle);
        make_lowpass(1800.0, 0.7, &lpg_c, lpg_oracle);
        for (n = 0; n < 256; ++n) {
            test_metal_biquad_process_q30(&main_c, &main_s,
                                           n == 0 ? 1000000 : 0, &main_sat);
            test_metal_biquad_process_q30(&lpg_c, &lpg_s,
                                           n == 0 ? 700000 : 0, &lpg_sat);
        }
        lpg_before = lpg_s;
        test_metal_biquad_reset(&main_s);
        check("MAIN and LPG filter histories remain independent",
              main_s.z1 == 0 && main_s.z2 == 0 &&
              lpg_s.z1 == lpg_before.z1 && lpg_s.z2 == lpg_before.z2 &&
              (lpg_s.z1 != 0 || lpg_s.z2 != 0) &&
              main_sat == 0 && lpg_sat == 0);
    }
    {
        test_metal_biquad_coeff_q30_t c;
        test_metal_biquad_state_t s = {0, 0};
        double oracle[5];
        uint32_t sat = 0;
        int32_t y = 0;
        int n;
        make_lowpass(1200.0, 0.707, &c, oracle);
        for (n = 0; n < 65536; ++n)
            y = test_metal_biquad_process_q30(&c, &s, 5000000, &sat);
        check("DC step converges to unity gain without saturation",
              sat == 0 && y > 4999000 && y < 5001000);
    }
    {
        test_metal_biquad_state_t s = {123, -456};
        test_metal_biquad_reset(&s);
        check("reset clears both independent DF2T history states",
              s.z1 == 0 && s.z2 == 0);
    }
    {
        test_metal_biquad_coeff_q30_t c;
        test_metal_biquad_state_t s = {INT32_MAX, 0};
        double oc[5];
        uint32_t sat = 0;
        int32_t y;
        make_lowpass(1200.0, 2.0, &c, oc);
        y = test_metal_biquad_process_q30(&c, &s, INT32_MAX, &sat);
        check("over-range state is explicitly saturated and counted",
              y == INT32_MAX && sat >= 1);
    }

    printf("metal biquad: worst impulse error %.3f LSB; worst sine error %.3f LSB\n",
           worst_impulse, worst_sine);
    printf("metal biquad: %s (%u checks, %d failures)\n",
           failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
