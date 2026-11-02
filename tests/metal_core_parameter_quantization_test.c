/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — CP1.4.2 host-only parameter quantization reference.
 *
 * This is a test oracle only. It is not linked into firmware and does not
 * assign parameter IDs, editor pages, storage bytes, or a persistence ABI.
 */
#include <math.h>
#include <stdio.h>
#include <stddef.h>

typedef enum { MAP_LINEAR, MAP_LOG, MAP_BIPOLAR_LINEAR } map_t;
typedef struct {
    const char *name;
    double min_value;
    double max_value;
    double default_value;
    double source_step;
    map_t map;
    int expected_default_code;
} control_t;

static const control_t controls[] = {
    {"FREQ", 30, 500, 160, 1, MAP_LOG, 76},
    {"FM INDEX", 0, 1, 0, 0.01, MAP_LINEAR, 0},
    {"METAL RATIO", 0.5, 12, 4.2, 0.01, MAP_LOG, 85},
    {"WEST COAST DRIVE", 0, 1, 0, 0.01, MAP_LINEAR, 0},
    {"FOLD", 0, 1, 0.25, 0.01, MAP_LINEAR, 32},
    {"CUTOFF", 100, 8000, 1200, 10, MAP_LOG, 72},
    {"RESONANCE", 0.5, 20, 2, 0.1, MAP_LOG, 48},
    {"LPG DECAY", 0.02, 2, 0.25, 0.01, MAP_LOG, 70},
    {"OSC2 LEVEL", 0, 1, 0.5, 0.01, MAP_LINEAR, 64},
    {"OSC2 DETUNE", 0, 30, 7, 0.5, MAP_LINEAR, 30},
    {"TIMBRE", 0, 1, 0, 0.01, MAP_LINEAR, 0},
    {"MOD RATIO", 0.25, 16, 3.07, 0.01, MAP_LOG, 77},
    {"AM DEPTH", 0, 1, 0, 0.01, MAP_LINEAR, 0},
    {"FM FEEDBACK", 0, 0.8, 0, 0.01, MAP_LINEAR, 0},
    {"FM ENV AMT", 0, 1, 0.5, 0.01, MAP_LINEAR, 64},
    {"FM ENV ATTACK", 0.001, 1, 0.005, 0.001, MAP_LOG, 30},
    {"FM ENV DECAY", 0.01, 3, 0.25, 0.01, MAP_LOG, 72},
    {"METAL DECAY", 0.05, 3, 0.4, 0.01, MAP_LOG, 65},
    {"FOLD SYMMETRY", -1, 1, 0, 0.01, MAP_BIPOLAR_LINEAR, 0},
    {"WAVEFOLDER DRIVE", 0, 1, 0.2, 0.01, MAP_LINEAR, 25},
    {"LPG RESPONSE", 0, 1, 0.5, 0.01, MAP_LINEAR, 64},
    {"VOICE VCA LEVEL", 0, 1, 0.5, 0.01, MAP_LINEAR, 64},
    {"WAVEFOLDER MODULE VOLUME", 0, 1, 1, 0.01, MAP_LINEAR, 127},
    {"WEST COAST VOLUME", 0, 1, 1, 0.01, MAP_LINEAR, 127},
    {"COMPLEX MODULE VOLUME", 0, 1, 1, 0.01, MAP_LINEAR, 127},
    {"LPG MODULE VOLUME", 0, 1, 1, 0.01, MAP_LINEAR, 127},
    {"ATTACK", 0.001, 1, 0.01, 0.001, MAP_LOG, 42},
    {"DECAY", 0.05, 3, 0.3, 0.01, MAP_LOG, 56},
    {"SUSTAIN", 0, 1, 0, 0.01, MAP_LINEAR, 0},
    {"RELEASE", 0.01, 3, 0.2, 0.01, MAP_LOG, 67}
};

#define CONTROL_COUNT (sizeof(controls) / sizeof(controls[0]))

static int code_min(const control_t *c) {
    return c->map == MAP_BIPOLAR_LINEAR ? -64 : 0;
}
static int code_max(const control_t *c) { return c->map == MAP_BIPOLAR_LINEAR ? 63 : 127; }

static int encode_value(const control_t *c, double value, int *out) {
    double normalized;
    long rounded;
    if (!c || !out || !isfinite(value) ||
        !isfinite(c->min_value) || !isfinite(c->max_value) ||
        c->min_value >= c->max_value)
        return 0;
    if (value < c->min_value) value = c->min_value;
    if (value > c->max_value) value = c->max_value;
    if (c->map == MAP_LOG) {
        if (c->min_value <= 0 || value <= 0) return 0;
        normalized = log(value / c->min_value) /
                    log(c->max_value / c->min_value);
        rounded = lround(127.0 * normalized);
    } else if (c->map == MAP_BIPOLAR_LINEAR) {
        rounded = value < 0.0 ? lround(value * 64.0) : lround(value * 63.0);
    } else {
        normalized = (value - c->min_value) /
                     (c->max_value - c->min_value);
        rounded = lround(127.0 * normalized);
    }
    if (rounded < code_min(c)) rounded = code_min(c);
    if (rounded > code_max(c)) rounded = code_max(c);
    *out = (int)rounded;
    return 1;
}

static int decode_code(const control_t *c, int code, double *out) {
    double normalized;
    if (!c || !out || code < code_min(c) || code > code_max(c) ||
        !isfinite(c->min_value) || !isfinite(c->max_value) ||
        c->min_value >= c->max_value)
        return 0;
    if (c->map == MAP_LOG) {
        if (c->min_value <= 0) return 0;
        normalized = (double)code / 127.0;
        *out = c->min_value *
               pow(c->max_value / c->min_value, normalized);
    } else if (c->map == MAP_BIPOLAR_LINEAR) {
        *out = code < 0 ? (double)code / 64.0 : (double)code / 63.0;
    } else {
        normalized = (double)code / 127.0;
        *out = c->min_value +
               normalized * (c->max_value - c->min_value);
    }
    return isfinite(*out);
}

static int failures;
static void check(const char *name, int ok) {
    printf("CP1.4.2: %-72s %s\n", name, ok ? "PASS" : "FAIL");
    failures += !ok;
}

static void test_table_and_defaults(void) {
    size_t i;
    check("conversion table contains exactly 30 continuous controls",
          CONTROL_COUNT == 30);
    for (i = 0; i < CONTROL_COUNT; ++i) {
        int code = 999;
        char label[128];
        int ok = encode_value(&controls[i], controls[i].default_value, &code) &&
                 code == controls[i].expected_default_code;
        (void)snprintf(label, sizeof(label), "%s default encodes to draft code %d",
                       controls[i].name, controls[i].expected_default_code);
        check(label, ok);
    }
}

static void test_endpoints_and_roundtrip(void) {
    size_t i;
    for (i = 0; i < CONTROL_COUNT; ++i) {
        const control_t *c = &controls[i];
        int lo = 999, hi = 999, code;
        double decoded = 0.0;
        char label[160];
        int ok = encode_value(c, c->min_value, &lo) &&
                 encode_value(c, c->max_value, &hi) &&
                 lo == code_min(c) && hi == code_max(c);
        (void)snprintf(label, sizeof(label), "%s endpoints map exactly", c->name);
        check(label, ok);
        ok = 1;
        for (code = code_min(c); code <= code_max(c); ++code) {
            int roundtrip = 999;
            if (!decode_code(c, code, &decoded) ||
                decoded < c->min_value - 1e-12 ||
                decoded > c->max_value + 1e-12 ||
                !encode_value(c, decoded, &roundtrip) ||
                roundtrip != code) {
                ok = 0;
                break;
            }
        }
        (void)snprintf(label, sizeof(label),
                       "%s every legal code decodes in range and round-trips",
                       c->name);
        check(label, ok);
    }
}

static void test_monotonicity_and_error(void) {
    size_t i;
    for (i = 0; i < CONTROL_COUNT; ++i) {
        const control_t *c = &controls[i];
        double previous = -HUGE_VAL, max_abs_error = 0.0;
        double max_rel_error = 0.0, default_decoded = 0.0;
        int sample, ok = 1, default_code = 0;
        char label[160];
        for (sample = 0; sample <= 10000; ++sample) {
            double fraction = (double)sample / 10000.0;
            double value = c->min_value +
                           fraction * (c->max_value - c->min_value);
            double decoded, abs_error, rel_error;
            int code;
            if (c->map == MAP_LOG)
                value = c->min_value *
                        pow(c->max_value / c->min_value, fraction);
            if (!encode_value(c, value, &code) ||
                !decode_code(c, code, &decoded) || decoded + 1e-12 < previous) {
                ok = 0;
                break;
            }
            previous = decoded;
            abs_error = fabs(decoded - value);
            if (abs_error > max_abs_error) max_abs_error = abs_error;
            rel_error = fabs(value) > 1e-15 ? abs_error / fabs(value) : abs_error;
            if (rel_error > max_rel_error) max_rel_error = rel_error;
        }
        (void)snprintf(label, sizeof(label), "%s mapping is monotonic over 10001 samples",
                       c->name);
        check(label, ok);
        if (!encode_value(c, c->default_value, &default_code) ||
            !decode_code(c, default_code, &default_decoded))
            default_decoded = NAN;
        printf("CP1.4.2 METRIC %-25s max_abs=%0.9g max_rel=%0.6g default_error=%0.9g\n",
               c->name, max_abs_error, max_rel_error,
               fabs(default_decoded - c->default_value));
    }
}


static void test_source_step_boundaries(void) {
    size_t i;
    unsigned long total_boundaries = 0;
    for (i = 0; i < CONTROL_COUNT; ++i) {
        const control_t *c = &controls[i];
        unsigned long n, count;
        int previous_code = -999, ok = c->source_step > 0.0;
        double max_error_steps = 0.0;
        char label[160];
        count = (unsigned long)floor((c->max_value - c->min_value) /
                                     c->source_step + 1e-9);
        for (n = 0; ok && n <= count; ++n) {
            double source = c->min_value + (double)n * c->source_step;
            double decoded = 0.0;
            int code = 0;
            double error_steps;
            if (source > c->max_value + 1e-9 ||
                !encode_value(c, source, &code) ||
                !decode_code(c, code, &decoded) ||
                decoded < c->min_value - 1e-12 ||
                decoded > c->max_value + 1e-12 ||
                code < previous_code) {
                ok = 0;
                break;
            }
            previous_code = code;
            error_steps = fabs(decoded - source) / c->source_step;
            if (error_steps > max_error_steps) max_error_steps = error_steps;
        }
        total_boundaries += count + 1;
        (void)snprintf(label, sizeof(label),
                       "%s all %lu declared source-step boundaries quantize monotonically",
                       c->name, count + 1);
        check(label, ok);
        printf("CP1.4.2 STEP-METRIC %-25s boundaries=%lu max_error_steps=%0.4f\n",
               c->name, count + 1, max_error_steps);
    }
    printf("CP1.4.2 SOURCE-STEP TOTAL boundaries=%lu controls=%zu\n",
           total_boundaries, CONTROL_COUNT);
}

static void test_quantization_bounds_and_clamping(void) {
    size_t i;
    for (i = 0; i < CONTROL_COUNT; ++i) {
        const control_t *c = &controls[i];
        double max_error = 0.0, bound;
        int sample, ok = 1, lo = 999, hi = 999;
        char label[176];
        for (sample = 0; sample <= 100000; ++sample) {
            double fraction = (double)sample / 100000.0;
            double value = c->min_value + fraction * (c->max_value - c->min_value);
            double decoded = 0.0, error;
            int code = 0;
            if (c->map == MAP_LOG)
                value = c->min_value * pow(c->max_value / c->min_value, fraction);
            if (!encode_value(c, value, &code) || !decode_code(c, code, &decoded)) {
                ok = 0; break;
            }
            if (c->map == MAP_LOG) error = fabs(decoded / value - 1.0);
            else if (c->map == MAP_BIPOLAR_LINEAR) error = fabs(decoded - value);
            else error = fabs(decoded - value) / (c->max_value - c->min_value);
            if (error > max_error) max_error = error;
        }
        if (c->map == MAP_LOG) {
            double ratio = pow(c->max_value / c->min_value, 1.0 / 127.0);
            bound = fmax(ratio - 1.0, 1.0 - 1.0 / ratio);
        } else if (c->map == MAP_BIPOLAR_LINEAR) bound = 0.5 / 63.0;
        else bound = 0.5 / 127.0;
        (void)snprintf(label, sizeof(label), "%s dense-grid error stays within nearest-code bound", c->name);
        check(label, ok && max_error <= bound + 1e-10);
        printf("CP1.4.2 BOUND-METRIC %-25s measured=%0.9g limit=%0.9g\n", c->name, max_error, bound);
        ok = encode_value(c, c->min_value - (c->max_value - c->min_value), &lo) &&
             encode_value(c, c->max_value + (c->max_value - c->min_value), &hi) &&
             lo == code_min(c) && hi == code_max(c);
        (void)snprintf(label, sizeof(label), "%s out-of-range finite values clamp to exact endpoints", c->name);
        check(label, ok);
    }
}

static void test_invalid_inputs_and_bipolar_center(void) {
    int code = 0;
    double value = 0.0;
    const control_t *bipolar = &controls[18];
    int ok = !encode_value(&controls[0], NAN, &code) &&
             !encode_value(&controls[0], INFINITY, &code) &&
             !decode_code(&controls[0], -1, &value) &&
             !decode_code(&controls[18], -65, &value) &&
             !decode_code(&controls[18], 128, &value);
    check("NaN, infinity and out-of-range raw codes are rejected", ok);
    ok = encode_value(bipolar, 0.0, &code) && code == 0 &&
         decode_code(bipolar, code, &value) && fabs(value) < 1e-12;
    check("FOLD SYMMETRY zero maps exactly to signed code 0",
          ok);
    printf("CP1.4.2 NOTE bipolar zero decoded at code 0 = %.12f; signed range = -64..63 (128 codes)\n", value);
}

int main(void) {
    test_table_and_defaults();
    test_endpoints_and_roundtrip();
    test_monotonicity_and_error();
    test_source_step_boundaries();
    test_quantization_bounds_and_clamping();
    test_invalid_inputs_and_bipolar_center();
    printf("CP1.4.2 quantization reference: %s (%d failures; %zu controls)\n",
           failures ? "FAIL" : "PASS", failures, CONTROL_COUNT);
    return failures ? 1 : 0;
}
