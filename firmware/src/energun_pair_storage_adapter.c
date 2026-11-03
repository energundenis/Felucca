/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Felucca storage adapter for pair transactions.
 *
 * This file is included by felucca.c after storage.c, so it can bind to the
 * existing static st_* primitives without exporting or duplicating them.
 * The Sidecar backend remains address-free and is supplied by a qualified
 * backend; no production XDATA sector is allocated by this module.
 */
#include "energun_pair_storage_adapter.h"

static int energun_pair_seq_newer(uint32_t a, uint32_t b)
{
    return a != b && (uint32_t)(a - b) < 0x80000000u;
}

static int energun_pair_sidecar_read(
    energun_pair_storage_adapter_t *adapter, uint8_t copy,
    size_t *length, uint32_t *sequence)
{
    if (adapter == 0 || adapter->sidecar_ops == 0 ||
        adapter->sidecar_ops->read_copy == 0 || copy > 1u ||
        length == 0 || sequence == 0)
        return -1;
    *length = 0u;
    *sequence = 0u;
    return adapter->sidecar_ops->read_copy(
        adapter->sidecar_context, adapter->identity_kind,
        adapter->identity_index, copy, adapter->scratch_record,
        sizeof adapter->scratch_record, length, sequence);
}

/* Find the newest Base generation that has at least one exact matching,
 * semantically valid Sidecar copy. st_read_copy_meta validates the enclosing
 * Felucca header and payload CRC for each explicitly requested A/B copy.
 */
static int energun_pair_find_protected_pair(
    energun_pair_storage_adapter_t *adapter,
    const energun_pair_base_meta_t base[2], const uint8_t valid[2])
{
    int best_base = -1, best_sidecar = -1;
    uint32_t best_sequence = 0u;
    unsigned int b, s;

    for (b = 0; b < 2u; ++b) {
        if (!valid[b])
            continue;
        for (s = 0; s < 2u; ++s) {
            size_t record_length;
            uint32_t sidecar_sequence;
            energun_sc_view_t view;
            if (energun_pair_sidecar_read(adapter, (uint8_t)s,
                    &record_length, &sidecar_sequence) != 0)
                continue;
            (void)sidecar_sequence;
            if (energun_sc_v1_validate(adapter->scratch_record,
                    record_length, adapter->identity_kind,
                    adapter->identity_index, base[b].sequence,
                    base[b].payload_crc32, adapter->base_motion_count,
                    &view) != ENERGUN_SC_OK)
                continue;
            if (best_base < 0 ||
                energun_pair_seq_newer(base[b].sequence, best_sequence)) {
                best_base = (int)b;
                best_sidecar = (int)s;
                best_sequence = base[b].sequence;
            }
        }
    }
    adapter->protected_base_copy = (int8_t)best_base;
    adapter->protected_sidecar_copy = (int8_t)best_sidecar;
    return best_base;
}

static int energun_pair_plan_base(
    void *opaque, uint32_t object, const uint8_t *payload, uint32_t length,
    energun_pair_base_meta_t *target)
{
    energun_pair_storage_adapter_t *adapter =
        (energun_pair_storage_adapter_t *)opaque;
    energun_pair_base_meta_t base[2];
    uint8_t valid[2] = {0u, 0u};
    st_meta_t current;
    unsigned int copy;
    int protected_copy, current_copy = -1;

    if (adapter == 0 || target == 0 || payload == 0 ||
        object != adapter->base_object || length > ST_PAYLOAD_MAX)
        return -1;

    for (copy = 0; copy < 2u; ++copy) {
        st_meta_t meta;
        if (st_read_copy_meta(object, copy, st_buf, ST_PAYLOAD_MAX, &meta) == 0) {
            base[copy].copy = (uint8_t)copy;
            base[copy].sequence = meta.sequence;
            base[copy].payload_length = meta.payload_length;
            base[copy].payload_crc32 = meta.payload_crc;
            valid[copy] = 1u;
        }
    }

    if (st_current_meta(object, &current) == 0) {
        current_copy = (int)current.copy;
        adapter->newest_base_sequence = current.sequence;
        target->sequence = current.sequence + 1u;
    } else {
        adapter->newest_base_sequence = 0u;
        target->sequence = 0u;
    }

    protected_copy = energun_pair_find_protected_pair(adapter, base, valid);
    if (protected_copy >= 0) {
        /* Preserve the last complete pair, even if the other Base copy is a
         * newer orphan whose Sidecar was staged before an interrupted save.
         */
        target->copy = (uint8_t)(protected_copy == 0 ? 1u : 0u);
    } else {
        /* No complete pair exists yet: retain ordinary A/B fallback safety. */
        target->copy = (uint8_t)(current_copy == 0 ? 1u : 0u);
    }
    target->payload_length = length;
    target->payload_crc32 = st_crc32(payload, length);
    return 0;
}

static int energun_pair_select_sidecar_target(
    void *opaque, energun_sc_identity_kind_t kind, uint8_t index,
    const energun_pair_base_meta_t *target_base, uint8_t *target)
{
    energun_pair_storage_adapter_t *adapter =
        (energun_pair_storage_adapter_t *)opaque;
    uint32_t sequence[2] = {0u, 0u};
    uint8_t valid[2] = {0u, 0u};
    unsigned int copy;
    int newest = -1;
    (void)target_base;

    if (adapter == 0 || target == 0 || kind != adapter->identity_kind ||
        index != adapter->identity_index)
        return -1;

    if (adapter->protected_sidecar_copy >= 0) {
        *target = (uint8_t)(adapter->protected_sidecar_copy == 0 ? 1u : 0u);
        adapter->selected_sidecar_copy = *target;
        return 0;
    }

    for (copy = 0; copy < 2u; ++copy) {
        size_t length;
        if (energun_pair_sidecar_read(adapter, (uint8_t)copy,
                &length, &sequence[copy]) == 0 &&
            length <= ENERGUN_SC_V1_MAX_RECORD)
            valid[copy] = 1u;
    }
    if (valid[0]) newest = 0;
    if (valid[1] && (newest < 0 ||
        energun_pair_seq_newer(sequence[1], sequence[0])))
        newest = 1;
    *target = (uint8_t)(newest == 0 ? 1u : 0u);
    adapter->selected_sidecar_copy = *target;
    return 0;
}

static int energun_pair_write_sidecar(
    void *opaque, energun_sc_identity_kind_t kind, uint8_t index,
    uint8_t copy, const uint8_t *record, size_t length)
{
    energun_pair_storage_adapter_t *adapter =
        (energun_pair_storage_adapter_t *)opaque;
    if (adapter == 0 || adapter->sidecar_ops == 0 ||
        adapter->sidecar_ops->write_copy == 0 ||
        kind != adapter->identity_kind || index != adapter->identity_index ||
        copy != adapter->selected_sidecar_copy || copy > 1u ||
        record == 0 || length > ENERGUN_SC_V1_MAX_RECORD)
        return -1;
    return adapter->sidecar_ops->write_copy(adapter->sidecar_context,
        kind, index, copy, record, length);
}

static int energun_pair_write_base(
    void *opaque, uint32_t object, const uint8_t *payload, uint32_t length,
    const energun_pair_base_meta_t *expected)
{
    energun_pair_storage_adapter_t *adapter =
        (energun_pair_storage_adapter_t *)opaque;
    st_meta_t actual;
    if (adapter == 0 || expected == 0 || object != adapter->base_object ||
        expected->copy > 1u || length != expected->payload_length ||
        st_crc32(payload, length) != expected->payload_crc32)
        return -1;
    if (st_save_to(object, payload, length, (int)expected->copy) != 0)
        return -1;
    if (st_read_copy_meta(object, expected->copy, st_buf,
            ST_PAYLOAD_MAX, &actual) != 0)
        return -1;
    return actual.sequence == expected->sequence &&
           actual.payload_length == expected->payload_length &&
           actual.payload_crc == expected->payload_crc32 ? 0 : -1;
}

static int energun_pair_verify_pair(
    void *opaque, uint32_t object, energun_sc_identity_kind_t kind,
    uint8_t index, const energun_pair_base_meta_t *expected,
    const uint8_t *record, size_t record_length, uint16_t motion_count)
{
    energun_pair_storage_adapter_t *adapter =
        (energun_pair_storage_adapter_t *)opaque;
    st_meta_t base;
    size_t actual_length;
    uint32_t sidecar_sequence;
    energun_sc_view_t view;
    uint8_t copy;
    if (adapter == 0 || expected == 0 || record == 0 ||
        object != adapter->base_object || kind != adapter->identity_kind ||
        index != adapter->identity_index)
        return -1;
    copy = adapter->selected_sidecar_copy;
    if (st_read_copy_meta(object, expected->copy, st_buf,
            ST_PAYLOAD_MAX, &base) != 0 ||
        base.sequence != expected->sequence ||
        base.payload_length != expected->payload_length ||
        base.payload_crc != expected->payload_crc32)
        return -1;
    if (energun_pair_sidecar_read(adapter, copy, &actual_length,
            &sidecar_sequence) != 0 ||
        actual_length != record_length ||
        memcmp(adapter->scratch_record, record, record_length) != 0)
        return -1;
    (void)sidecar_sequence;
    return energun_sc_v1_validate(adapter->scratch_record, actual_length,
        kind, index, expected->sequence, expected->payload_crc32,
        motion_count, &view) == ENERGUN_SC_OK ? 0 : -1;
}

static const energun_pair_txn_ops_t energun_pair_storage_ops = {
    energun_pair_plan_base,
    energun_pair_select_sidecar_target,
    energun_pair_write_sidecar,
    energun_pair_write_base,
    energun_pair_verify_pair
};

int energun_pair_storage_adapter_init(
    energun_pair_storage_adapter_t *adapter,
    const energun_sidecar_storage_ops_t *sidecar_ops,
    void *sidecar_context, uint32_t base_object)
{
    if (adapter == 0 || sidecar_ops == 0 ||
        sidecar_ops->read_copy == 0 || sidecar_ops->write_copy == 0 ||
        base_object >= OBJ_COUNT)
        return -1;
    memset(adapter, 0, sizeof *adapter);
    adapter->sidecar_ops = sidecar_ops;
    adapter->sidecar_context = sidecar_context;
    adapter->base_object = base_object;
    adapter->protected_base_copy = -1;
    adapter->protected_sidecar_copy = -1;
    return 0;
}

energun_pair_status_t energun_pair_storage_adapter_save(
    energun_pair_storage_adapter_t *adapter,
    energun_pair_txn_workspace_t *workspace,
    const uint8_t *base_payload, uint32_t base_length,
    energun_sc_identity_kind_t kind, uint8_t index,
    const uint8_t *sidecar_payload, uint16_t sidecar_length,
    uint16_t base_motion_count)
{
    if (adapter == 0 || workspace == 0 || base_payload == 0 ||
        sidecar_payload == 0 || adapter->sidecar_ops == 0)
        return ENERGUN_PAIR_ERR_ARGUMENT;
    adapter->identity_kind = kind;
    adapter->identity_index = index;
    adapter->base_motion_count = base_motion_count;
    adapter->protected_base_copy = -1;
    adapter->protected_sidecar_copy = -1;
    adapter->selected_sidecar_copy = 0u;
    return energun_pair_txn_save(&energun_pair_storage_ops, adapter,
        workspace, adapter->base_object, base_payload, base_length,
        kind, index, sidecar_payload, sidecar_length, base_motion_count);
}

int energun_pair_storage_adapter_load(
    energun_pair_storage_adapter_t *adapter, void *destination,
    uint32_t maximum, void *sidecar_destination, uint16_t sidecar_maximum,
    uint16_t *sidecar_length, energun_sc_identity_kind_t kind,
    uint8_t index, uint16_t base_motion_count,
    energun_pair_base_meta_t *metadata)
{
    energun_pair_base_meta_t base[2];
    uint8_t valid[2] = {0u, 0u};
    int best = -1, best_sidecar = -1;
    uint32_t best_sequence = 0u;
    unsigned int b, s;
    st_meta_t loaded;
    energun_sc_view_t selected_view;
    if (adapter == 0 || destination == 0 || sidecar_destination == 0 ||
        sidecar_length == 0 || metadata == 0 || adapter->sidecar_ops == 0 ||
        kind == 0)
        return -1;
    *sidecar_length = 0u;
    adapter->identity_kind = kind;
    adapter->identity_index = index;
    adapter->base_motion_count = base_motion_count;

    for (b = 0; b < 2u; ++b) {
        st_meta_t m;
        if (st_read_copy_meta(adapter->base_object, b, st_buf,
                ST_PAYLOAD_MAX, &m) == 0) {
            base[b].copy = (uint8_t)b;
            base[b].sequence = m.sequence;
            base[b].payload_length = m.payload_length;
            base[b].payload_crc32 = m.payload_crc;
            valid[b] = 1u;
        }
    }
    for (b = 0; b < 2u; ++b) {
        if (!valid[b])
            continue;
        for (s = 0; s < 2u; ++s) {
            size_t record_length;
            uint32_t sidecar_sequence;
            energun_sc_view_t view;
            if (energun_pair_sidecar_read(adapter, (uint8_t)s,
                    &record_length, &sidecar_sequence) != 0)
                continue;
            (void)sidecar_sequence;
            if (energun_sc_v1_validate(adapter->scratch_record,
                    record_length, kind, index, base[b].sequence,
                    base[b].payload_crc32, base_motion_count,
                    &view) != ENERGUN_SC_OK)
                continue;
            if (best < 0 || energun_pair_seq_newer(
                    base[b].sequence, best_sequence)) {
                best = (int)b;
                best_sidecar = (int)s;
                best_sequence = base[b].sequence;
            }
        }
    }
    if (best < 0 || best_sidecar < 0 ||
        base[best].payload_length > maximum)
        return -1;

    /* Re-read and validate the selected Sidecar before exposing either half. */
    {
        size_t record_length;
        uint32_t sidecar_sequence;
        if (energun_pair_sidecar_read(adapter, (uint8_t)best_sidecar,
                &record_length, &sidecar_sequence) != 0 ||
            energun_sc_v1_validate(adapter->scratch_record, record_length,
                kind, index, base[best].sequence, base[best].payload_crc32,
                base_motion_count, &selected_view) != ENERGUN_SC_OK ||
            selected_view.payload_length > sidecar_maximum)
            return -1;
        memcpy(sidecar_destination, selected_view.payload,
               selected_view.payload_length);
        *sidecar_length = selected_view.payload_length;
    }

    if (st_read_copy_meta(adapter->base_object, (uint32_t)best,
            destination, maximum, &loaded) != 0 ||
        loaded.sequence != base[best].sequence ||
        loaded.payload_crc != base[best].payload_crc32)
        return -1;
    metadata->copy = (uint8_t)best;
    metadata->sequence = loaded.sequence;
    metadata->payload_length = loaded.payload_length;
    metadata->payload_crc32 = loaded.payload_crc;
    adapter->protected_base_copy = (int8_t)best;
    adapter->protected_sidecar_copy = (int8_t)best_sidecar;
    return (int)loaded.payload_length;
}
