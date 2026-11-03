/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — typed four-part Private runtime cache, V1.
 */
#include "energun_private_v1.h"

static const uint8_t private_max[ENERGUN_PRIVATE_V1_KEYS] = {
    2u, 1u, 1u, 1u, 3u, 2u, 127u, 2u, 127u, 127u, 127u, 127u, 127u
};

static int private_values_valid(const uint8_t *values)
{
    unsigned i;
    if (values == 0)
        return 0;
    for (i = 0; i < ENERGUN_PRIVATE_V1_KEYS; ++i)
        if (values[i] > private_max[i])
            return 0;
    return 1;
}

energun_private_status_v1_t energun_private_v1_init(
    energun_private_cache_v1_t *cache,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS])
{
    unsigned part, key;
    if (cache == 0 || defaults == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (!private_values_valid(defaults))
        return ENERGUN_PRIVATE_V1_ERR_VALUE;
    for (part = 0; part < ENERGUN_PRIVATE_V1_PARTS; ++part) {
        cache->part[part].schema_version = ENERGUN_PRIVATE_V1_SCHEMA;
        cache->part[part].generation = 0u;
        cache->part[part].dirty_mask = 0u;
        cache->part[part].valid = 1u;
        for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key)
            cache->part[part].value[key] = defaults[key];
    }
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_v1_defaults(
    energun_private_cache_v1_t *cache, uint8_t part,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS])
{
    unsigned key;
    uint32_t generation;
    if (cache == 0 || defaults == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (part >= ENERGUN_PRIVATE_V1_PARTS)
        return ENERGUN_PRIVATE_V1_ERR_PART;
    if (!private_values_valid(defaults))
        return ENERGUN_PRIVATE_V1_ERR_VALUE;
    generation = cache->part[part].generation + 1u;
    cache->part[part].schema_version = ENERGUN_PRIVATE_V1_SCHEMA;
    cache->part[part].generation = generation;
    cache->part[part].dirty_mask = 0u;
    cache->part[part].valid = 1u;
    for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key)
        cache->part[part].value[key] = defaults[key];
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_v1_get(
    const energun_private_cache_v1_t *cache, uint8_t part,
    uint8_t key, uint8_t *value)
{
    if (cache == 0 || value == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (part >= ENERGUN_PRIVATE_V1_PARTS)
        return ENERGUN_PRIVATE_V1_ERR_PART;
    if (key >= ENERGUN_PRIVATE_V1_KEYS)
        return ENERGUN_PRIVATE_V1_ERR_KEY;
    if (!cache->part[part].valid ||
        cache->part[part].schema_version != ENERGUN_PRIVATE_V1_SCHEMA)
        return ENERGUN_PRIVATE_V1_ERR_SCHEMA;
    *value = cache->part[part].value[key];
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_v1_set(
    energun_private_cache_v1_t *cache, uint8_t part,
    uint8_t key, uint8_t value)
{
    if (cache == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (part >= ENERGUN_PRIVATE_V1_PARTS)
        return ENERGUN_PRIVATE_V1_ERR_PART;
    if (key >= ENERGUN_PRIVATE_V1_KEYS)
        return ENERGUN_PRIVATE_V1_ERR_KEY;
    if (value > private_max[key])
        return ENERGUN_PRIVATE_V1_ERR_VALUE;
    if (!cache->part[part].valid ||
        cache->part[part].schema_version != ENERGUN_PRIVATE_V1_SCHEMA)
        return ENERGUN_PRIVATE_V1_ERR_SCHEMA;
    if (cache->part[part].value[key] != value) {
        cache->part[part].value[key] = value;
        cache->part[part].generation++;
        cache->part[part].dirty_mask |= (uint16_t)(1u << key);
    }
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_v1_snapshot(
    const energun_private_cache_v1_t *cache, uint8_t part,
    energun_private_part_v1_t *snapshot)
{
    if (cache == 0 || snapshot == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (part >= ENERGUN_PRIVATE_V1_PARTS)
        return ENERGUN_PRIVATE_V1_ERR_PART;
    if (!cache->part[part].valid ||
        cache->part[part].schema_version != ENERGUN_PRIVATE_V1_SCHEMA)
        return ENERGUN_PRIVATE_V1_ERR_SCHEMA;
    *snapshot = cache->part[part];
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_v1_replace(
    energun_private_cache_v1_t *cache, uint8_t part,
    const energun_private_part_v1_t *snapshot,
    uint32_t expected_generation)
{
    unsigned key;
    uint16_t changed = 0u;
    energun_private_part_v1_t *current;
    if (cache == 0 || snapshot == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (part >= ENERGUN_PRIVATE_V1_PARTS)
        return ENERGUN_PRIVATE_V1_ERR_PART;
    current = &cache->part[part];
    if (!current->valid || current->schema_version != ENERGUN_PRIVATE_V1_SCHEMA)
        return ENERGUN_PRIVATE_V1_ERR_SCHEMA;
    if (current->generation != expected_generation)
        return ENERGUN_PRIVATE_V1_ERR_STALE;
    if (!snapshot->valid || snapshot->schema_version != ENERGUN_PRIVATE_V1_SCHEMA)
        return ENERGUN_PRIVATE_V1_ERR_SCHEMA;
    if (!private_values_valid(snapshot->value))
        return ENERGUN_PRIVATE_V1_ERR_VALUE;
    for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key)
        if (current->value[key] != snapshot->value[key])
            changed |= (uint16_t)(1u << key);
    if (changed != 0u) {
        for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key)
            current->value[key] = snapshot->value[key];
        current->generation++;
        current->dirty_mask |= changed;
    }
    return ENERGUN_PRIVATE_V1_OK;
}

energun_private_status_v1_t energun_private_v1_mark_persisted(
    energun_private_cache_v1_t *cache, uint8_t part,
    uint32_t saved_generation)
{
    if (cache == 0)
        return ENERGUN_PRIVATE_V1_ERR_ARGUMENT;
    if (part >= ENERGUN_PRIVATE_V1_PARTS)
        return ENERGUN_PRIVATE_V1_ERR_PART;
    if (!cache->part[part].valid ||
        cache->part[part].schema_version != ENERGUN_PRIVATE_V1_SCHEMA)
        return ENERGUN_PRIVATE_V1_ERR_SCHEMA;
    if (cache->part[part].generation != saved_generation)
        return ENERGUN_PRIVATE_V1_ERR_STALE;
    cache->part[part].dirty_mask = 0u;
    return ENERGUN_PRIVATE_V1_OK;
}
