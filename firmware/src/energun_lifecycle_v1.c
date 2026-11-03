/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — project/live lifecycle adapter for Base + Sidecar V1.
 */
#include "energun_lifecycle_v1.h"

static int lifecycle_valid(const energun_lifecycle_v1_t *lifecycle)
{
    return lifecycle != NULL && lifecycle->ops != NULL &&
           lifecycle->workspace != NULL &&
           lifecycle->canonical_sidecar_defaults != NULL &&
           lifecycle->canonical_sidecar_defaults_length > 0u;
}

static energun_lifecycle_v1_status_t lifecycle_commit(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    const uint8_t *base_payload, size_t base_length,
    const uint8_t *sidecar_payload, uint16_t sidecar_length)
{
    int rc;
    if (!lifecycle_valid(lifecycle) || base_payload == NULL ||
        base_length == 0u || sidecar_payload == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    rc = energun_pair_v1_commit(lifecycle->ops, lifecycle->storage_context,
        lifecycle->workspace, object, kind, index, base_payload, base_length,
        sidecar_payload, sidecar_length, lifecycle->base_motion_count);
    return rc == ENERGUN_PAIR_V1_OK ? ENERGUN_LIFECYCLE_V1_OK :
                                      ENERGUN_LIFECYCLE_V1_ERR_PAIR;
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_save(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    const uint8_t *base_payload, size_t base_length,
    const uint8_t *sidecar_payload, uint16_t sidecar_length)
{
    return lifecycle_commit(lifecycle, object, kind, index,
        base_payload, base_length, sidecar_payload, sidecar_length);
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_load(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    uint8_t *sidecar_payload, size_t sidecar_capacity, size_t *sidecar_length,
    energun_pair_v1_base_meta_t *base_meta)
{
    int rc;
    if (!lifecycle_valid(lifecycle) || base_payload == NULL ||
        base_length == NULL || sidecar_payload == NULL ||
        sidecar_length == NULL || base_meta == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    *base_length = 0u;
    *sidecar_length = 0u;
    rc = energun_pair_v1_load_compat(lifecycle->ops, lifecycle->storage_context,
        lifecycle->workspace, object, kind, index, base_payload, base_capacity,
        base_length, sidecar_payload, sidecar_capacity, sidecar_length,
        lifecycle->base_motion_count, base_meta);
    if (rc == ENERGUN_PAIR_V1_LEGACY_FALLBACK) {
        if (lifecycle->canonical_sidecar_defaults_length > sidecar_capacity) {
            *base_length = 0u;
            return ENERGUN_LIFECYCLE_V1_ERR_CAPACITY;
        }
        {
            uint16_t i;
            for (i = 0u; i < lifecycle->canonical_sidecar_defaults_length; ++i)
                sidecar_payload[i] = lifecycle->canonical_sidecar_defaults[i];
        }
        *sidecar_length = lifecycle->canonical_sidecar_defaults_length;
        return ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS;
    }
    return rc == ENERGUN_PAIR_V1_OK ? ENERGUN_LIFECYCLE_V1_OK :
                                      ENERGUN_LIFECYCLE_V1_ERR_PAIR;
}


static energun_lifecycle_v1_status_t lifecycle_save_typed(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    const uint8_t *base_payload, size_t base_length,
    const energun_sc_project_payload_v1_t *sidecar)
{
    size_t encoded_length = 0u;
    energun_sc_status_t codec_rc;
    if (!lifecycle_valid(lifecycle) || lifecycle->sidecar_scratch == NULL ||
        sidecar == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    codec_rc = energun_sc_project_payload_v1_encode(lifecycle->sidecar_scratch,
        lifecycle->sidecar_scratch_capacity, &encoded_length, sidecar);
    if (codec_rc != ENERGUN_SC_OK)
        return codec_rc == ENERGUN_SC_ERR_LENGTH ?
            ENERGUN_LIFECYCLE_V1_ERR_CAPACITY : ENERGUN_LIFECYCLE_V1_ERR_PAIR;
    if (encoded_length > 0xFFFFu)
        return ENERGUN_LIFECYCLE_V1_ERR_CAPACITY;
    return lifecycle_commit(lifecycle, object, kind, index,
        base_payload, base_length, lifecycle->sidecar_scratch,
        (uint16_t)encoded_length);
}

static energun_lifecycle_v1_status_t lifecycle_load_typed(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    energun_sc_project_payload_v1_t *sidecar,
    energun_pair_v1_base_meta_t *base_meta)
{
    size_t encoded_length = 0u;
    energun_lifecycle_v1_status_t status;
    energun_sc_status_t codec_rc;
    if (!lifecycle_valid(lifecycle) || lifecycle->sidecar_scratch == NULL ||
        sidecar == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    status = energun_lifecycle_v1_load(lifecycle, object, kind, index,
        base_payload, base_capacity, base_length,
        lifecycle->sidecar_scratch, lifecycle->sidecar_scratch_capacity,
        &encoded_length, base_meta);
    if (status != ENERGUN_LIFECYCLE_V1_OK &&
        status != ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS)
        return status;
    codec_rc = energun_sc_project_payload_v1_decode(lifecycle->sidecar_scratch,
        encoded_length, sidecar);
    if (codec_rc != ENERGUN_SC_OK) {
        if (base_length != NULL) *base_length = 0u;
        return ENERGUN_LIFECYCLE_V1_ERR_PAIR;
    }
    return status;
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_save_project(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t index,
    const uint8_t *base_payload, size_t base_length,
    const energun_sc_project_payload_v1_t *sidecar)
{
    return lifecycle_save_typed(lifecycle, object, ENERGUN_SC_ID_PROJECT,
        index, base_payload, base_length, sidecar);
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_load_project(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t index,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    energun_sc_project_payload_v1_t *sidecar,
    energun_pair_v1_base_meta_t *base_meta)
{
    return lifecycle_load_typed(lifecycle, object, ENERGUN_SC_ID_PROJECT,
        index, base_payload, base_capacity, base_length, sidecar, base_meta);
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_save_live(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object,
    const uint8_t *base_payload, size_t base_length,
    const energun_sc_project_payload_v1_t *sidecar)
{
    return lifecycle_save_typed(lifecycle, object, ENERGUN_SC_ID_LIVE_AUTOSAVE,
        0u, base_payload, base_length, sidecar);
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_load_live(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    energun_sc_project_payload_v1_t *sidecar,
    energun_pair_v1_base_meta_t *base_meta)
{
    return lifecycle_load_typed(lifecycle, object, ENERGUN_SC_ID_LIVE_AUTOSAVE,
        0u, base_payload, base_capacity, base_length, sidecar, base_meta);
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_copy_stored(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t source_object, energun_sc_identity_kind_t source_kind,
    uint8_t source_index,
    uint32_t destination_object, energun_sc_identity_kind_t destination_kind,
    uint8_t destination_index,
    const uint8_t *destination_base, size_t destination_base_length)
{
    size_t source_base_length = 0u, source_sidecar_length = 0u;
    energun_pair_v1_base_meta_t source_meta;
    energun_lifecycle_v1_status_t load_status;
    if (!lifecycle_valid(lifecycle) || lifecycle->base_scratch == NULL ||
        lifecycle->base_scratch_capacity == 0u || lifecycle->sidecar_scratch == NULL ||
        lifecycle->sidecar_scratch_capacity == 0u || destination_base == NULL ||
        destination_base_length == 0u)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    load_status = energun_lifecycle_v1_load(lifecycle,
        source_object, source_kind, source_index,
        lifecycle->base_scratch, lifecycle->base_scratch_capacity, &source_base_length,
        lifecycle->sidecar_scratch, lifecycle->sidecar_scratch_capacity, &source_sidecar_length,
        &source_meta);
    if (load_status != ENERGUN_LIFECYCLE_V1_OK &&
        load_status != ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS)
        return load_status;
    return lifecycle_commit(lifecycle, destination_object,
        destination_kind, destination_index, destination_base,
        destination_base_length, lifecycle->sidecar_scratch, (uint16_t)source_sidecar_length);
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_erase(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object)
{
    int rc;
    if (!lifecycle_valid(lifecycle))
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    rc = energun_pair_v1_erase(lifecycle->ops, lifecycle->storage_context, object);
    return rc == ENERGUN_PAIR_V1_OK ? ENERGUN_LIFECYCLE_V1_OK :
                                      ENERGUN_LIFECYCLE_V1_ERR_PAIR;
}

energun_lifecycle_v1_status_t energun_lifecycle_v1_autosave(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, const uint8_t *base_payload, size_t base_length,
    const uint8_t *sidecar_payload, uint16_t sidecar_length)
{
    if (!lifecycle_valid(lifecycle))
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    if (lifecycle->write_guard != NULL &&
        lifecycle->write_guard(lifecycle->guard_context) != 0)
        return ENERGUN_LIFECYCLE_V1_ERR_GUARD;
    return lifecycle_commit(lifecycle, object, ENERGUN_SC_ID_LIVE_AUTOSAVE,
        0u, base_payload, base_length, sidecar_payload, sidecar_length);
}
