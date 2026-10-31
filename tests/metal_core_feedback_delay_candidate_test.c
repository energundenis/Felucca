/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only tests for the provisional 3 ms / 44.1 kHz feedback delay.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "metal_core_feedback_delay_candidate.h"
#include "eng_metal_core_vca.h"

#define CHECK(c, m) do { if (!(c)) { \
    fprintf(stderr, "FAIL: %s (line %d)\\n", (m), __LINE__); ++failures; \
} } while (0)

static int failures;

/* Candidate note-on policy: VCA non-idle means sounding retrigger / steal. */
static void feedback_note_on_candidate(test_metal_feedback_delay_t *delay,
                                       uint8_t previous_vca_phase)
{
    if (previous_vca_phase == ENG_METAL_VCA_IDLE)
        test_metal_feedback_delay_reset(delay);
}

static int64_t abs_i32(int32_t value)
{
    return value < 0 ? -(int64_t)value : (int64_t)value;
}

/* Test-only closed loop: delayed sample -> gain -> delay input.
 * Division is intentional here: the harness checks bounded behavior, not
 * the exact rounding policy of a future production feedback multiplier.
 */
static uint32_t run_feedback_loop(uint16_t gain_q15, int32_t impulse,
                                 int64_t *first_echo, int64_t *tail_peak)
{
    test_metal_feedback_delay_t delay;
    uint32_t n, hash = 2166136261u;
    *first_echo = 0;
    *tail_peak = 0;
    test_metal_feedback_delay_reset(&delay);

    for (n = 0; n < 8192u; ++n) {
        int32_t delayed = test_metal_feedback_delay_read(&delay);
        int64_t feedback = ((int64_t)delayed * gain_q15) / 32768;
        int64_t input = feedback + (n == 0u ? impulse : 0);

        CHECK(input <= INT32_MAX && input >= INT32_MIN,
              "feedback loop must not overflow int32");
        if (input > INT32_MAX) input = INT32_MAX;
        if (input < INT32_MIN) input = INT32_MIN;
        CHECK(impulse > 0 ? input >= 0 : input <= 0,
              "feedback loop must preserve signal polarity");
        CHECK(abs_i32((int32_t)input) <= abs_i32(impulse),
              "feedback gain below unity must not grow the impulse");

        if (n == 132u || n == 133u) {
            if (abs_i32((int32_t)feedback) > *first_echo)
                *first_echo = abs_i32((int32_t)feedback);
        }
        if (n >= 2048u && abs_i32((int32_t)feedback) > *tail_peak)
            *tail_peak = abs_i32((int32_t)feedback);

        hash ^= (uint32_t)(int32_t)input;
        hash *= 16777619u;
        test_metal_feedback_delay_write(&delay, (int32_t)input);
    }
    return hash;
}

int main(void)
{
    test_metal_feedback_delay_t a, b;
    const int32_t impulse = 1 << 28;
    const uint16_t gains[] = {0u, 8192u, 16384u, 26214u}; /* 0, .25, .5, .8 */
    int32_t out132 = 0, out133 = 0;
    uint32_t n, i;

    _Static_assert(TEST_METAL_FB_RING_SAMPLES == 133u,
                   "3 ms at 44.1 kHz requires 133 ring slots");
    _Static_assert(sizeof(a.sample) == 532u,
                   "feedback ring sample storage must be 532 bytes");
    _Static_assert(sizeof(a) <= 536u,
                   "ring plus cursor must remain within 536 bytes");

    test_metal_feedback_delay_reset(&a);
    test_metal_feedback_delay_reset(&b);
    CHECK(a.write_index == 0u && b.write_index == 0u,
          "reset must zero both cursors");

    /* Read-before-write: an impulse appears at n=132 and n=133. */
    for (n = 0; n < 140u; ++n) {
        int32_t y = test_metal_feedback_delay_read(&a);
        if (n == 132u) out132 = y;
        if (n == 133u) out133 = y;
        test_metal_feedback_delay_write(&a, n == 0u ? impulse : 0);
    }
    CHECK(out132 > 0, "fractional delay first tap must be nonzero");
    CHECK(out133 > 0, "fractional delay second tap must be nonzero");
    CHECK(out132 > out133, "0.7 near tap must exceed 0.3 far tap");
    CHECK(out132 > 187800000 && out132 < 188000000,
          "first tap must approximate 0.7 of the impulse");
    CHECK(out133 > 80400000 && out133 < 80700000,
          "second tap must approximate 0.3 of the impulse");

    /* Reset must erase old feedback energy and restore the cursor. */
    test_metal_feedback_delay_reset(&a);
    CHECK(a.write_index == 0u, "reset must rewind the ring cursor");
    for (n = 0; n < 140u; ++n) {
        CHECK(test_metal_feedback_delay_read(&a) == 0,
              "reset ring must contain no stale feedback");
        test_metal_feedback_delay_write(&a, 0);
    }

    /* Resetting one voice slot must not modify another slot. */
    test_metal_feedback_delay_write(&a, 1234567);
    test_metal_feedback_delay_write(&b, 7654321);
    test_metal_feedback_delay_reset(&a);
    CHECK(b.sample[0] == 7654321 && b.write_index == 1u,
          "per-voice reset must not affect a different ring");

    /* VCA phase is a candidate discriminator for the prior voice lifecycle. */
    test_metal_feedback_delay_reset(&a);
    test_metal_feedback_delay_write(&a, 111111);
    test_metal_feedback_delay_write(&a, 222222);
    feedback_note_on_candidate(&a, ENG_METAL_VCA_ATTACK);
    CHECK(a.write_index == 2u && a.sample[0] == 111111 &&
          a.sample[1] == 222222,
          "active retrigger must preserve feedback history");
    feedback_note_on_candidate(&a, ENG_METAL_VCA_RELEASE);
    CHECK(a.write_index == 2u && a.sample[0] == 111111,
          "released-but-sounding steal must preserve feedback history");
    feedback_note_on_candidate(&a, ENG_METAL_VCA_IDLE);
    CHECK(a.write_index == 0u && a.sample[0] == 0 && a.sample[1] == 0,
          "fresh note after VCA idle must clear stale feedback");

    /* Closed-loop stability at gains 0, .25, .5 and .8, both polarities.
     * Repeat each run to prove deterministic integer behavior.
     */
    for (i = 0; i < sizeof(gains) / sizeof(gains[0]); ++i) {
        int64_t first_pos, tail_pos, first_neg, tail_neg;
        int64_t repeat_first, repeat_tail;
        uint32_t hash_pos, hash_pos_repeat, hash_neg, hash_neg_repeat;
        hash_pos = run_feedback_loop(gains[i], INT32_MAX,
                                     &first_pos, &tail_pos);
        hash_pos_repeat = run_feedback_loop(gains[i], INT32_MAX,
                                            &repeat_first, &repeat_tail);
        hash_neg = run_feedback_loop(gains[i], -INT32_MAX,
                                     &first_neg, &tail_neg);
        hash_neg_repeat = run_feedback_loop(gains[i], -INT32_MAX,
                                             &repeat_first, &repeat_tail);
        CHECK(hash_pos == hash_pos_repeat && hash_neg == hash_neg_repeat,
              "feedback loop must be deterministic for a fixed gain and impulse");
        if (gains[i] == 0u) {
            CHECK(first_pos == 0 && first_neg == 0 &&
                  tail_pos == 0 && tail_neg == 0,
                  "zero feedback gain must produce no echoes");
        } else {
            CHECK(first_pos > 0 && first_neg > 0,
                  "nonzero feedback gain must produce an echo");
            CHECK(tail_pos < first_pos && tail_neg < first_neg,
                  "feedback echoes must decay over repeated delay cycles");
        }
    }

    if (failures) {
        fprintf(stderr, "CP3.1 feedback delay candidate: %d failures\\n", failures);
        return 1;
    }
    puts("CP3.1 feedback delay candidate: PASS (0 failures; host-only, not production DSP)");
    return 0;
}
