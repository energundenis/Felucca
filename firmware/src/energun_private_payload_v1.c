/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Private runtime / Sidecar payload bridge, V1.
 */
#include "energun_private_payload_v1.h"

static void private_payload_copy(uint8_t *dst, const uint8_t *src, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; ++i)
        dst[i] = src[i];
}

static uint32_t private_next_generation(const energun_private_part_v1_t *part)
{
    if (part->valid && part->schema_version == ENERGUN_PRIVATE_V1_SCHEMA)
        return part->generation + 1u;
    return 1u;
}

energun_private_status_v1_t energun_private_payload_v1_project_capture(
    const energun_private_cache_v1_t *cache,
    energun_sc_project_payload_v1_t *payload)
{
    energun_private_part_v1_t snapshots[ENERGUN_PRIVATE_V1_PARTS];
    unsigned part;
    if (cache == 0 || payload == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    for (part = 0; part < ENERGUN_PRIVATE_V1_PARTS; ++part) {
        energun_private_status_v1_t rc =
            energun_private_v1_snapshot(cache, (uint8_t)part, &snapshots[part]);
        if (rc != ENERGUN_PRIVATE_V1_OK)
            return rc;
    }
    for (part = 0; part < ENERGUN_PRIVATE_V1_PARTS; ++part)
        private_payload_copy(payload->private_values[part],
            snapshots[part].value, ENERGUN_PRIVATE_V1_KEYS);
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_payload_v1_project_restore(
    energun_private_cache_v1_t *cache,
    const energun_sc_project_payload_v1_t *payload,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS])
{
    energun_private_cache_v1_t candidate;
    unsigned part, key;
    energun_private_status_v1_t rc;
    if (cache == 0 || payload == 0 || defaults == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    rc = energun_private_v1_init(&candidate, defaults);
    if (rc != ENERGUN_PRIVATE_V1_OK)
        return rc;
    for (part = 0; part < ENERGUN_PRIVATE_V1_PARTS; ++part) {
        for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key) {
            rc = energun_private_v1_set(&candidate, (uint8_t)part,
                (uint8_t)key, payload->private_values[part][key]);
            if (rc != ENERGUN_PRIVATE_V1_OK)
                return rc;
        }
        candidate.part[part].generation =
            private_next_generation(&cache->part[part]);
        candidate.part[part].dirty_mask = 0u;
    }
    *cache = candidate;
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_payload_v1_preset_capture(
    const energun_private_cache_v1_t *cache, uint8_t part, uint8_t slot,
    energun_sc_preset_payload_v1_t *payload)
{
    energun_private_part_v1_t snapshot;
    energun_private_status_v1_t rc;
    if (cache == 0 || payload == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (slot >= ENERGUN_SC_V1_PRESET_SLOTS)
        return ENERGUN_PRIVATE_V1_ERR_PART;
    rc = energun_private_v1_snapshot(cache, part, &snapshot);
    if (rc != ENERGUN_PRIVATE_V1_OK)
        return rc;
    private_payload_copy(payload->private_values[slot], snapshot.value,
        ENERGUN_PRIVATE_V1_KEYS);
    payload->slot_bitmap[slot >> 3] |= (uint8_t)(1u << (slot & 7u));
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_payload_v1_preset_restore(
    energun_private_cache_v1_t *cache, uint8_t part, uint8_t slot,
    const energun_sc_preset_payload_v1_t *payload,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS])
{
    energun_private_cache_v1_t candidate;
    const uint8_t *values;
    unsigned key;
    energun_private_status_v1_t rc;
    if (cache == 0 || payload == 0 || defaults == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (part >= ENERGUN_PRIVATE_V1_PARTS || slot >= ENERGUN_SC_V1_PRESET_SLOTS)
        return ENERGUN_PRIVATE_V1_ERR_PART;
    values = (payload->slot_bitmap[slot >> 3] & (uint8_t)(1u << (slot & 7u)))
        ? payload->private_values[slot] : defaults;
    rc = energun_private_v1_init(&candidate, defaults);
    if (rc != ENERGUN_PRIVATE_V1_OK)
        return rc;
    for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key) {
        rc = energun_private_v1_set(&candidate, 0u, (uint8_t)key, values[key]);
        if (rc != ENERGUN_PRIVATE_V1_OK)
            return rc;
    }
    candidate.part[0].generation = private_next_generation(&cache->part[part]);
    candidate.part[0].dirty_mask = 0u;
    cache->part[part] = candidate.part[0];
    return ENERGUN_PRIVATE_V1_OK;
}
