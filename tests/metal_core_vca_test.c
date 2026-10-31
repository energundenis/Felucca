/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated VCA fixed-point host tests.
 * No Felucca engine registration, audio render, or device is exercised.
 */
#include "../firmware/src/eng_metal_core_vca.h"

#include <stdint.h>
#include <stdio.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal VCA: %-70s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static void test_helpers_and_targets(void)
{
    int32_t peak, sustain;
    check("time conversion at 44.1 kHz is rounded to nearest sample",
          eng_metal_vca_us_to_samples(44100u, 5000u) == 221u);
    check("zero sample rate/time produces zero samples",
          eng_metal_vca_us_to_samples(0u, 5000u) == 0u &&
          eng_metal_vca_us_to_samples(44100u, 0u) == 0u);
    check("coefficient is bounded and nonzero",
          eng_metal_vca_coefficient(0u) == ENG_METAL_VCA_Q30_ONE &&
          eng_metal_vca_coefficient(220u) > 0u &&
          eng_metal_vca_coefficient(220u) < ENG_METAL_VCA_Q30_ONE);

    eng_metal_vca_targets_q16(32768, 0, 0, 0, 0, &peak, &sustain);
    check("absent velocity defaults to 0.8 and zero sustain uses V98 floor",
          peak >= 26213 && peak <= 26215 &&
          sustain == ENG_METAL_VCA_Q16_FLOOR);

    eng_metal_vca_targets_q16(ENG_METAL_VCA_Q16_ONE,
                              ENG_METAL_VCA_Q16_ONE,
                              0, 1, 1, &peak, &sustain);
    check("velocity below 0.1 clamps and Accent peak remains within 1.18 headroom",
          peak >= 7730 && peak <= 7735 &&
          sustain >= ENG_METAL_VCA_Q16_FLOOR &&
          peak <= ENG_METAL_VCA_Q16_MAX);

    eng_metal_vca_targets_q16(ENG_METAL_VCA_Q16_ONE,
                              ENG_METAL_VCA_Q16_ONE,
                              ENG_METAL_VCA_Q16_ONE, 1, 1,
                              &peak, &sustain);
    check("Accent applies 1.18 peak and 1.08 sustain factors",
          peak >= 77330 && peak <= 77332 &&
          sustain >= 70777 && sustain <= 70780);
}

static void test_attack_decay_sustain_release(void)
{
    eng_metal_vca_t vca;
    uint32_t i;
    int32_t value;

    eng_metal_vca_reset(&vca);
    check("reset is silent, idle and done",
          eng_metal_vca_process(&vca) == 0 &&
          vca.phase == ENG_METAL_VCA_IDLE && eng_metal_vca_done(&vca));

    /* tau attack = 220 samples (5 ms); decay begins at 441 samples (10 ms). */
    eng_metal_vca_trigger(&vca, 50000, 12000, 220u, 441u,
                          13230u, 3000u, 8820u);
    for (i = 0; i < 220u; ++i)
        value = eng_metal_vca_process(&vca);
    check("Attack rises monotonically to approximately 63 percent at one tau",
          value >= 31000 && value <= 32000 &&
          vca.phase == ENG_METAL_VCA_ATTACK);

    for (; i < 441u; ++i)
        eng_metal_vca_process(&vca);
    check("Attack remains active up to the source-scheduled Decay boundary",
          vca.phase == ENG_METAL_VCA_ATTACK && vca.decay_remaining == 0u);

    eng_metal_vca_process(&vca);
    check("Decay begins at its scheduled boundary without a level reset",
          vca.phase == ENG_METAL_VCA_DECAY &&
          vca.level_q16 > 0 && vca.target_q16 == 12000);

    for (i = 0; i < 60000u && vca.phase != ENG_METAL_VCA_SUSTAIN; ++i)
        eng_metal_vca_process(&vca);
    check("Decay converges exactly to Sustain",
          vca.phase == ENG_METAL_VCA_SUSTAIN &&
          vca.level_q16 == 12000);

    for (i = 0; i < 3000u; ++i)
        eng_metal_vca_process(&vca);
    check("Sustain holds its target until Gate-off",
          vca.phase == ENG_METAL_VCA_SUSTAIN &&
          vca.level_q16 == 12000 && vca.gate_remaining == 0u);

    value = eng_metal_vca_process(&vca);
    check("Gate-off starts Release from current level",
          vca.phase == ENG_METAL_VCA_RELEASE &&
          value < 12000 && value > ENG_METAL_VCA_Q16_FLOOR);

    for (i = 0; i < 100000u && !eng_metal_vca_done(&vca); ++i)
        eng_metal_vca_process(&vca);
    check("Release reaches silence and reports done",
          eng_metal_vca_done(&vca) && vca.level_q16 == 0);
}

static void test_short_gate_priority(void)
{
    eng_metal_vca_t vca;
    uint32_t i;
    int32_t previous, value;
    int monotonic = 1;

    eng_metal_vca_reset(&vca);
    /* Gate ends at 100 samples; Attack/Decay boundary is at 441 samples. */
    eng_metal_vca_trigger(&vca, 60000, 10000, 441u, 441u,
                          13230u, 100u, 8820u);
    for (i = 0; i < 100u; ++i)
        previous = eng_metal_vca_process(&vca);
    check("short Gate ends while Attack is still active",
          vca.phase == ENG_METAL_VCA_ATTACK &&
          vca.decay_remaining > 0u && vca.gate_remaining == 0u);

    value = eng_metal_vca_process(&vca);
    check("Option B: Release wins before the pending Decay event",
          vca.phase == ENG_METAL_VCA_RELEASE &&
          vca.target_q16 == ENG_METAL_VCA_Q16_FLOOR &&
          value < previous);

    previous = value;
    for (i = 0; i < 50000u && !eng_metal_vca_done(&vca); ++i) {
        value = eng_metal_vca_process(&vca);
        if (value > previous) monotonic = 0;
        previous = value;
    }
    check("no post-Release rise; delayed Decay never reopens VCA",
          monotonic && eng_metal_vca_done(&vca) && vca.level_q16 == 0);
}

static void test_retrigger_and_zero_gate(void)
{
    eng_metal_vca_t vca;
    uint32_t i;
    int32_t before, after;

    eng_metal_vca_reset(&vca);
    eng_metal_vca_trigger(&vca, 60000, 5000, 220u, 441u,
                          13230u, 100u, 8820u);
    for (i = 0; i < 101u; ++i)
        eng_metal_vca_process(&vca);
    check("Gate ending during Attack has entered Release",
          vca.phase == ENG_METAL_VCA_RELEASE);
    for (i = 0; i < 20u; ++i)
        eng_metal_vca_process(&vca);
    before = vca.level_q16;
    eng_metal_vca_trigger(&vca, 65000, 8000, 220u, 441u,
                          13230u, 2000u, 8820u);
    after = eng_metal_vca_process(&vca);
    check("retrigger during Release holds current level, no reset-to-zero",
          vca.phase == ENG_METAL_VCA_ATTACK &&
          after >= before && after - before < 1000);

    eng_metal_vca_trigger(&vca, 50000, 7000, 220u, 441u,
                          13230u, 0u, 8820u);
    after = eng_metal_vca_process(&vca);
    check("zero Gate immediately selects Release",
          vca.phase == ENG_METAL_VCA_RELEASE &&
          after < before + 1000);
}

int main(void)
{
    test_helpers_and_targets();
    test_attack_decay_sustain_release();
    test_short_gate_priority();
    test_retrigger_and_zero_gate();
    printf("metal VCA: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
