/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — fixed-point Complex FM curve tests.
 */
#define NVOICE 8
#define NPART 4
#include "../firmware/engines/src/felucca_engine.h"
#include "felucca_tables.h"
#include "../firmware/engines/src/dsp.c"
#include "../firmware/src/eng_metal_core.c"

#include <stdio.h>

static int failures;

static void check(const char *name, int ok)
{
    printf("metal-fm-curve: %-64s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}

static void sqrt_curve(void)
{
    check("sqrt(0) is zero", metal_core_sqrt_q15(0) == 0);
    check("sqrt(1.0) is 1.0", metal_core_sqrt_q15(32767) == 32767);
    check("sqrt curve is monotonic at half scale",
          metal_core_sqrt_q15(16384) > metal_core_sqrt_q15(8192));
    check("negative input clamps to zero", metal_core_sqrt_q15(-100) == 0);
    check("input above unity clamps to unity", metal_core_sqrt_q15(40000) == 32767);
}

static void character_curves(void)
{
    int32_t smooth0 = metal_core_fm_character_q15(0, MC_FM_CHAR_SMOOTH);
    int32_t classic0 = metal_core_fm_character_q15(0, MC_FM_CHAR_CLASSIC);
    int32_t metal0 = metal_core_fm_character_q15(0, MC_FM_CHAR_METALLIC);
    int32_t smooth1 = metal_core_fm_character_q15(32767, MC_FM_CHAR_SMOOTH);
    int32_t classic1 = metal_core_fm_character_q15(32767, MC_FM_CHAR_CLASSIC);
    int32_t metal1 = metal_core_fm_character_q15(32767, MC_FM_CHAR_METALLIC);
    int32_t smooth_half = metal_core_fm_character_q15(16384, MC_FM_CHAR_SMOOTH);
    int32_t classic_half = metal_core_fm_character_q15(16384, MC_FM_CHAR_CLASSIC);
    int32_t metal_half = metal_core_fm_character_q15(16384, MC_FM_CHAR_METALLIC);

    check("all character curves start at zero", smooth0 == 0 && classic0 == 0 && metal0 == 0);
    check("SMOOTH full-scale equals approximately 0.72", smooth1 >= 23590 && smooth1 <= 23594);
    check("CLASSIC full-scale equals unity", classic1 == 32767);
    check("METALLIC full-scale reaches approximately 1.70", metal1 >= 55690 && metal1 <= 55710);
    check("SMOOTH half-scale follows 0.72 * sqrt(index)", smooth_half > classic_half);
    check("METALLIC half-scale follows its quadratic curve", metal_half > classic_half);
    check("invalid character safely defaults to METALLIC",
          metal_core_fm_character_q15(16384, 99u) == metal_half);
    check("index below zero clamps to zero", metal_core_fm_character_q15(-1, MC_FM_CHAR_CLASSIC) == 0);
    check("index above unity clamps to unity", metal_core_fm_character_q15(40000, MC_FM_CHAR_CLASSIC) == 32767);
}

static void drive_and_depth(void)
{
    int32_t half_drive = metal_core_west_coast_drive_q15(16384);
    int32_t full_drive = metal_core_west_coast_drive_q15(32767);
    int32_t classic_full = metal_core_complex_fm_depth_q15(32767, 32767, MC_FM_CHAR_CLASSIC);
    int32_t metallic_full = metal_core_complex_fm_depth_q15(32767, 32767, MC_FM_CHAR_METALLIC);

    check("WEST COAST DRIVE zero disables Complex FM depth",
          metal_core_complex_fm_depth_q15(32767, 0, MC_FM_CHAR_METALLIC) == 0);
    check("wcDrv(1) is unity", full_drive == 32766 || full_drive == 32767);
    check("wcDrv(0.5) equals drive raised to 1.5", half_drive >= 11570 && half_drive <= 11610);
    check("full CLASSIC depth equals unity", classic_full >= 32760 && classic_full <= 32767);
    check("full METALLIC depth remains extended Q15 above unity", metallic_full > 55000);
    check("drive below zero clamps to zero", metal_core_west_coast_drive_q15(-10) == 0);
    check("drive above unity clamps to unity", metal_core_west_coast_drive_q15(40000) == full_drive);
}

int main(void)
{
    sqrt_curve();
    character_curves();
    drive_and_depth();
    printf(failures ? "metal-fm-curve: %d FAILED\n" : "metal-fm-curve: all passed\n", failures);
    return failures != 0;
}
