/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — typed Sidecar V1 logical payload codecs.
 */
#include "energun_sidecar_payload_v1.h"

static void payload_copy(uint8_t *dst, const uint8_t *src, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i)
        dst[i] = src[i];
}

energun_sc_status_t energun_sc_project_payload_v1_encode(
    uint8_t *output, size_t capacity, size_t *output_length,
    const energun_sc_project_payload_v1_t *payload)
{
    size_t length, i;
    if (output_length != NULL)
        *output_length = 0u;
    if (output == NULL || output_length == NULL || payload == NULL)
        return ENERGUN_SC_ERR_ARGUMENT;
    if (payload->motion_count > ENERGUN_SC_V1_PROJECT_MAX_MOTION)
        return ENERGUN_SC_ERR_MOTION_COUNT;
    length = ENERGUN_SC_V1_PROJECT_FIXED_PAYLOAD +
             (size_t)payload->motion_count * 3u;
    if (length > ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD || capacity < length)
        return ENERGUN_SC_ERR_LENGTH;

    payload_copy(output, &payload->p_mc[0][0], 64u);
    payload_copy(output + 64u, &payload->private_values[0][0], 52u);
    output[116] = payload->motion_count;
    for (i = 0; i < (size_t)payload->motion_count * 3u; ++i)
        output[117u + i] = ((const uint8_t *)payload->motion)[i];
    *output_length = length;
    return ENERGUN_SC_OK;
}

energun_sc_status_t energun_sc_project_payload_v1_decode(
    const uint8_t *input, size_t input_length,
    energun_sc_project_payload_v1_t *payload)
{
    size_t expected, motion_bytes;
    if (input == NULL || payload == NULL)
        return ENERGUN_SC_ERR_ARGUMENT;
    if (input_length < ENERGUN_SC_V1_PROJECT_FIXED_PAYLOAD ||
        input_length > ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD)
        return ENERGUN_SC_ERR_LENGTH;
    if (input[116] > ENERGUN_SC_V1_PROJECT_MAX_MOTION)
        return ENERGUN_SC_ERR_MOTION_COUNT;
    motion_bytes = (size_t)input[116] * 3u;
    expected = ENERGUN_SC_V1_PROJECT_FIXED_PAYLOAD + motion_bytes;
    if (input_length != expected)
        return ENERGUN_SC_ERR_LENGTH;

    payload_copy(&payload->p_mc[0][0], input, 64u);
    payload_copy(&payload->private_values[0][0], input + 64u, 52u);
    payload->motion_count = input[116];
    payload_copy((uint8_t *)payload->motion, input + 117u, motion_bytes);
    while (motion_bytes < sizeof payload->motion)
        ((uint8_t *)payload->motion)[motion_bytes++] = 0u;
    return ENERGUN_SC_OK;
}

energun_sc_status_t energun_sc_preset_payload_v1_encode(
    uint8_t *output, size_t capacity, size_t *output_length,
    const energun_sc_preset_payload_v1_t *payload)
{
    if (output_length != NULL)
        *output_length = 0u;
    if (output == NULL || output_length == NULL || payload == NULL)
        return ENERGUN_SC_ERR_ARGUMENT;
    if (capacity < ENERGUN_SC_V1_PRESET_PAYLOAD)
        return ENERGUN_SC_ERR_LENGTH;
    payload_copy(output, payload->slot_bitmap, 2u);
    payload_copy(output + 2u, &payload->private_values[0][0], 208u);
    *output_length = ENERGUN_SC_V1_PRESET_PAYLOAD;
    return ENERGUN_SC_OK;
}

energun_sc_status_t energun_sc_preset_payload_v1_decode(
    const uint8_t *input, size_t input_length,
    energun_sc_preset_payload_v1_t *payload)
{
    if (input == NULL || payload == NULL)
        return ENERGUN_SC_ERR_ARGUMENT;
    if (input_length != ENERGUN_SC_V1_PRESET_PAYLOAD)
        return ENERGUN_SC_ERR_LENGTH;
    payload_copy(payload->slot_bitmap, input, 2u);
    payload_copy(&payload->private_values[0][0], input + 2u, 208u);
    return ENERGUN_SC_OK;
}
