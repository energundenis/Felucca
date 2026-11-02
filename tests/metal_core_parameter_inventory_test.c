/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — CP1.4.1 logical parameter inventory contract.
 *
 * Host-only guard for the accepted 8 Engine EDIT + 16 P_MC + 13 Private map.
 * This does not assign numeric IDs, firmware descriptors, pages or storage ABI.
 */
#include <stdio.h>
#include <string.h>

static const char *const engine_edit[] = {
    "FREQ",
    "OSC2 LEVEL",
    "TIMBRE",
    "WEST COAST DRIVE",
    "METAL RATIO",
    "FOLD",
    "CUTOFF",
    "RESONANCE"
};

static const char *const p_mc[] = {
    "OSC2 DETUNE",
    "FM INDEX",
    "MOD RATIO",
    "AM DEPTH",
    "FM FEEDBACK",
    "FM ENV AMT",
    "FM ENV ATTACK",
    "FM ENV DECAY",
    "METAL DECAY",
    "FOLD SYMMETRY",
    "WAVEFOLDER DRIVE",
    "LPG RESPONSE",
    "LPG DECAY",
    "VOICE VCA LEVEL",
    "WAVEFOLDER MODULE VOLUME",
    "WEST COAST VOLUME"
};

static const char *const private_part[] = {
    "WAVE",
    "FOLLOW",
    "METAL",
    "MODULATION",
    "MOD OSC WAVE",
    "FM CHARACTER",
    "COMPLEX MODULE VOLUME",
    "FOLD MODE",
    "LPG MODULE VOLUME",
    "ATTACK",
    "DECAY",
    "SUSTAIN",
    "RELEASE"
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(const char *name, int ok)
{
    printf("CP1.4.1: %-76s %s\n", name, ok ? "PASS" : "FAIL");
    failures += !ok;
}

static int contains(const char *const *a, size_t n, const char *value)
{
    size_t i;
    for (i = 0; i < n; ++i)
        if (strcmp(a[i], value) == 0)
            return 1;
    return 0;
}

static void test_counts_and_frozen_engine_map(void)
{
    static const char *const expected[] = {
        "FREQ", "OSC2 LEVEL", "TIMBRE", "WEST COAST DRIVE",
        "METAL RATIO", "FOLD", "CUTOFF", "RESONANCE"
    };
    size_t i;
    int ok = COUNT(engine_edit) == 8 && COUNT(p_mc) == 16 &&
             COUNT(private_part) == 13 &&
             COUNT(engine_edit) + COUNT(p_mc) + COUNT(private_part) == 37;
    for (i = 0; ok && i < COUNT(expected); ++i)
        ok = strcmp(engine_edit[i], expected[i]) == 0;
    check("accepted E0-E7 order and 8 + 16 + 13 = 37 controls", ok);
    check("four independent Private banks contain 52 values",
          COUNT(private_part) * 4u == 52u);
}

static void test_no_duplicates(void)
{
    const char *const *groups[] = {engine_edit, p_mc, private_part};
    const size_t sizes[] = {COUNT(engine_edit), COUNT(p_mc), COUNT(private_part)};
    size_t g, i, h, j;
    int ok = 1;
    for (g = 0; ok && g < 3; ++g)
        for (i = 0; ok && i < sizes[g]; ++i)
            for (h = g; ok && h < 3; ++h)
                for (j = h == g ? i + 1u : 0u; j < sizes[h]; ++j)
                    if (strcmp(groups[g][i], groups[h][j]) == 0) {
                        ok = 0;
                        break;
                    }
    check("all 37 logical controls are unique across E / P_MC / Private", ok);
}

static void test_reconciled_membership(void)
{
    check("OSC2 LEVEL and TIMBRE remain E1/E2, not duplicated in P_MC",
          contains(engine_edit, COUNT(engine_edit), "OSC2 LEVEL") &&
          contains(engine_edit, COUNT(engine_edit), "TIMBRE") &&
          !contains(p_mc, COUNT(p_mc), "OSC2 LEVEL") &&
          !contains(p_mc, COUNT(p_mc), "TIMBRE"));
    check("FM INDEX and LPG DECAY are common P_MC controls, not E slots",
          contains(p_mc, COUNT(p_mc), "FM INDEX") &&
          contains(p_mc, COUNT(p_mc), "LPG DECAY") &&
          !contains(engine_edit, COUNT(engine_edit), "FM INDEX") &&
          !contains(engine_edit, COUNT(engine_edit), "LPG DECAY"));
    check("FOLLOW and METAL remain separate Private booleans",
          contains(private_part, COUNT(private_part), "FOLLOW") &&
          contains(private_part, COUNT(private_part), "METAL"));
}

int main(void)
{
    test_counts_and_frozen_engine_map();
    test_no_duplicates();
    test_reconciled_membership();
    printf(failures ? "CP1.4.1 inventory test FAILED (%d)\n" :
                      "CP1.4.1 inventory test passed\n", failures);
    return failures != 0;
}
