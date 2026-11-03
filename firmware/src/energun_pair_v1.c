/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — backend-neutral Base + Sidecar V1 pair coordinator.
 */
#include "energun_pair_v1.h"

static void pair_copy(uint8_t *destination, const uint8_t *source, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i)
        destination[i] = source[i];
}

static int pair_equal(const uint8_t *a, const uint8_t *b, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static uint32_t pair_crc32(const uint8_t *data, size_t length)
{
    static const uint32_t table[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
    };
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        crc = (crc >> 4) ^ table[crc & 15u];
        crc = (crc >> 4) ^ table[crc & 15u];
    }
    return ~crc;
}

static int pair_ops_valid(const energun_pair_v1_ops_t *ops)
{
    return ops != NULL && ops->base_current != NULL &&
           ops->base_load_current != NULL && ops->base_write_copy != NULL &&
           ops->sidecar_read_copy != NULL && ops->sidecar_write_copy != NULL;
}

static energun_pair_v1_status_t pair_read_candidates(
    const energun_pair_v1_ops_t *ops, void *context,
    energun_pair_v1_workspace_t *workspace,
    energun_sc_identity_kind_t kind, uint8_t index)
{
    unsigned copy;
    for (copy = 0; copy < 2u; ++copy) {
        size_t length = 0u;
        uint32_t sequence = 0u;
        int rc = ops->sidecar_read_copy(context, kind, index, (uint8_t)copy,
            workspace->records[copy], sizeof workspace->records[copy],
            &length, &sequence);
        workspace->candidates[copy].record = NULL;
        workspace->candidates[copy].record_length = 0u;
        workspace->candidates[copy].storage_sequence = 0u;
        workspace->candidates[copy].storage_valid = 0u;
        if (rc < 0)
            return ENERGUN_PAIR_V1_ERR_SIDECAR_IO;
        if (rc == 0) {
            if (length > sizeof workspace->records[copy])
                return ENERGUN_PAIR_V1_ERR_SIDECAR_IO;
            workspace->candidates[copy].record = workspace->records[copy];
            workspace->candidates[copy].record_length = length;
            workspace->candidates[copy].storage_sequence = sequence;
            workspace->candidates[copy].storage_valid = 1u;
        } else if (rc != 1) {
            return ENERGUN_PAIR_V1_ERR_SIDECAR_IO;
        }
    }
    return ENERGUN_PAIR_V1_OK;
}

energun_pair_v1_status_t energun_pair_v1_commit(
    const energun_pair_v1_ops_t *ops, void *context,
    energun_pair_v1_workspace_t *workspace,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    const uint8_t *base_payload, size_t base_length,
    const uint8_t *sidecar_payload, uint16_t sidecar_length,
    uint16_t base_motion_count)
{
    energun_pair_v1_base_meta_t current;
    energun_pair_v1_base_meta_t committed;
    energun_sc_ab_selection_t selection;
    energun_sc_view_t view;
    size_t encoded_length = 0u, verify_length = 0u;
    uint32_t verify_sequence = 0u, next_base_sequence, next_base_crc;
    uint8_t base_target;
    int current_rc, rc;

    if (!pair_ops_valid(ops) || workspace == NULL || base_payload == NULL ||
        base_length == 0u || sidecar_payload == NULL)
        return ENERGUN_PAIR_V1_ERR_ARGUMENT;

    current_rc = ops->base_current(context, object, &current);
    if (current_rc < 0)
        return ENERGUN_PAIR_V1_ERR_BASE_IO;
    if (current_rc == 1) {
        current.copy = 0u;
        current.sequence = 0u;
        current.payload_length = 0u;
        current.payload_crc32 = 0u;
        base_target = 0u;
    } else if (current_rc == 0) {
        if (current.copy > 1u)
            return ENERGUN_PAIR_V1_ERR_BASE_IO;
        base_target = (uint8_t)(1u - current.copy);
    } else {
        return ENERGUN_PAIR_V1_ERR_BASE_IO;
    }

    next_base_sequence = current.sequence + 1u;
    next_base_crc = pair_crc32(base_payload, base_length);

    if (pair_read_candidates(ops, context, workspace, kind, index) != ENERGUN_PAIR_V1_OK)
        return ENERGUN_PAIR_V1_ERR_SIDECAR_IO;
    if (energun_sc_v1_select_ab(workspace->candidates, kind, index,
            current.sequence, current.payload_crc32, base_motion_count,
            &selection) != ENERGUN_SC_OK)
        return ENERGUN_PAIR_V1_ERR_SIDECAR_FORMAT;

    if (energun_sc_v1_encode(workspace->verify_record,
            sizeof workspace->verify_record, &encoded_length,
            kind, index, next_base_sequence, next_base_crc,
            sidecar_payload, sidecar_length, base_motion_count) != ENERGUN_SC_OK)
        return ENERGUN_PAIR_V1_ERR_SIDECAR_FORMAT;

    rc = ops->sidecar_write_copy(context, kind, index,
        (uint8_t)selection.write_copy, workspace->verify_record,
        encoded_length, selection.next_storage_sequence);
    if (rc != 0)
        return ENERGUN_PAIR_V1_ERR_SIDECAR_IO;

    /* Verify the future-bound record before making its Base generation live. */
    rc = ops->sidecar_read_copy(context, kind, index,
        (uint8_t)selection.write_copy, workspace->records[0],
        sizeof workspace->records[0], &verify_length, &verify_sequence);
    if (rc != 0 || verify_length > sizeof workspace->records[0] ||
        verify_sequence != selection.next_storage_sequence ||
        verify_length != encoded_length ||
        !pair_equal(workspace->records[0], workspace->verify_record, encoded_length) ||
        energun_sc_v1_validate(workspace->records[0], verify_length,
            kind, index, next_base_sequence, next_base_crc,
            base_motion_count, &view) != ENERGUN_SC_OK)
        return ENERGUN_PAIR_V1_ERR_SIDECAR_IO;

    /* Sidecar is now durable but orphaned until the Base commit succeeds. */
    rc = ops->base_write_copy(context, object, base_target,
                              base_payload, base_length);
    if (rc != 0)
        return ENERGUN_PAIR_V1_ERR_BASE_COMMIT;

    rc = ops->base_current(context, object, &committed);
    if (rc != 0 || committed.copy != base_target ||
        committed.sequence != next_base_sequence ||
        committed.payload_length != base_length ||
        committed.payload_crc32 != next_base_crc)
        return ENERGUN_PAIR_V1_ERR_BASE_VERIFY;

    return ENERGUN_PAIR_V1_OK;
}

energun_pair_v1_status_t energun_pair_v1_erase(
    const energun_pair_v1_ops_t *ops, void *context,
    uint32_t object)
{
    static const uint8_t empty_payload = 0u;
    energun_pair_v1_base_meta_t current, committed;
    uint8_t target;
    uint32_t next_sequence;
    int rc;

    if (ops == NULL || ops->base_current == NULL || ops->base_write_copy == NULL)
        return ENERGUN_PAIR_V1_ERR_ARGUMENT;

    rc = ops->base_current(context, object, &current);
    if (rc < 0)
        return ENERGUN_PAIR_V1_ERR_BASE_IO;
    if (rc == 1) {
        current.copy = 1u;
        current.sequence = 0u;
        target = 0u;
    } else if (rc == 0 && current.copy <= 1u) {
        target = (uint8_t)(1u - current.copy);
    } else {
        return ENERGUN_PAIR_V1_ERR_BASE_IO;
    }

    next_sequence = current.sequence + 1u;
    rc = ops->base_write_copy(context, object, target, &empty_payload, 0u);
    if (rc != 0)
        return ENERGUN_PAIR_V1_ERR_BASE_COMMIT;

    rc = ops->base_current(context, object, &committed);
    if (rc != 0 || committed.copy != target ||
        committed.sequence != next_sequence ||
        committed.payload_length != 0u || committed.payload_crc32 != 0u)
        return ENERGUN_PAIR_V1_ERR_BASE_VERIFY;

    return ENERGUN_PAIR_V1_OK;
}

energun_pair_v1_status_t energun_pair_v1_load(
    const energun_pair_v1_ops_t *ops, void *context,
    energun_pair_v1_workspace_t *workspace,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    uint8_t *base_payload, size_t base_capacity,
    size_t *base_length,
    uint8_t *sidecar_payload, size_t sidecar_capacity,
    size_t *sidecar_length,
    uint16_t base_motion_count,
    energun_pair_v1_base_meta_t *base_meta)
{
    energun_pair_v1_base_meta_t selected, loaded;
    energun_sc_ab_selection_t selection;
    energun_sc_view_t view;
    size_t loaded_length = 0u;
    int rc;

    if (base_length != NULL) *base_length = 0u;
    if (sidecar_length != NULL) *sidecar_length = 0u;
    if (!pair_ops_valid(ops) || workspace == NULL || base_payload == NULL ||
        base_length == NULL || sidecar_payload == NULL || sidecar_length == NULL ||
        base_meta == NULL)
        return ENERGUN_PAIR_V1_ERR_ARGUMENT;

    rc = ops->base_current(context, object, &selected);
    if (rc != 0)
        return rc == 1 ? ENERGUN_PAIR_V1_ERR_NO_MATCH : ENERGUN_PAIR_V1_ERR_BASE_IO;
    if (selected.copy > 1u)
        return ENERGUN_PAIR_V1_ERR_BASE_IO;

    if (pair_read_candidates(ops, context, workspace, kind, index) != ENERGUN_PAIR_V1_OK)
        return ENERGUN_PAIR_V1_ERR_SIDECAR_IO;
    if (energun_sc_v1_select_ab(workspace->candidates, kind, index,
            selected.sequence, selected.payload_crc32, base_motion_count,
            &selection) != ENERGUN_SC_OK)
        return ENERGUN_PAIR_V1_ERR_SIDECAR_FORMAT;
    if (selection.matching_copy < 0)
        return ENERGUN_PAIR_V1_ERR_NO_MATCH;

    rc = energun_sc_v1_validate(
        workspace->candidates[(unsigned)selection.matching_copy].record,
        workspace->candidates[(unsigned)selection.matching_copy].record_length,
        kind, index, selected.sequence, selected.payload_crc32,
        base_motion_count, &view);
    if (rc != ENERGUN_SC_OK)
        return ENERGUN_PAIR_V1_ERR_SIDECAR_FORMAT;
    if (view.payload_length > sidecar_capacity ||
        selected.payload_length > base_capacity)
        return ENERGUN_PAIR_V1_ERR_CAPACITY;

    rc = ops->base_load_current(context, object, base_payload,
                                base_capacity, &loaded);
    if (rc != 0 || loaded.copy != selected.copy ||
        loaded.sequence != selected.sequence ||
        loaded.payload_length != selected.payload_length ||
        loaded.payload_crc32 != selected.payload_crc32)
        return ENERGUN_PAIR_V1_ERR_BASE_VERIFY;

    loaded_length = view.payload_length;
    pair_copy(sidecar_payload, view.payload, loaded_length);
    *base_length = loaded.payload_length;
    *sidecar_length = loaded_length;
    *base_meta = loaded;
    return ENERGUN_PAIR_V1_OK;
}
