/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — legacy FUN10 Base projection / Sidecar Motion split.
 */
#include "energun_project_base_projection.h"

_Static_assert(ENERGUN_PROJECT_BASE_COMMON_NP + 8u == ENERGUN_PROJECT_BASE_NP,
               "legacy Base projection must contain 103 common + 8 Engine EDIT");
_Static_assert(ENERGUN_PROJECT_PMC_FIRST_ID + ENERGUN_PROJECT_PMC_COUNT ==
               ENERGUN_PROJECT_ENGINE_FIRST_ID,
               "P_MC IDs must be contiguous immediately before E0");
_Static_assert(ENERGUN_PROJECT_ENGINE_FIRST_ID + 8u == ENERGUN_PROJECT_RUNTIME_NP,
               "runtime namespace must end at P_COUNT 127");

static int event_duplicate(const energun_project_motion_event_t *events,
                           uint32_t count,
                           const energun_project_motion_event_t *candidate)
{
    uint32_t i;
    for (i = 0; i < count; ++i) {
        if (events[i].place == candidate->place &&
            (events[i].param & 0x7Fu) == (candidate->param & 0x7Fu))
            return 1;
    }
    return 0;
}

static int value_valid(int8_t value)
{
    return value >= -64;
}

static int base_id_valid(uint8_t id)
{
    return id < ENERGUN_PROJECT_PMC_FIRST_ID ||
           id >= ENERGUN_PROJECT_ENGINE_FIRST_ID;
}

energun_project_map_status_t energun_project_pack_base_params(
    uint8_t output[ENERGUN_PROJECT_BASE_NP],
    const int16_t runtime[ENERGUN_PROJECT_RUNTIME_NP])
{
    uint32_t i;
    if (output == NULL || runtime == NULL)
        return ENERGUN_PROJECT_MAP_ERR_ARGUMENT;

    for (i = 0; i < ENERGUN_PROJECT_BASE_COMMON_NP; ++i)
        if (runtime[i] < -64 || runtime[i] > 127)
            return ENERGUN_PROJECT_MAP_ERR_VALUE;
    for (i = 0; i < 8u; ++i)
        if (runtime[ENERGUN_PROJECT_ENGINE_FIRST_ID + i] < -64 ||
            runtime[ENERGUN_PROJECT_ENGINE_FIRST_ID + i] > 127)
            return ENERGUN_PROJECT_MAP_ERR_VALUE;

    for (i = 0; i < ENERGUN_PROJECT_BASE_COMMON_NP; ++i)
        output[i] = (uint8_t)(runtime[i] + 64);
    for (i = 0; i < 8u; ++i)
        output[ENERGUN_PROJECT_BASE_COMMON_NP + i] =
            (uint8_t)(runtime[ENERGUN_PROJECT_ENGINE_FIRST_ID + i] + 64);
    return ENERGUN_PROJECT_MAP_OK;
}

energun_project_map_status_t energun_project_unpack_base_params(
    int16_t runtime[ENERGUN_PROJECT_RUNTIME_NP],
    const uint8_t input[ENERGUN_PROJECT_BASE_NP],
    const int16_t defaults[ENERGUN_PROJECT_RUNTIME_NP])
{
    uint32_t i;
    if (runtime == NULL || input == NULL || defaults == NULL)
        return ENERGUN_PROJECT_MAP_ERR_ARGUMENT;

    for (i = 0; i < ENERGUN_PROJECT_BASE_NP; ++i)
        if (input[i] > 191u)
            return ENERGUN_PROJECT_MAP_ERR_VALUE;
    for (i = 0; i < ENERGUN_PROJECT_RUNTIME_NP; ++i)
        runtime[i] = defaults[i];
    for (i = 0; i < ENERGUN_PROJECT_BASE_COMMON_NP; ++i)
        runtime[i] = (int16_t)input[i] - 64;
    for (i = 0; i < 8u; ++i)
        runtime[ENERGUN_PROJECT_ENGINE_FIRST_ID + i] =
            (int16_t)input[ENERGUN_PROJECT_BASE_COMMON_NP + i] - 64;
    return ENERGUN_PROJECT_MAP_OK;
}

energun_project_map_status_t energun_project_split_motion(
    const energun_project_motion_event_t *input, uint8_t input_count,
    energun_project_motion_event_t *base_output, uint8_t *base_count,
    energun_project_motion_event_t *sidecar_output, uint8_t *sidecar_count)
{
    uint32_t i, nb = 0u, ns = 0u;
    if ((input_count != 0u && input == NULL) || base_output == NULL ||
        base_count == NULL || sidecar_output == NULL || sidecar_count == NULL)
        return ENERGUN_PROJECT_MAP_ERR_ARGUMENT;
    if (input_count > ENERGUN_PROJECT_MOTION_MAX)
        return ENERGUN_PROJECT_MAP_ERR_COUNT;

    for (i = 0; i < input_count; ++i) {
        energun_project_motion_event_t event = input[i];
        uint8_t id = (uint8_t)(event.param & 0x7Fu);
        if (!value_valid(event.value))
            return ENERGUN_PROJECT_MAP_ERR_VALUE;
        if (event_duplicate(input, i, &event))
            return ENERGUN_PROJECT_MAP_ERR_DUPLICATE;
        if (id >= ENERGUN_PROJECT_PMC_FIRST_ID &&
            id < ENERGUN_PROJECT_ENGINE_FIRST_ID) {
            event.param = (uint8_t)((event.param & ENERGUN_PROJECT_MOTION_LOCK) |
                         (id - ENERGUN_PROJECT_PMC_FIRST_ID));
            sidecar_output[ns++] = event;
        } else {
            if (!base_id_valid(id))
                return ENERGUN_PROJECT_MAP_ERR_ID;
            base_output[nb++] = event;
        }
    }
    *base_count = (uint8_t)nb;
    *sidecar_count = (uint8_t)ns;
    return ENERGUN_PROJECT_MAP_OK;
}

energun_project_map_status_t energun_project_merge_motion(
    const energun_project_motion_event_t *base_input, uint8_t base_count,
    const energun_project_motion_event_t *sidecar_input, uint8_t sidecar_count,
    energun_project_motion_event_t *output, uint8_t *output_count)
{
    uint32_t i, total = (uint32_t)base_count + sidecar_count;
    if ((base_count != 0u && base_input == NULL) ||
        (sidecar_count != 0u && sidecar_input == NULL) ||
        output == NULL || output_count == NULL)
        return ENERGUN_PROJECT_MAP_ERR_ARGUMENT;
    if (total > ENERGUN_PROJECT_MOTION_MAX)
        return ENERGUN_PROJECT_MAP_ERR_COUNT;

    for (i = 0; i < base_count; ++i) {
        uint8_t id = (uint8_t)(base_input[i].param & 0x7Fu);
        if (!base_id_valid(id))
            return ENERGUN_PROJECT_MAP_ERR_ID;
        if (!value_valid(base_input[i].value))
            return ENERGUN_PROJECT_MAP_ERR_VALUE;
        if (event_duplicate(output, i, &base_input[i]))
            return ENERGUN_PROJECT_MAP_ERR_DUPLICATE;
        output[i] = base_input[i];
    }
    for (i = 0; i < sidecar_count; ++i) {
        energun_project_motion_event_t event = sidecar_input[i];
        uint8_t logical_id = (uint8_t)(event.param & 0x7Fu);
        if (logical_id >= ENERGUN_PROJECT_PMC_COUNT)
            return ENERGUN_PROJECT_MAP_ERR_ID;
        if (!value_valid(event.value))
            return ENERGUN_PROJECT_MAP_ERR_VALUE;
        event.param = (uint8_t)((event.param & ENERGUN_PROJECT_MOTION_LOCK) |
                     (ENERGUN_PROJECT_PMC_FIRST_ID + logical_id));
        if (event_duplicate(output, (uint32_t)base_count + i, &event))
            return ENERGUN_PROJECT_MAP_ERR_DUPLICATE;
        output[(uint32_t)base_count + i] = event;
    }
    *output_count = (uint8_t)total;
    return ENERGUN_PROJECT_MAP_OK;
}
