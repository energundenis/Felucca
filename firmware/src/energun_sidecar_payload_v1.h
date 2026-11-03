/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — typed Sidecar V1 logical payload codecs.
 */
#ifndef ENERGUN_SIDECAR_PAYLOAD_V1_H
#define ENERGUN_SIDECAR_PAYLOAD_V1_H

#include "energun_sidecar_v1.h"

#define ENERGUN_SC_V1_PARTS 4u
#define ENERGUN_SC_V1_PMC_PER_PART 16u
#define ENERGUN_SC_V1_PRIVATE_PER_PART 13u
#define ENERGUN_SC_V1_PRESET_SLOTS 16u
#define ENERGUN_SC_V1_PROJECT_FIXED_PAYLOAD 117u

typedef struct {
    uint8_t p_mc[ENERGUN_SC_V1_PARTS][ENERGUN_SC_V1_PMC_PER_PART];
    uint8_t private_values[ENERGUN_SC_V1_PARTS][ENERGUN_SC_V1_PRIVATE_PER_PART];
    uint8_t motion_count;
    uint8_t motion[ENERGUN_SC_V1_PROJECT_MAX_MOTION][3];
} energun_sc_project_payload_v1_t;

typedef struct {
    uint8_t slot_bitmap[2];
    uint8_t private_values[ENERGUN_SC_V1_PRESET_SLOTS][ENERGUN_SC_V1_PRIVATE_PER_PART];
} energun_sc_preset_payload_v1_t;

/* Explicit byte codecs; no C struct padding is serialized. Semantic value
 * validation remains the responsibility of energun_sc_v1_validate/encode.
 */
energun_sc_status_t energun_sc_project_payload_v1_encode(
    uint8_t *output, size_t capacity, size_t *output_length,
    const energun_sc_project_payload_v1_t *payload);
energun_sc_status_t energun_sc_project_payload_v1_decode(
    const uint8_t *input, size_t input_length,
    energun_sc_project_payload_v1_t *payload);
energun_sc_status_t energun_sc_preset_payload_v1_encode(
    uint8_t *output, size_t capacity, size_t *output_length,
    const energun_sc_preset_payload_v1_t *payload);
energun_sc_status_t energun_sc_preset_payload_v1_decode(
    const uint8_t *input, size_t input_length,
    energun_sc_preset_payload_v1_t *payload);

#endif
