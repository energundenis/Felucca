/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — legacy FUN10 Base projection / Sidecar Motion split.
 *
 * Pure host-testable adapter contract. Not yet called by project.c and does
 * not access storage or Flash.
 */
#ifndef ENERGUN_PROJECT_BASE_PROJECTION_H
#define ENERGUN_PROJECT_BASE_PROJECTION_H

#include <stddef.h>
#include <stdint.h>

#define ENERGUN_PROJECT_BASE_NP          111u
#define ENERGUN_PROJECT_BASE_COMMON_NP   103u
#define ENERGUN_PROJECT_PMC_COUNT        16u
#define ENERGUN_PROJECT_RUNTIME_NP       127u
#define ENERGUN_PROJECT_PMC_FIRST_ID     103u
#define ENERGUN_PROJECT_ENGINE_FIRST_ID  119u
#define ENERGUN_PROJECT_MOTION_MAX       128u
#define ENERGUN_PROJECT_MOTION_LOCK      0x80u

typedef struct {
    uint8_t place;
    uint8_t param;
    int8_t value;
} energun_project_motion_event_t;

typedef enum {
    ENERGUN_PROJECT_MAP_OK = 0,
    ENERGUN_PROJECT_MAP_ERR_ARGUMENT,
    ENERGUN_PROJECT_MAP_ERR_VALUE,
    ENERGUN_PROJECT_MAP_ERR_COUNT,
    ENERGUN_PROJECT_MAP_ERR_ID,
    ENERGUN_PROJECT_MAP_ERR_DUPLICATE
} energun_project_map_status_t;

/* Base projection stores legacy common IDs 0..102 followed by E0..E7.
 * Runtime P_MC IDs 103..118 are deliberately omitted and defaulted on load.
 * Values use the existing FUN10 byte encoding (value + 64).
 */
energun_project_map_status_t energun_project_pack_base_params(
    uint8_t output[ENERGUN_PROJECT_BASE_NP],
    const int16_t runtime[ENERGUN_PROJECT_RUNTIME_NP]);

energun_project_map_status_t energun_project_unpack_base_params(
    int16_t runtime[ENERGUN_PROJECT_RUNTIME_NP],
    const uint8_t input[ENERGUN_PROJECT_BASE_NP],
    const int16_t defaults[ENERGUN_PROJECT_RUNTIME_NP]);

/* Split a combined runtime Motion list into Base-owned IDs and Sidecar P_MC
 * logical indices. The caller preserves the shared per-track Motion on-mask.
 */
energun_project_map_status_t energun_project_split_motion(
    const energun_project_motion_event_t *input, uint8_t input_count,
    energun_project_motion_event_t *base_output, uint8_t *base_count,
    energun_project_motion_event_t *sidecar_output, uint8_t *sidecar_count);

/* Merge Base events and Sidecar logical P_MC events back into global runtime
 * IDs. Sidecar low-seven-bit indices must be 0..15.
 */
energun_project_map_status_t energun_project_merge_motion(
    const energun_project_motion_event_t *base_input, uint8_t base_count,
    const energun_project_motion_event_t *sidecar_input, uint8_t sidecar_count,
    energun_project_motion_event_t *output, uint8_t *output_count);

#endif
