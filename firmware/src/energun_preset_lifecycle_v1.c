/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — user-preset bank Sidecar lifecycle, V1.
 */
#include "energun_preset_lifecycle_v1.h"

static int preset_args_valid(const energun_lifecycle_v1_t *lifecycle,
                             uint8_t bank)
{
    return lifecycle != NULL && lifecycle->ops != NULL &&
           lifecycle->workspace != NULL && lifecycle->canonical_sidecar_defaults != NULL &&
           lifecycle->canonical_sidecar_defaults_length == ENERGUN_SC_V1_PRESET_PAYLOAD &&
           lifecycle->sidecar_scratch != NULL &&
           lifecycle->sidecar_scratch_capacity >= ENERGUN_SC_V1_PRESET_PAYLOAD &&
           lifecycle->base_scratch != NULL && lifecycle->base_scratch_capacity > 0u &&
           bank < 2u;
}

static energun_lifecycle_v1_status_t preset_load_internal(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    energun_sc_preset_payload_v1_t *sidecar)
{
    size_t base_length = 0u, sidecar_length = 0u;
    energun_pair_v1_base_meta_t meta;
    energun_lifecycle_v1_status_t status;
    energun_sc_status_t codec_status;
    if (!preset_args_valid(lifecycle, bank) || sidecar == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    status = energun_lifecycle_v1_load(lifecycle, object,
        ENERGUN_SC_ID_PRESET_BANK, bank,
        lifecycle->base_scratch, lifecycle->base_scratch_capacity, &base_length,
        lifecycle->sidecar_scratch, lifecycle->sidecar_scratch_capacity,
        &sidecar_length, &meta);
    if (status != ENERGUN_LIFECYCLE_V1_OK &&
        status != ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS)
        return status;
    codec_status = energun_sc_preset_payload_v1_decode(
        lifecycle->sidecar_scratch, sidecar_length, sidecar);
    if (codec_status != ENERGUN_SC_OK)
        return ENERGUN_LIFECYCLE_V1_ERR_PAIR;
    return status;
}

static energun_lifecycle_v1_status_t preset_commit(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    const uint8_t *base_payload, size_t base_length,
    const energun_sc_preset_payload_v1_t *sidecar)
{
    size_t encoded_length = 0u;
    energun_sc_status_t codec_status;
    if (!preset_args_valid(lifecycle, bank) || base_payload == NULL ||
        base_length == 0u || sidecar == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    codec_status = energun_sc_preset_payload_v1_encode(
        lifecycle->sidecar_scratch, lifecycle->sidecar_scratch_capacity,
        &encoded_length, sidecar);
    if (codec_status != ENERGUN_SC_OK)
        return codec_status == ENERGUN_SC_ERR_LENGTH ?
            ENERGUN_LIFECYCLE_V1_ERR_CAPACITY : ENERGUN_LIFECYCLE_V1_ERR_PAIR;
    return energun_lifecycle_v1_save(lifecycle, object,
        ENERGUN_SC_ID_PRESET_BANK, bank, base_payload, base_length,
        lifecycle->sidecar_scratch, (uint16_t)encoded_length);
}

energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_save_bank(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    const uint8_t *base_payload, size_t base_length,
    const energun_sc_preset_payload_v1_t *sidecar)
{
    return preset_commit(lifecycle, object, bank, base_payload, base_length, sidecar);
}

energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_load_bank(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    energun_sc_preset_payload_v1_t *sidecar,
    energun_pair_v1_base_meta_t *base_meta)
{
    size_t sidecar_length = 0u;
    energun_lifecycle_v1_status_t status;
    energun_sc_status_t codec_status;
    if (!preset_args_valid(lifecycle, bank) || base_payload == NULL ||
        base_length == NULL || sidecar == NULL || base_meta == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    status = energun_lifecycle_v1_load(lifecycle, object,
        ENERGUN_SC_ID_PRESET_BANK, bank, base_payload, base_capacity, base_length,
        lifecycle->sidecar_scratch, lifecycle->sidecar_scratch_capacity,
        &sidecar_length, base_meta);
    if (status != ENERGUN_LIFECYCLE_V1_OK &&
        status != ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS)
        return status;
    codec_status = energun_sc_preset_payload_v1_decode(
        lifecycle->sidecar_scratch, sidecar_length, sidecar);
    if (codec_status != ENERGUN_SC_OK) {
        *base_length = 0u;
        return ENERGUN_LIFECYCLE_V1_ERR_PAIR;
    }
    return status;
}

energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_capture_slot(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    uint8_t part, uint8_t slot,
    const uint8_t *new_base_bank, size_t new_base_bank_length,
    const energun_private_cache_v1_t *private_cache)
{
    energun_sc_preset_payload_v1_t payload;
    energun_lifecycle_v1_status_t status;
    energun_private_status_v1_t private_status;
    if (!preset_args_valid(lifecycle, bank) || part >= ENERGUN_PRIVATE_V1_PARTS ||
        slot >= ENERGUN_SC_V1_PRESET_SLOTS || new_base_bank == NULL ||
        new_base_bank_length == 0u || private_cache == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    status = preset_load_internal(lifecycle, object, bank, &payload);
    if (status != ENERGUN_LIFECYCLE_V1_OK &&
        status != ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS)
        return status;
    private_status = energun_private_payload_v1_preset_capture(
        private_cache, part, slot, &payload);
    if (private_status != ENERGUN_PRIVATE_V1_OK)
        return ENERGUN_LIFECYCLE_V1_ERR_PAIR;
    return preset_commit(lifecycle, object, bank, new_base_bank,
        new_base_bank_length, &payload);
}

energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_erase_slot(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    uint8_t slot,
    const uint8_t *new_base_bank, size_t new_base_bank_length)
{
    energun_sc_preset_payload_v1_t payload;
    energun_lifecycle_v1_status_t status;
    uint8_t mask;
    unsigned key;
    if (!preset_args_valid(lifecycle, bank) ||
        slot >= ENERGUN_SC_V1_PRESET_SLOTS || new_base_bank == NULL ||
        new_base_bank_length == 0u)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    status = preset_load_internal(lifecycle, object, bank, &payload);
    if (status != ENERGUN_LIFECYCLE_V1_OK &&
        status != ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS)
        return status;
    mask = (uint8_t)(1u << (slot & 7u));
    payload.slot_bitmap[slot >> 3] &= (uint8_t)~mask;
    for (key = 0u; key < ENERGUN_SC_V1_PRIVATE_PER_PART; ++key)
        payload.private_values[slot][key] = 0u;
    return preset_commit(lifecycle, object, bank, new_base_bank,
        new_base_bank_length, &payload);
}

energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_restore_slot(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    uint8_t part, uint8_t slot,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    energun_private_cache_v1_t *private_cache,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS],
    energun_pair_v1_base_meta_t *base_meta)
{
    energun_sc_preset_payload_v1_t payload;
    energun_lifecycle_v1_status_t status;
    energun_private_status_v1_t private_status;
    if (!preset_args_valid(lifecycle, bank) || part >= ENERGUN_PRIVATE_V1_PARTS ||
        slot >= ENERGUN_SC_V1_PRESET_SLOTS || private_cache == NULL || defaults == NULL)
        return ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT;
    status = energun_preset_lifecycle_v1_load_bank(lifecycle, object, bank,
        base_payload, base_capacity, base_length, &payload, base_meta);
    if (status != ENERGUN_LIFECYCLE_V1_OK &&
        status != ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS)
        return status;
    private_status = energun_private_payload_v1_preset_restore(
        private_cache, part, slot, &payload, defaults);
    if (private_status != ENERGUN_PRIVATE_V1_OK) {
        if (base_length != NULL) *base_length = 0u;
        return ENERGUN_LIFECYCLE_V1_ERR_PAIR;
    }
    return status;
}
