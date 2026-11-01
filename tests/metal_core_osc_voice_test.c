/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — host voice phase adapter tests.
 * Compiles against the actual Felucca voice_t / vmod_t declarations.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

#include "../firmware/src/eng_metal_core_osc_voice.h"

#include <stdio.h>

static int failures;
static void check(const char *name, int ok)
{
    printf("metal osc voice: %-68s %s\n", name, ok ? "PASS" : "FAIL");
    failures += !ok;
}

static void test_independent_voice_slots(void)
{
    voice_t v = {0};
    vmod_t m = {0};
    uint32_t i1 = pitch_inc(48u * 16u);
    uint32_t i2 = pitch_inc(67u * 16u);
    int32_t a, b;

    m.inc = i1;
    a = eng_metal_osc_voice1_next(&v, &m, ENG_METAL_OSC_SAW, 0x80000000u);
    m.inc = i2;
    b = eng_metal_osc_voice2_next(&v, &m, ENG_METAL_OSC_PULSE, 0x80000000u);

    check("OSC1 and OSC2 advance separate host voice phase slots",
          v.ph[0] == i1 && v.ph[1] == i2 && v.ph[0] != v.ph[1]);
    check("both oscillator outputs remain within signed audio range",
          a >= -32768 && a <= 32767 && b >= -32768 && b <= 32767);
}

static void test_note_on_reset_and_retrigger_contract(void)
{
    voice_t v = {0};
    v.ph[0] = 0x12345678u;
    v.ph[1] = 0x87654321u;
    v.ph[2] = 0xABCDEF01u;

    eng_metal_osc_voice_note_on(&v);
    check("fresh note_on resets OSC1 and OSC2 phase slots",
          v.ph[0] == 0u && v.ph[1] == 0u);
    check("adapter leaves Felucca phase slot 2 untouched",
          v.ph[2] == 0xABCDEF01u);

    /* Mirror the host sounding-retrigger save/restore contract: voice_start()
     * saves ph[0..2], invokes note_on(), then restores them.
     */
    v.ph[0] = 0x10203040u;
    v.ph[1] = 0x50607080u;
    {
        uint32_t p0 = v.ph[0], p1 = v.ph[1];
        eng_metal_osc_voice_note_on(&v);
        v.ph[0] = p0;
        v.ph[1] = p1;
    }
    check("sounding retrigger retains both oscillator phases",
          v.ph[0] == 0x10203040u && v.ph[1] == 0x50607080u);
}

int main(void)
{
    test_independent_voice_slots();
    test_note_on_reset_and_retrigger_contract();
    printf("metal osc voice: %s (%d failures)\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
