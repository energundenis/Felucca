/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — CP1.6 Base projection / Motion ownership tests.
 */
#include <stdio.h>
#include <string.h>
#include "../firmware/src/energun_project_base_projection.h"

static unsigned int checks, failures;
static void check(const char *name, int ok)
{
    ++checks;
    printf("CP1.6 Base projection: %-74s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

static void test_base_projection(void)
{
    int16_t runtime[ENERGUN_PROJECT_RUNTIME_NP];
    int16_t defaults[ENERGUN_PROJECT_RUNTIME_NP];
    int16_t loaded[ENERGUN_PROJECT_RUNTIME_NP];
    uint8_t base[ENERGUN_PROJECT_BASE_NP];
    uint8_t base_again[ENERGUN_PROJECT_BASE_NP];
    unsigned int i;

    for (i = 0; i < ENERGUN_PROJECT_RUNTIME_NP; ++i) {
        runtime[i] = defaults[i] = (int16_t)((i % 80u) - 40);
    }
    for (i = 0; i < ENERGUN_PROJECT_PMC_COUNT; ++i)
        runtime[ENERGUN_PROJECT_PMC_FIRST_ID + i] = (int16_t)(i * 7u);
    for (i = 0; i < 8u; ++i)
        runtime[ENERGUN_PROJECT_ENGINE_FIRST_ID + i] = (int16_t)(100u + i);

    check("pack emits exactly the legacy 111-parameter Base projection",
          energun_project_pack_base_params(base, runtime) == ENERGUN_PROJECT_MAP_OK &&
          sizeof base == 111u);
    runtime[ENERGUN_PROJECT_PMC_FIRST_ID + 0u] = 127;
    runtime[ENERGUN_PROJECT_PMC_FIRST_ID + 15u] = -64;
    check("P_MC endpoint changes do not alter Base FUN10 bytes",
          energun_project_pack_base_params(base_again, runtime) == ENERGUN_PROJECT_MAP_OK &&
          memcmp(base, base_again, sizeof base) == 0);

    check("legacy Base load restores common values and E0-E7, defaults P_MC",
          energun_project_unpack_base_params(loaded, base, defaults) == ENERGUN_PROJECT_MAP_OK &&
          loaded[0] == runtime[0] &&
          loaded[102] == runtime[102] &&
          loaded[ENERGUN_PROJECT_ENGINE_FIRST_ID] == 100 &&
          loaded[ENERGUN_PROJECT_ENGINE_FIRST_ID + 7u] == 107 &&
          loaded[ENERGUN_PROJECT_PMC_FIRST_ID] == defaults[ENERGUN_PROJECT_PMC_FIRST_ID] &&
          loaded[ENERGUN_PROJECT_PMC_FIRST_ID + 15u] == defaults[ENERGUN_PROJECT_PMC_FIRST_ID + 15u]);

    check("Base byte layout uses the existing value+64 encoding",
          base[0] == (uint8_t)(runtime[0] + 64) &&
          base[ENERGUN_PROJECT_BASE_COMMON_NP] == (uint8_t)(100 + 64));

    runtime[0] = 128;
    check("out-of-range Base common value is rejected",
          energun_project_pack_base_params(base, runtime) == ENERGUN_PROJECT_MAP_ERR_VALUE);
    runtime[0] = 0;
    runtime[ENERGUN_PROJECT_ENGINE_FIRST_ID + 7u] = 128;
    check("out-of-range Engine EDIT value is rejected",
          energun_project_pack_base_params(base, runtime) == ENERGUN_PROJECT_MAP_ERR_VALUE);
    base[4] = 192u;
    check("invalid stored Base byte is rejected on load",
          energun_project_unpack_base_params(loaded, base, defaults) == ENERGUN_PROJECT_MAP_ERR_VALUE);
}

static void test_motion_split_merge(void)
{
    energun_project_motion_event_t input[4] = {
        {1u, 2u, 10},
        {65u, 0x80u | 103u, -64},
        {130u, 0x80u | 118u, 127},
        {255u, 126u, 0}
    };
    energun_project_motion_event_t base[ENERGUN_PROJECT_MOTION_MAX];
    energun_project_motion_event_t sidecar[ENERGUN_PROJECT_MOTION_MAX];
    energun_project_motion_event_t merged[ENERGUN_PROJECT_MOTION_MAX];
    uint8_t nb = 0u, ns = 0u, nm = 0u;

    check("Base Motion and P_MC Motion split into separate owners",
          energun_project_split_motion(input, 4u, base, &nb, sidecar, &ns) == ENERGUN_PROJECT_MAP_OK &&
          nb == 2u && ns == 2u);
    check("Sidecar stores logical P_MC indices and preserves lock flag",
          sidecar[0].param == 0x80u && sidecar[1].param == 0x8Fu);
    check("Base-owned events retain original IDs and values",
          base[0].param == 2u && base[1].param == 126u &&
          base[0].value == 10 && base[1].value == 0);
    check("merge restores the original combined Motion list",
          energun_project_merge_motion(base, nb, sidecar, ns, merged, &nm) == ENERGUN_PROJECT_MAP_OK &&
          nm == 4u && memcmp(input, merged, sizeof input) == 0);

    sidecar[0].param = 16u;
    check("Sidecar Motion rejects logical index 16 (Private namespace)",
          energun_project_merge_motion(base, nb, sidecar, ns, merged, &nm) == ENERGUN_PROJECT_MAP_ERR_ID);
    sidecar[0].param = 0u;
    base[0].param = 103u;
    check("Base Motion rejects P_MC ID leakage",
          energun_project_merge_motion(base, nb, sidecar, ns, merged, &nm) == ENERGUN_PROJECT_MAP_ERR_ID);
    base[0].param = 2u;
    input[1].param = 0x80u | 103u;
    input[2].param = 0x80u | 103u;
    check("duplicate P_MC place/parameter is rejected during split",
          energun_project_split_motion(input, 4u, base, &nb, sidecar, &ns) == ENERGUN_PROJECT_MAP_ERR_DUPLICATE);
    input[1].param = 0x80u | 103u;
    input[2].param = 0x80u | 118u;
    input[2].place = input[1].place;
    check("different P_MC controls may share a step",
          energun_project_split_motion(input, 4u, base, &nb, sidecar, &ns) == ENERGUN_PROJECT_MAP_OK);
    check("combined Base + Sidecar Motion count is bounded at 128",
          energun_project_merge_motion(base, 128u, sidecar, 1u, merged, &nm) == ENERGUN_PROJECT_MAP_ERR_COUNT);
}

int main(void)
{
    test_base_projection();
    test_motion_split_merge();
    printf("CP1.6 Base projection: %u checks, %u failures\n", checks, failures);
    return failures != 0u;
}
