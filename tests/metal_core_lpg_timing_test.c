/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated LPG timing/target math tests (host-only).
 *
 * Models v98 time constants and gate limiting plus approved Option C peak.
 * Does not execute an audio envelope or filter.
 */
#include "../firmware/src/eng_metal_core_math.h"

#include <stdio.h>
#include <stdint.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal LPG timing: %-58s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

int main(void)
{
    eng_metal_lpg_timing_t lo, hi, short_gate, short_decay, capped, zero_rate;
    const uint32_t sr = 44100u;
    const uint32_t gate_125ms = 5513u;

    eng_metal_lpg_timing_q15(sr, 1000000u, gate_125ms, 0, 2000u, &lo);
    eng_metal_lpg_timing_q15(sr, 1000000u, gate_125ms, 32767, 2000u, &hi);
    eng_metal_lpg_timing_q15(sr, 1000000u, 1000u, 16384, 2000u, &short_gate);
    eng_metal_lpg_timing_q15(sr, 20000u, 20000u, 0, 2000u, &short_decay);
    eng_metal_lpg_timing_q15(sr, 1000000u, gate_125ms, 32767, 3000u, &capped);
    eng_metal_lpg_timing_q15(0, 1000000u, gate_125ms, 32767, 2000u, &zero_rate);

    check("RESPONSE endpoints map attack to 1.5 ms and 7.5 ms",
          lo.attack_samples >= 65u && lo.attack_samples <= 67u &&
          hi.attack_samples >= 330u && hi.attack_samples <= 332u);
    check("tail is limited by the shorter gate",
          lo.tail_samples == gate_125ms && short_gate.tail_samples == 1000u);
    check("gain decay time constant grows with RESPONSE",
          hi.gain_decay_tau_samples > lo.gain_decay_tau_samples);
    check("gain decay keeps the 6 ms minimum",
          short_decay.gain_decay_tau_samples >= 264u &&
          short_decay.gain_decay_tau_samples <= 266u);
    check("filter return is gate-limited and uses DECAY times 0.22",
          lo.filter_return_samples == gate_125ms &&
          short_decay.filter_return_samples >= 193u &&
          short_decay.filter_return_samples <= 195u);
    check("filter decay time constant grows with RESPONSE",
          hi.filter_decay_tau_samples > lo.filter_decay_tau_samples);
    check("filter decay keeps the 12 ms minimum",
          short_decay.filter_decay_tau_samples >= 529u &&
          short_decay.filter_decay_tau_samples <= 531u);
    check("transient cutoff follows 1.25x to 5x RESPONSE scaling",
          lo.filter_transient_hz >= 2499u && lo.filter_transient_hz <= 2501u &&
          hi.filter_transient_hz >= 9999u && hi.filter_transient_hz <= 10001u);
    check("transient cutoff is capped at 12 kHz and return never below 100 Hz",
          capped.filter_transient_hz == 12000u &&
          capped.filter_return_hz == 3000u);
    check("zero sample rate returns a cleared schedule",
          zero_rate.attack_samples == 0u && zero_rate.tail_samples == 0u &&
          zero_rate.filter_transient_hz == 0u);

    printf("metal LPG timing: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
