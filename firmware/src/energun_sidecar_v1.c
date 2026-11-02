/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Sidecar V1 envelope and semantic validator.
 *
 * This module is deliberately storage-backend agnostic. Felucca's existing
 * storage object CRC protects the complete encoded record; no inner CRC is
 * duplicated here. No object ID or physical Flash address is defined here.
 */
#include "energun_sidecar_v1.h"

#include <string.h>

#define SC_OFF_MAGIC       0u
#define SC_OFF_FORMAT      4u
#define SC_OFF_SCHEMA      5u
#define SC_OFF_IDENTITY    6u
#define SC_OFF_INDEX       7u
#define SC_OFF_BASE_SEQ    8u
#define SC_OFF_BASE_CRC    12u
#define SC_OFF_PAYLOAD_LEN 16u
#define SC_OFF_RESERVED    18u

#define SC_PROJECT_PMC_OFF       0u
#define SC_PROJECT_PRIVATE_OFF   64u
#define SC_PROJECT_MOTION_COUNT  116u
#define SC_PROJECT_MOTION_OFF    117u

#define SC_PRESET_BITMAP_OFF     0u
#define SC_PRESET_PRIVATE_OFF    2u
#define SC_PRIVATE_COUNT         13u
#define SC_PRESET_SLOTS          16u

static uint16_t sc_get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t sc_get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void sc_put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void sc_put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

/* Decode a serialized two's-complement byte without an out-of-range unsigned
 * to signed conversion (which is implementation-defined in C).
 */
static int sc_i8_value(uint8_t value)
{
    return value <= 0x7Fu ? (int)value : (int)value - 256;
}

static int sc_identity_valid(energun_sc_identity_kind_t kind, uint8_t index)
{
    switch (kind) {
    case ENERGUN_SC_ID_PROJECT:
        return index < 4u;
    case ENERGUN_SC_ID_LIVE_AUTOSAVE:
        return index == 0u;
    case ENERGUN_SC_ID_PRESET_BANK:
        return index < 2u;
    default:
        return 0;
    }
}

/* Private V1 logical order:
 * WAVE(enum 3), FOLLOW(bool), METAL(bool), MODULATION(enum 2),
 * MOD OSC WAVE(enum 4), FM CHARACTER(enum 3), COMPLEX MODULE VOLUME(7-bit),
 * FOLD MODE(enum 3), LPG MODULE VOLUME(7-bit), ATTACK/DECAY/SUSTAIN/RELEASE
 * (7-bit canonical Felucca codes).
 *
 * Enum ordinals follow the v98 source order. FM CHARACTER accepts all three
 * defined ordinals; its source object default vs HTML selected-state mismatch
 * is intentionally not resolved by the persistence format.
 */
static int sc_private_value_valid(unsigned int key, uint8_t value)
{
    static const uint8_t enum_max[SC_PRIVATE_COUNT] = {
        2u, 1u, 1u, 1u, 3u, 2u, 127u, 2u, 127u, 127u, 127u, 127u, 127u
    };
    if (key >= SC_PRIVATE_COUNT)
        return 0;
    return value <= enum_max[key];
}

static energun_sc_status_t sc_validate_project(
    const uint8_t *payload, uint16_t length, uint16_t base_motion_count)
{
    unsigned int part, i, j;
    uint8_t motion_count;

    if (length < SC_PROJECT_MOTION_OFF)
        return ENERGUN_SC_ERR_LENGTH;
    if (length > ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD)
        return ENERGUN_SC_ERR_LENGTH;

    /* P_MC order is repeated independently for all four parts. Fifteen
     * values per part are unsigned 7-bit; FOLD SYMMETRY (index 9) is signed.
     */
    for (part = 0; part < 4u; ++part) {
        for (i = 0; i < 16u; ++i) {
            uint8_t raw = payload[SC_PROJECT_PMC_OFF + part * 16u + i];
            if (i == 9u) {
                int value = sc_i8_value(raw);
                if (value < -64 || value > 63)
                    return ENERGUN_SC_ERR_PARAMETER_VALUE;
            } else if (raw > 127u) {
                return ENERGUN_SC_ERR_PARAMETER_VALUE;
            }
        }
    }

    for (i = 0; i < 4u; ++i) {
        for (j = 0; j < SC_PRIVATE_COUNT; ++j) {
            uint8_t value = payload[SC_PROJECT_PRIVATE_OFF + i * SC_PRIVATE_COUNT + j];
            if (!sc_private_value_valid(j, value))
                return ENERGUN_SC_ERR_PRIVATE_VALUE;
        }
    }

    motion_count = payload[SC_PROJECT_MOTION_COUNT];
    if (motion_count > ENERGUN_SC_V1_PROJECT_MAX_MOTION ||
        base_motion_count > ENERGUN_SC_V1_PROJECT_MAX_MOTION ||
        (uint16_t)motion_count + base_motion_count > ENERGUN_SC_V1_PROJECT_MAX_MOTION)
        return ENERGUN_SC_ERR_MOTION_COUNT;
    if ((uint32_t)SC_PROJECT_MOTION_OFF + (uint32_t)motion_count * 3u != length)
        return ENERGUN_SC_ERR_LENGTH;

    for (i = 0; i < motion_count; ++i) {
        const uint8_t *event = payload + SC_PROJECT_MOTION_OFF + i * 3u;
        uint8_t place = event[0];
        uint8_t param = event[1];
        int value = sc_i8_value(event[2]);
        (void)place; /* all 256 encodings map to track 0..3 / step 0..63 */
        (void)param; /* numeric P_MC IDs remain unassigned at this stage */
        if (value < -64 || value > 127)
            return ENERGUN_SC_ERR_MOTION_EVENT;
        for (j = 0; j < i; ++j) {
            const uint8_t *prior = payload + SC_PROJECT_MOTION_OFF + j * 3u;
            if (prior[0] == place && (prior[1] & 0x7Fu) == (param & 0x7Fu))
                return ENERGUN_SC_ERR_MOTION_EVENT;
        }
    }
    return ENERGUN_SC_OK;
}

static energun_sc_status_t sc_validate_preset(const uint8_t *payload, uint16_t length)
{
    unsigned int slot, key;
    if (length != ENERGUN_SC_V1_PRESET_PAYLOAD)
        return ENERGUN_SC_ERR_LENGTH;

    for (slot = 0; slot < SC_PRESET_SLOTS; ++slot) {
        unsigned int present =
            (payload[SC_PRESET_BITMAP_OFF + (slot >> 3)] >> (slot & 7u)) & 1u;
        for (key = 0; key < SC_PRIVATE_COUNT; ++key) {
            uint8_t value = payload[SC_PRESET_PRIVATE_OFF + slot * SC_PRIVATE_COUNT + key];
            if (!present) {
                if (value != 0u)
                    return ENERGUN_SC_ERR_SLOT_BITMAP;
            } else if (!sc_private_value_valid(key, value)) {
                return ENERGUN_SC_ERR_PRIVATE_VALUE;
            }
        }
    }
    return ENERGUN_SC_OK;
}

static energun_sc_status_t sc_validate_payload(
    energun_sc_identity_kind_t kind, const uint8_t *payload,
    uint16_t length, uint16_t base_motion_count)
{
    if (payload == NULL)
        return ENERGUN_SC_ERR_ARGUMENT;
    if (kind == ENERGUN_SC_ID_PROJECT || kind == ENERGUN_SC_ID_LIVE_AUTOSAVE)
        return sc_validate_project(payload, length, base_motion_count);
    if (kind == ENERGUN_SC_ID_PRESET_BANK) {
        if (base_motion_count != 0u)
            return ENERGUN_SC_ERR_ARGUMENT;
        return sc_validate_preset(payload, length);
    }
    return ENERGUN_SC_ERR_IDENTITY;
}

energun_sc_status_t energun_sc_v1_validate(
    const uint8_t *record, size_t record_length,
    energun_sc_identity_kind_t expected_kind, uint8_t expected_index,
    uint32_t expected_base_sequence, uint32_t expected_base_crc32,
    uint16_t base_motion_count,
    energun_sc_view_t *view)
{
    uint16_t payload_length;
    energun_sc_identity_kind_t kind;
    uint8_t index;
    energun_sc_status_t status;

    if (view != NULL)
        memset(view, 0, sizeof *view);
    if (record == NULL || view == NULL || record_length < ENERGUN_SC_V1_HEADER_SIZE)
        return ENERGUN_SC_ERR_ARGUMENT;
    if (memcmp(record + SC_OFF_MAGIC, "EGSC", 4u) != 0)
        return ENERGUN_SC_ERR_MAGIC;
    if (record[SC_OFF_FORMAT] != ENERGUN_SC_V1_FORMAT_VERSION)
        return ENERGUN_SC_ERR_FORMAT_VERSION;
    if (record[SC_OFF_SCHEMA] != ENERGUN_SC_V1_SCHEMA_VERSION)
        return ENERGUN_SC_ERR_SCHEMA_VERSION;

    kind = (energun_sc_identity_kind_t)record[SC_OFF_IDENTITY];
    index = record[SC_OFF_INDEX];
    if (!sc_identity_valid(kind, index) ||
        kind != expected_kind || index != expected_index ||
        !sc_identity_valid(expected_kind, expected_index))
        return ENERGUN_SC_ERR_IDENTITY;
    if (sc_get_u32(record + SC_OFF_BASE_SEQ) != expected_base_sequence ||
        sc_get_u32(record + SC_OFF_BASE_CRC) != expected_base_crc32)
        return ENERGUN_SC_ERR_BASE_GENERATION;
    if (sc_get_u16(record + SC_OFF_RESERVED) != 0u)
        return ENERGUN_SC_ERR_RESERVED;

    payload_length = sc_get_u16(record + SC_OFF_PAYLOAD_LEN);
    if ((size_t)payload_length + ENERGUN_SC_V1_HEADER_SIZE != record_length)
        return ENERGUN_SC_ERR_LENGTH;

    status = sc_validate_payload(kind, record + ENERGUN_SC_V1_HEADER_SIZE,
                                 payload_length, base_motion_count);
    if (status != ENERGUN_SC_OK)
        return status;

    view->identity_kind = kind;
    view->identity_index = index;
    view->base_sequence = expected_base_sequence;
    view->base_payload_crc32 = expected_base_crc32;
    view->payload = record + ENERGUN_SC_V1_HEADER_SIZE;
    view->payload_length = payload_length;
    return ENERGUN_SC_OK;
}

energun_sc_status_t energun_sc_v1_encode(
    uint8_t *output, size_t output_capacity, size_t *output_length,
    energun_sc_identity_kind_t identity_kind, uint8_t identity_index,
    uint32_t base_sequence, uint32_t base_payload_crc32,
    const uint8_t *payload, uint16_t payload_length,
    uint16_t base_motion_count)
{
    energun_sc_status_t status;
    size_t total;

    if (output_length != NULL)
        *output_length = 0u;
    if (output == NULL || output_length == NULL || payload == NULL)
        return ENERGUN_SC_ERR_ARGUMENT;
    if (!sc_identity_valid(identity_kind, identity_index))
        return ENERGUN_SC_ERR_IDENTITY;
    status = sc_validate_payload(identity_kind, payload, payload_length, base_motion_count);
    if (status != ENERGUN_SC_OK)
        return status;
    total = ENERGUN_SC_V1_HEADER_SIZE + (size_t)payload_length;
    if (output_capacity < total)
        return ENERGUN_SC_ERR_LENGTH;

    /* Output is untouched until every validation and capacity check succeeds. */
    memset(output, 0, ENERGUN_SC_V1_HEADER_SIZE);
    memcpy(output + SC_OFF_MAGIC, "EGSC", 4u);
    output[SC_OFF_FORMAT] = ENERGUN_SC_V1_FORMAT_VERSION;
    output[SC_OFF_SCHEMA] = ENERGUN_SC_V1_SCHEMA_VERSION;
    output[SC_OFF_IDENTITY] = (uint8_t)identity_kind;
    output[SC_OFF_INDEX] = identity_index;
    sc_put_u32(output + SC_OFF_BASE_SEQ, base_sequence);
    sc_put_u32(output + SC_OFF_BASE_CRC, base_payload_crc32);
    sc_put_u16(output + SC_OFF_PAYLOAD_LEN, payload_length);
    sc_put_u16(output + SC_OFF_RESERVED, 0u);
    memcpy(output + ENERGUN_SC_V1_HEADER_SIZE, payload, payload_length);
    *output_length = total;
    return ENERGUN_SC_OK;
}

const char *energun_sc_v1_status_string(energun_sc_status_t status)
{
    static const char *const names[] = {
        "ok", "argument", "magic", "format-version", "schema-version",
        "identity", "base-generation", "reserved", "length",
        "parameter-value", "private-value", "motion-count",
        "motion-event", "slot-bitmap"
    };
    if ((unsigned int)status >= sizeof names / sizeof names[0])
        return "unknown";
    return names[(unsigned int)status];
}
